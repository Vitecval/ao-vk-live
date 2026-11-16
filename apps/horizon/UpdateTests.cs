using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace Horizon {
 public static class UpdateTests {
  sealed class Transport:HttpMessageHandler {
   public Func<HttpRequestMessage,HttpResponseMessage> Reply;public int Requests;
   protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage message,CancellationToken token){Requests++;return Task.FromResult(Reply(message));}
  }
  static readonly List<string> Checks=new List<string>();
  static void Check(bool condition,string label){if(!condition)throw new Exception(label);Checks.Add(label);}
  static void Refused(Action action,string label){try{action();}catch(IOException){Checks.Add(label);return;}throw new Exception(label);}
  static async Task Refused(Func<Task> action,string label){try{await action();}catch(IOException){Checks.Add(label);return;}throw new Exception(label);}
  static string Sha(byte[] b){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(b)).Replace("-","").ToLowerInvariant();}
  static HttpResponseMessage ReportStatus(){return new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent("{\"enabled\":true,\"schema\":1,\"encoding\":\"gzip-base64\"}")};}
  static Dictionary<string,object> Release(string tag,bool draft=false,bool preview=false){return new Dictionary<string,object>{{"tag_name",tag},{"draft",draft},{"prerelease",preview},{"name","Horizon "+tag},{"assets",new object[]{new Dictionary<string,object>{{"name",UpdateService.DownloadName},{"state","uploaded"},{"size",4096},{"digest","sha256:"+new string('a',64)},{"browser_download_url",UpdateService.Repository+"/releases/download/"+tag+"/"+UpdateService.DownloadName}}}}};}
  public static async Task Run(string root){
   var json=new JavaScriptSerializer{MaxJsonLength=32000000};var cancel=CancellationToken.None;
   Check(UpdateService.Compare("v0.7.0","v0.7.0-preview.9")>0&&UpdateService.Compare("v0.7.10","v0.7.9")>0,"Semantic release comparison handles stable versions and multi-digit updates");
   var releases=new[]{Release("v0.7.0"),Release("v0.8.0",false,true),Release("v0.7.1",true),Release("v0.7.2")};
   Check(UpdateService.Select(json.Serialize(releases)).Tag=="v0.7.2","Checks choose a complete stable release and ignore previews/drafts");
   var malicious=Release("v0.7.3");var asset=(Dictionary<string,object>)((object[])malicious["assets"])[0];asset["browser_download_url"]="https://example.invalid/installer.exe";
   Refused(()=>UpdateService.Select(json.Serialize(new[]{malicious})),"Release metadata cannot redirect the initial installer URL outside the pinned repository");
   asset["browser_download_url"]=UpdateService.Repository+"/releases/download/v0.7.3/"+UpdateService.DownloadName;asset["digest"]="";
   Refused(()=>UpdateService.Select(json.Serialize(new[]{malicious})),"An installer without a published SHA-256 is refused");
   var redirect=new Transport{Reply=r=>{var response=new HttpResponseMessage(HttpStatusCode.Redirect);response.Headers.Location=new Uri("http://example.invalid/update");return response;}};
   using(var client=new HttpClient(redirect)){await Refused(async()=>{using(var response=await UpdateService.Get(client,UpdateService.Repository,cancel)){}},"HTTPS downgrade and external download redirects are refused");}Check(redirect.Requests==1,"Unsafe redirect is refused before contacting its target");
   var limited=new Transport{Reply=r=>new HttpResponseMessage(HttpStatusCode.Forbidden)};
   using(var client=new HttpClient(limited)){await Refused(async()=>{using(var response=await UpdateService.Get(client,UpdateService.Repository,cancel)){}},"GitHub rate limits are handled without installation");}
   await Refused(async()=>{await UpdateService.ReadMetadata(new MemoryStream(new byte[4000001]),cancel);},"Oversized release metadata is bounded");
   byte[] bytes=Encoding.UTF8.GetBytes(new string('x',10000));var pin=new HorizonRelease{Bytes=bytes.Length,Sha256=Sha(bytes)};
   string good=Path.Combine(root,"verified-update.tmp");await UpdateService.CopyVerified(new MemoryStream(bytes),good,pin,(d,t)=>{},cancel);Check(SetupService.Hash(good)==pin.Sha256,"Downloaded bytes are verified against size and SHA-256 before use");
   await Refused(()=>UpdateService.CopyVerified(new MemoryStream(bytes.Take(9999).ToArray()),Path.Combine(root,"short.partial"),pin,(d,t)=>{},cancel),"Truncated update streams are refused");
   byte[] bad=(byte[])bytes.Clone();bad[10]^=255;await Refused(()=>UpdateService.CopyVerified(new MemoryStream(bad),Path.Combine(root,"bad.partial"),pin,(d,t)=>{},cancel),"Corrupted update streams are refused");
   await Refused(()=>UpdateService.CopyVerified(new MemoryStream(bytes.Concat(new byte[]{0}).ToArray()),Path.Combine(root,"long.partial"),pin,(d,t)=>{},cancel),"Oversized update streams are refused");
   Refused(()=>UpdateService.ValidateBundle(good),"A non-executable response cannot be applied as an update");UpdateService.ValidateBundle(SetupService.ExePath);Checks.Add("The packaged installer has a complete update payload");
   string clientDir=Path.Combine(root,"AO private fixture");Directory.CreateDirectory(clientDir);File.WriteAllText(Path.Combine(clientDir,"AnarchyOnline.exe"),"dummy never executed");File.WriteAllText(Path.Combine(clientDir,"randy31.dll"),"dummy");File.WriteAllText(Path.Combine(clientDir,"personal.txt"),"keep personal");
   string updates=Path.Combine(root,"updates","test");Directory.CreateDirectory(updates);string download=Path.Combine(updates,UpdateService.DownloadName);File.Copy(SetupService.ExePath,download);
   var current=new HorizonRelease{Tag=UpdateService.VersionTag,Sha256=SetupService.Hash(download),Bytes=new FileInfo(download).Length};
   string ticket=UpdateService.PrepareHandoff(download,current,clientDir,"lite",true);var t=UpdateService.ReadHandoff(ticket);Check((string)t["client"]==clientDir&&(bool)t["skipMemoryChange"],"Update handoff preserves the selected client, edition and memory preference");
   t["createdUtc"]=DateTime.UtcNow.AddHours(-1).Ticks;File.WriteAllText(ticket,json.Serialize(t));Refused(()=>UpdateService.ReadHandoff(ticket),"An expired update handoff is refused before writes");
   t["createdUtc"]=DateTime.UtcNow.Ticks;t["sha256"]=new string('b',64);File.WriteAllText(ticket,json.Serialize(t));Refused(()=>UpdateService.ReadHandoff(ticket),"A changed update executable is refused before writes");
   using(var p=System.Diagnostics.Process.GetCurrentProcess()){t["parentPid"]=p.Id;t["parentStartUtc"]=0L;Refused(()=>UpdateService.WaitForParent(t),"PID reuse cannot cause the updater to wait for an unrelated process");}
   t["parentPid"]=int.MaxValue;UpdateService.WaitForParent(t);Checks.Add("An already exited parent does not block the handoff");
   string appDir=Path.Combine(root,"Horizon update fixture"),shortcut=Path.Combine(root,"Updated Horizon.lnk");string installed=SetupService.InstallAppAt(appDir,shortcut,false);
   using(var f=new FileStream(installed,FileMode.Append,FileAccess.Write))f.WriteByte(42);string previousHash=SetupService.Hash(installed);string ownership=Path.Combine(appDir,"horizon-install.json");var record=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(ownership));record["sha256"]=previousHash;File.WriteAllText(ownership,json.Serialize(record));File.WriteAllText(Path.Combine(appDir,"personal-note.txt"),"preserve");
   SetupService.InstallAppAt(appDir,shortcut,false);record=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(ownership));
   Check(SetupService.Hash(installed)==SetupService.Hash(SetupService.ExePath)&&SetupService.Hash(Path.Combine(appDir,(string)record["previousFile"]))==previousHash&&File.ReadAllText(Path.Combine(appDir,"personal-note.txt"))=="preserve","App replacement verifies the new EXE, retains the prior owned app and preserves unrelated files");
   // A matching receipt skips a full texture scan during app-only updates.
   var lite=json.Deserialize<Dictionary<string,object>>(App.ResourceText("lite-manifest.json"));var addon=(Dictionary<string,object>)lite["addon"];
   var coreIndex=json.Deserialize<Dictionary<string,string>>(App.ResourceText("core-files.json"));Check(coreIndex.ContainsKey("ao-vk.dll"),"The update keeps the renderer's pinned bootstrap integrity index");
   string core=await Task.Run(()=>SetupService.EnsureCore((m,p)=>{}));foreach(var name in new[]{"ao-vk.dll","ddraw.dll"})File.Copy(Path.Combine(core,name),Path.Combine(clientDir,name),true);
   var full=json.Deserialize<Dictionary<string,object>>(App.ResourceText("full-manifest.json"));string receipt=Path.Combine(clientDir,"ao-vk-live.install.json");File.WriteAllText(receipt,json.Serialize(full));
   Check(UpdateService.GraphicsCurrent(clientDir,"full"),"A full-sized matching receipt skips texture verification during app-only updates");
   Check(!UpdateService.GraphicsCurrent(clientDir,"lite"),"Changing edition still requests a graphics update");
   File.WriteAllText(Path.Combine(clientDir,"ao-vk.dll"),"changed renderer");Check(!UpdateService.GraphicsCurrent(clientDir,"full"),"A changed renderer cannot be treated as already current");File.Copy(Path.Combine(core,"ao-vk.dll"),Path.Combine(clientDir,"ao-vk.dll"),true);
   string oldLog=Path.Combine(clientDir,"randy-vk.log"),latestLog=Path.Combine(clientDir,"randy-vk.old.log");File.WriteAllText(oldLog,"OLD_UNSELECTED_LOG");File.SetLastWriteTimeUtc(oldLog,DateTime.UtcNow.AddHours(-2));File.WriteAllText(latestLog,"GPU timing 22 ms\npassword=do-not-send\nuser@example.test\n"+clientDir+"\\file\nLATEST_LOG_MARKER");File.SetLastWriteTimeUtc(latestLog,DateTime.UtcNow);
   File.WriteAllText(Path.Combine(clientDir,"chat.log"),"PRIVATE_CHAT_MUST_NOT_BE_READ");string oldHash=SetupService.Hash(latestLog);
   var report=IssueReports.Prepare(clientDir,"Performance issue","Looking up flickers",true);
   Check(report.Body.Contains("LATEST_LOG_MARKER")&&!report.Body.Contains("OLD_UNSELECTED_LOG")&&!report.Body.Contains("PRIVATE_CHAT_MUST_NOT_BE_READ"),"Reports include only the newest whitelisted renderer log from the selected installation");
   Check(!report.Body.Contains("do-not-send")&&!report.Body.Contains("user@example.test")&&!report.Body.Contains(clientDir),"Common secrets, email addresses and selected-folder paths are removed from reports");
   using(var zip=ZipFile.OpenRead(report.Archive))using(var reader=new StreamReader(zip.GetEntry("Report.txt").Open()))Check(reader.ReadToEnd()==report.Body,"The saved ZIP contains exactly the reviewed report");
   Check(SetupService.Hash(latestLog)==oldHash&&File.ReadAllText(Path.Combine(clientDir,"personal.txt"))=="keep personal","Preparing a report preserves original logs and personal files");
   var without=IssueReports.Prepare(clientDir,"No logs","",false);Check(!without.Body.Contains("LATEST_LOG_MARKER"),"Players can prepare a report without including logs");
   var accepted=new Transport{Reply=r=>{if(r.Method==HttpMethod.Get)return ReportStatus();Check(r.Method==HttpMethod.Post&&r.RequestUri.AbsoluteUri==IssueReports.Endpoint&&!r.Headers.Contains("Authorization"),"Reports use the fixed HTTPS endpoint without an embedded account token");var posted=json.Deserialize<Dictionary<string,object>>(r.Content.ReadAsStringAsync().GetAwaiter().GetResult());using(var raw=new MemoryStream(Convert.FromBase64String((string)posted["report"])))using(var gzip=new GZipStream(raw,CompressionMode.Decompress))using(var reader=new StreamReader(gzip))Check(reader.ReadToEnd()==report.Body,"Compression sends exactly the reviewed full report");return new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent(json.Serialize(new{accepted=true,id=report.Id}))};}};
   using(var http=new HttpClient(accepted)){Check((await IssueReports.Send(report,http,cancel)).Contains("accepted"),"Only a matching service acknowledgement marks the report accepted");}
   var wrong=new Transport{Reply=r=>r.Method==HttpMethod.Get?ReportStatus():new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent("{\"accepted\":true,\"id\":\"wrong\"}")}};
   using(var http=new HttpClient(wrong)){await Refused(async()=>{await IssueReports.Send(report,http,cancel);},"An unrelated acknowledgement cannot mark a report sent");}
   foreach(var status in new[]{HttpStatusCode.NotFound,(HttpStatusCode)429,HttpStatusCode.ServiceUnavailable}){var offline=new Transport{Reply=r=>new HttpResponseMessage(status)};using(var http=new HttpClient(offline)){await Refused(async()=>{await IssueReports.Send(report,http,cancel);},"Unavailable or limited reporting keeps the ZIP available: "+(int)status);}}
   var disabled=new Transport{Reply=r=>new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent("{\"enabled\":false}")}};using(var http=new HttpClient(disabled)){await Refused(async()=>{await IssueReports.Send(report,http,cancel);},"Disabled WordPress reporting is detected before uploading a log");}Check(disabled.Requests==1,"The readiness check sends no report body to a disabled receiver");
   File.WriteAllText(Path.Combine(root,"UPDATE-TEST-RESULTS.json"),json.Serialize(new{result="passed",checks=Checks,networkContacted=false,emailSent=false,retailClientsChanged=false}));
  }
 }
}
