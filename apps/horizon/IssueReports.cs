using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace Horizon {
 public sealed class IssueReport {public string Id,Title,RawTitle,Description,Body,Archive,Directory;public bool IncludesLogs;}
 public static class IssueReports {
  public const string Endpoint="https://asyncstuff.com/wp-json/rubika-horizon/v1/reports";
  internal static string Scrub(string text,string client){
   foreach(var path in new[]{App.StateRoot,client,Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)}.Where(p=>!string.IsNullOrEmpty(p)).OrderByDescending(p=>p.Length))text=Regex.Replace(text,Regex.Escape(path),"[local folder]",RegexOptions.IgnoreCase);
   text=Regex.Replace(text,@"(?im)^.*(?:\[chat\]|\b(?:password|passwd|authorization|cookie|token|access_token|refresh_token|api[_-]?key|secret|account|accountname|loginname|username|playername|charactername)\b|(?:To|From) \[).*$","[personal information removed]");
   text=Regex.Replace(text,@"(?i)[a-z]:[\\/]users[\\/][^\\/\s""<>]+","[user folder]");
   text=Regex.Replace(text,@"https?://[^\s<>""']+","[URL removed]",RegexOptions.IgnoreCase);
   text=Regex.Replace(text,@"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}","[email removed]");
   return text.Replace("\0","");
  }
  internal static string Excerpt(string path,int limit){
   if((File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)throw new IOException("A linked log was excluded.");SetupService.CheckDirectorySafety(Path.GetDirectoryName(path));
   using(var f=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
    int head=(int)Math.Min(f.Length,16384),tail=(int)Math.Min(Math.Max(0,f.Length-head),limit-head);var b=new byte[head];int n=f.Read(b,0,b.Length);string first=Encoding.UTF8.GetString(b,0,n);if(f.Length<=head)return first;
    bool gap=f.Length>head+tail;if(gap)f.Seek(-tail,SeekOrigin.End);b=new byte[tail];n=f.Read(b,0,b.Length);return first+(gap?"\n[... middle of long log omitted ...]\n":"")+Encoding.UTF8.GetString(b,0,n);
   }
  }
  public static IssueReport Prepare(string client,string title,string description,bool logs){
   var files=new Dictionary<string,string>();var notices=new List<string>();
   if(logs){
    if(!string.IsNullOrEmpty(client)&&Directory.Exists(client)){
     SetupService.CheckDirectorySafety(client);
     var latest=new DirectoryInfo(client).EnumerateFiles("randy-vk*.log").Where(f=>Regex.IsMatch(f.Name,@"^randy-vk(?:\.old|-\d+)?\.log$",RegexOptions.IgnoreCase)&&!f.Attributes.HasFlag(FileAttributes.ReparsePoint)).OrderByDescending(f=>f.LastWriteTimeUtc).FirstOrDefault();
     if(latest!=null)files["renderer.log"]=latest.FullName;else notices.Add("No renderer log was found in the selected AO folder.");
    }
    foreach(var pair in new[]{new[]{"last-setup.log","horizon-setup.log"},new[]{"app-errors.log","horizon-errors.log"},new[]{"startup.log","horizon-startup.log"}}){string path=Path.Combine(App.StateRoot,pair[0]);if(File.Exists(path))files[pair[1]]=path;}
   }
   string id=Guid.NewGuid().ToString("N");var text=new StringBuilder();text.AppendLine("Report: "+id).AppendLine("Horizon: "+SetupService.Version).AppendLine("Windows: "+Environment.OSVersion.Version+"; "+(Environment.Is64BitOperatingSystem?"64-bit":"32-bit")).AppendLine().AppendLine("What happened:").AppendLine(Scrub(string.IsNullOrWhiteSpace(description)?"Latest diagnostics submitted from Horizon.":description.Trim(),client));
   foreach(var notice in notices)text.AppendLine(notice);
   var excerpts=new Dictionary<string,string>();foreach(var file in files){try{excerpts[file.Key]="Log modified (UTC): "+File.GetLastWriteTimeUtc(file.Value).ToString("o")+"\n"+Scrub(Excerpt(file.Value,file.Key=="renderer.log"?3*1024*1024:131072),client);}catch(Exception ex)when(ex is IOException||ex is UnauthorizedAccessException){notices.Add(file.Key+": could not read this log.");}}
   foreach(var item in excerpts){text.AppendLine().AppendLine("--- "+item.Key+" ---").AppendLine(item.Value);}
   foreach(var notice in notices)if(!text.ToString().Contains(notice))text.AppendLine(notice);
   string root=Path.Combine(App.StateRoot,"reports");SetupService.CheckDirectorySafety(root);Directory.CreateDirectory(root);string directory=SetupService.GuardUnder(root,Path.Combine(root,DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)));Directory.CreateDirectory(directory);
   string body=text.ToString(),reportPath=Path.Combine(directory,"Report.txt");File.WriteAllText(reportPath,body,new UTF8Encoding(false));string zip=Path.Combine(directory,"Horizon-report.zip");
   using(var archive=ZipFile.Open(zip,ZipArchiveMode.Create)){
    var entry=archive.CreateEntry("Report.txt");using(var writer=new StreamWriter(entry.Open(),new UTF8Encoding(false)))writer.Write(body);
   }
   return new IssueReport{Id=id,Title=Scrub(string.IsNullOrWhiteSpace(title)?"Horizon issue":title.Trim(),client),RawTitle=title,Description=description,Body=body,Archive=zip,Directory=directory,IncludesLogs=logs};
  }
  internal static async Task<string> Send(IssueReport report,HttpClient client,CancellationToken cancel){
   var json=new JavaScriptSerializer{MaxJsonLength=1200000};byte[] raw=Encoding.UTF8.GetBytes(report.Body);if(raw.Length>4194304)throw new IOException("This report exceeds the service's 4 MiB limit. Use the saved ZIP instead.");
   string compressed;using(var output=new MemoryStream()){using(var gzip=new GZipStream(output,CompressionLevel.Optimal,true))gzip.Write(raw,0,raw.Length);if(output.Length>700000)throw new IOException("This log does not compress enough for the reporting service. Use the saved ZIP instead.");compressed=Convert.ToBase64String(output.ToArray());}
   string body=json.Serialize(new{schema=1,id=report.Id,version=SetupService.Version,title=report.Title,encoding="gzip-base64",bytes=raw.Length,report=compressed});
   if(Encoding.UTF8.GetByteCount(body)>1048576)throw new IOException("This report is too large to send. Use the saved ZIP instead.");
   using(var request=new HttpRequestMessage(HttpMethod.Get,Endpoint+"/status"))using(var status=await client.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,cancel)){
    if(!status.IsSuccessStatusCode)throw new IOException("The reporting service is not available. Your report ZIP is saved.");
    var state=await ReadReply(status,cancel);if(!state.ContainsKey("enabled")||!(state["enabled"] is bool)||!(bool)state["enabled"])throw new IOException("The site owner has not enabled reporting yet. Your report ZIP is saved.");
    if(!state.ContainsKey("schema")||Convert.ToInt32(state["schema"])!=1||!state.ContainsKey("encoding")||(state["encoding"] as string)!="gzip-base64")throw new IOException("The WordPress receiver needs Horizon Reports 1.0.1 or newer for full logs. Replace the older plugin; your report ZIP is saved.");
   }
   using(var request=new HttpRequestMessage(HttpMethod.Post,Endpoint){Content=new StringContent(body,Encoding.UTF8,"application/json")})using(var response=await client.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,cancel)){
    int status=(int)response.StatusCode;if(status==404||status==503)throw new IOException("The reporting service is not available. Your report ZIP is saved and can be emailed to dev@asyncstuff.com.");
    if(status==429)throw new IOException("The reporting service is busy or has reached its report limit. Try later; your ZIP is saved.");
    if(!response.IsSuccessStatusCode)throw new IOException("The reporting service could not accept this report (HTTP "+status+"). Your report ZIP is saved.");
    var reply=await ReadReply(response,cancel);if(!reply.ContainsKey("accepted")||!(reply["accepted"] is bool)||!(bool)reply["accepted"]||!reply.ContainsKey("id")||(string)reply["id"]!=report.Id)throw new IOException("The service did not confirm this report. The saved ZIP is available for retry.");
    return "Report accepted for email to dev@asyncstuff.com. Thank you.";
   }
  }
  static async Task<Dictionary<string,object>> ReadReply(HttpResponseMessage response,CancellationToken cancel){using(var stream=await response.Content.ReadAsStreamAsync()){
   var data=new byte[16385];int total=0,n;while((n=await stream.ReadAsync(data,total,data.Length-total,cancel))>0){total+=n;if(total==data.Length)throw new IOException("Unexpected reporting-service response.");}
   try{var reply=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(Encoding.UTF8.GetString(data,0,total));if(reply==null)throw new ArgumentException();return reply;}catch(ArgumentException){throw new IOException("The reporting service returned an unexpected response. The saved ZIP is available.");}
  }}
  public static async Task<string> Send(IssueReport report,CancellationToken cancel){using(var linked=CancellationTokenSource.CreateLinkedTokenSource(cancel)){linked.CancelAfter(TimeSpan.FromSeconds(30));using(var client=new HttpClient(new HttpClientHandler{AllowAutoRedirect=false}){Timeout=Timeout.InfiniteTimeSpan}){client.DefaultRequestHeaders.UserAgent.ParseAdd("RubiKaHorizon/0.7");return await Send(report,client,linked.Token);}}}
 }
}
