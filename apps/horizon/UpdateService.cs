using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace Horizon {
 public sealed class HorizonRelease {
  public string Tag,Name,Url,DownloadUrl,Sha256;public long Bytes;
 }
 public static class UpdateService {
  public const string VersionTag="v0.7.6";
  public const string Repository="https://github.com/Vitecval/ao-vk-live";
  public const string DownloadName="Rubi-Ka-Horizon-Setup.exe";
  public const string PermanentDownload=Repository+"/releases/latest/download/"+DownloadName;
  const string Api="https://api.github.com/repos/Vitecval/ao-vk-live/releases/latest";
  static JavaScriptSerializer Json(){return new JavaScriptSerializer{MaxJsonLength=4000000};}
  internal static bool TryVersion(string tag,out Version version,out int preview){
   version=null;preview=0;var m=Regex.Match(tag??"",@"^v?(\d+)\.(\d+)\.(\d+)(?:-preview\.(\d+))?$");if(!m.Success)return false;
   if(!Version.TryParse(m.Groups[1]+"."+m.Groups[2]+"."+m.Groups[3],out version))return false;
   if(m.Groups[4].Success){if(!int.TryParse(m.Groups[4].Value,out preview))return false;}else preview=int.MaxValue;
   return version>=new Version(0,7,0);
  }
  internal static int Compare(string a,string b){Version av,bv;int ap,bp;if(!TryVersion(a,out av,out ap)||!TryVersion(b,out bv,out bp))throw new IOException("Unsupported Horizon release version.");int c=av.CompareTo(bv);return c!=0?c:ap.CompareTo(bp);}
  internal static HorizonRelease Select(string json){
   var rows=Json().DeserializeObject(json) as object[];if(rows==null)throw new IOException("GitHub returned an unexpected release list.");HorizonRelease best=null;
   foreach(var row in rows){var r=row as Dictionary<string,object>;if(r==null||!r.ContainsKey("tag_name")||!r.ContainsKey("draft")||!(r["draft"] is bool)||(bool)r["draft"])continue;
    string tag=r["tag_name"] as string;Version v;int preview;if(!TryVersion(tag,out v,out preview))continue;
    if(preview!=int.MaxValue||!r.ContainsKey("prerelease")||!(r["prerelease"] is bool)||(bool)r["prerelease"])continue;
    if(!r.ContainsKey("assets"))continue;var assets=r["assets"] as object[];if(assets==null)continue;
    var candidates=assets.OfType<Dictionary<string,object>>().Where(x=>x.ContainsKey("name")&&(x["name"] as string)==DownloadName).ToList();if(candidates.Count!=1)continue;var a=candidates[0];
    if(!a.ContainsKey("state")||(a["state"] as string)!="uploaded"||!a.ContainsKey("size")||!a.ContainsKey("digest")||!a.ContainsKey("browser_download_url"))continue;
    long bytes;string digest=a["digest"] as string,url=a["browser_download_url"] as string;
    if(!long.TryParse(Convert.ToString(a["size"]),out bytes)||bytes<1024||bytes>512L*1024*1024||!Regex.IsMatch(digest??"",@"^sha256:[a-f0-9]{64}$")||url!=Repository+"/releases/download/"+tag+"/"+DownloadName)continue;
    var release=new HorizonRelease{Tag=tag,Name=r.ContainsKey("name")?r["name"] as string:tag,Url=Repository+"/releases/tag/"+tag,DownloadUrl=url,Sha256=digest.Substring(7),Bytes=bytes};if(best==null||Compare(tag,best.Tag)>0)best=release;
   }if(best==null)throw new IOException("No complete Horizon release with a verified installer is available on GitHub yet.");return best;
  }
  static HttpClient Client(){ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;var client=new HttpClient(new HttpClientHandler{AllowAutoRedirect=false}){Timeout=Timeout.InfiniteTimeSpan};client.DefaultRequestHeaders.UserAgent.ParseAdd("RubiKaHorizon/0.7");return client;}
  internal static async Task<HttpResponseMessage> Get(HttpClient client,string url,CancellationToken cancel){
   for(int i=0;i<6;i++){
    var uri=new Uri(url);if(uri.Scheme!="https"||!(uri.Host=="github.com"||uri.Host=="api.github.com"||uri.Host.EndsWith(".githubusercontent.com",StringComparison.OrdinalIgnoreCase)))throw new IOException("The update download left GitHub's secure servers.");
    var response=await client.GetAsync(uri,HttpCompletionOption.ResponseHeadersRead,cancel).ConfigureAwait(false);
    int status=(int)response.StatusCode;if(status>=300&&status<400&&response.Headers.Location!=null){url=new Uri(uri,response.Headers.Location).AbsoluteUri;response.Dispose();continue;}
    if(status==403||status==429){response.Dispose();throw new IOException("GitHub has temporarily limited update checks. Try again later.");}
    if(!response.IsSuccessStatusCode){response.Dispose();throw new IOException("GitHub could not complete the request (HTTP "+status+"). Try again later.");}return response;
   }throw new IOException("Too many download redirects.");
  }
  internal static async Task<string> ReadMetadata(Stream stream,CancellationToken cancel){using(var output=new MemoryStream()){var b=new byte[16384];int n;while((n=await stream.ReadAsync(b,0,b.Length,cancel).ConfigureAwait(false))>0){if(output.Length+n>4000000)throw new IOException("The release list is too large.");output.Write(b,0,n);}return Encoding.UTF8.GetString(output.ToArray());}}
  public static async Task<HorizonRelease> Check(CancellationToken cancel){using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(cancel)){timeout.CancelAfter(TimeSpan.FromSeconds(25));using(var client=Client())using(var response=await Get(client,Api,timeout.Token).ConfigureAwait(false))using(var input=await response.Content.ReadAsStreamAsync().ConfigureAwait(false))return Select("["+await ReadMetadata(input,timeout.Token).ConfigureAwait(false)+"]");}}
  internal static async Task CopyVerified(Stream source,string path,HorizonRelease release,Action<long,long> progress,CancellationToken cancel){
   long total=0;using(var hash=SHA256.Create())using(var output=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.None,65536,true)){
    var b=new byte[65536];int n;while((n=await source.ReadAsync(b,0,b.Length,cancel).ConfigureAwait(false))>0){if(total+n>release.Bytes)throw new IOException("The update is larger than its published size.");await output.WriteAsync(b,0,n,cancel).ConfigureAwait(false);hash.TransformBlock(b,0,n,b,0);total+=n;progress(total,release.Bytes);}
    hash.TransformFinalBlock(new byte[0],0,0);if(total!=release.Bytes||BitConverter.ToString(hash.Hash).Replace("-","").ToLowerInvariant()!=release.Sha256)throw new IOException("The update did not match its published checksum. It was not installed.");output.Flush(true);
   }
  }
  internal static void ValidateBundle(string file){using(var s=File.OpenRead(file)){if(s.Length<1080||s.ReadByte()!=77||s.ReadByte()!=90)throw new IOException("The update is not a Horizon installer.");s.Position=s.Length-56;var r=new BinaryReader(s);if(Encoding.ASCII.GetString(r.ReadBytes(8))!="RKHCORE1")throw new IOException("The update payload is missing.");long offset=r.ReadInt64(),length=r.ReadInt64();if(offset<1024||length<1||offset+length!=s.Length-56||offset>s.Length-56)throw new IOException("The update payload is incomplete.");}}
  public static async Task<string> Download(HorizonRelease release,Action<long,long> progress,CancellationToken cancel){
   if(release.DownloadUrl!=Repository+"/releases/download/"+release.Tag+"/"+DownloadName)throw new IOException("Unrecognized update source.");string root=Path.Combine(App.StateRoot,"updates");SetupService.CheckDirectorySafety(root);Directory.CreateDirectory(root);
   string folder=SetupService.GuardUnder(root,Path.Combine(root,release.Tag+"-"+release.Sha256.Substring(0,12)));Directory.CreateDirectory(folder);string cached=SetupService.GuardUnder(folder,Path.Combine(folder,DownloadName));
   if(File.Exists(cached)&&new FileInfo(cached).Length==release.Bytes&&SetupService.Hash(cached)==release.Sha256){ValidateBundle(cached);return cached;}
   // Keep an altered cached file for diagnosis and use a fresh owned directory.
   if(File.Exists(cached)){folder=SetupService.GuardUnder(root,Path.Combine(root,"download-"+Guid.NewGuid().ToString("N")));Directory.CreateDirectory(folder);cached=SetupService.GuardUnder(folder,Path.Combine(folder,DownloadName));}
   string partial=SetupService.GuardUnder(folder,cached+"."+Guid.NewGuid().ToString("N")+".partial");
   try{using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(cancel)){timeout.CancelAfter(TimeSpan.FromMinutes(30));using(var client=Client())using(var response=await Get(client,release.DownloadUrl,timeout.Token).ConfigureAwait(false))using(var input=await response.Content.ReadAsStreamAsync().ConfigureAwait(false))await CopyVerified(input,partial,release,progress,timeout.Token).ConfigureAwait(false);}ValidateBundle(partial);File.Move(partial,cached);return cached;}
   finally{if(File.Exists(partial))File.Delete(SetupService.GuardUnder(folder,partial));}
  }
  public static string PrepareHandoff(string downloaded,HorizonRelease release,string client,string edition,bool skipMemoryChange){
   if(SetupService.Hash(downloaded)!=release.Sha256)throw new IOException("The downloaded update changed. Download it again.");ValidateBundle(downloaded);SetupService.AssertSelectedClientClosed(client);
   string ticket=SetupService.GuardUnder(Path.Combine(App.StateRoot,"updates"),Path.Combine(Path.GetDirectoryName(downloaded),"handoff-"+Guid.NewGuid().ToString("N")+".json"));
   using(var process=Process.GetCurrentProcess()){File.WriteAllText(ticket,Json().Serialize(new Dictionary<string,object>{{"format",1},{"parentPid",process.Id},{"parentStartUtc",process.StartTime.ToUniversalTime().Ticks},{"createdUtc",DateTime.UtcNow.Ticks},{"tag",release.Tag},{"sha256",release.Sha256},{"client",Path.GetFullPath(client)},{"edition",edition},{"skipMemoryChange",skipMemoryChange}}),Encoding.UTF8);}return ticket;
  }
  internal static Dictionary<string,object> ReadHandoff(string ticket){
   ticket=SetupService.GuardUnder(Path.Combine(App.StateRoot,"updates"),ticket);if(new FileInfo(ticket).Length>16384)throw new IOException("Invalid update handoff.");var t=Json().Deserialize<Dictionary<string,object>>(File.ReadAllText(ticket));
   if(Convert.ToInt32(t["format"])!=1||(string)t["tag"]!=VersionTag||SetupService.Hash(SetupService.ExePath)!=(string)t["sha256"]||Math.Abs((DateTime.UtcNow-new DateTime(Convert.ToInt64(t["createdUtc"]),DateTimeKind.Utc)).TotalMinutes)>10||!new[]{"lite","full"}.Contains((string)t["edition"]))throw new IOException("The update handoff expired or does not match this installer. No graphics files changed.");return t;
  }
  internal static void WaitForParent(Dictionary<string,object> ticket){try{using(var p=Process.GetProcessById(Convert.ToInt32(ticket["parentPid"]))){if(p.StartTime.ToUniversalTime().Ticks!=Convert.ToInt64(ticket["parentStartUtc"]))throw new IOException("The update parent process changed.");if(!p.WaitForExit(30000))throw new IOException("Close the previous Horizon window before applying this update.");}}catch(ArgumentException){/* The matching parent has already exited. */}}
  internal static bool GraphicsCurrent(string client,string edition){
   try{
    var json=new JavaScriptSerializer{MaxJsonLength=32000000};string path=SetupService.GuardUnder(client,Path.Combine(client,"ao-vk-live.install.json"));if(new FileInfo(path).Length>32000000)return false;
    var manifest=json.Deserialize<Dictionary<string,object>>(App.ResourceText(edition+"-manifest.json"));var receipt=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path));
    if((string)receipt["edition"]!=edition||Convert.ToString(receipt["build"])!=Convert.ToString(manifest["build"]))return false;
    foreach(var p in (Dictionary<string,object>)manifest["addon"])if(SetupService.Hash(Path.Combine(client,p.Key))!=(string)p.Value)return false;
    var expected=(Dictionary<string,object>)manifest["assets"];var owned=(Dictionary<string,object>)receipt["assets"];foreach(var p in expected){if(p.Key=="randy-vk/mesh-ids.bin"&&!owned.ContainsKey(p.Key))continue;if(!owned.ContainsKey(p.Key)||Convert.ToString(owned[p.Key])!=Convert.ToString(p.Value))return false;}
    // A removed texture/cache entry also requires the install transaction to
    // retire the owned old file. Only the generated bindings index is extra.
    foreach(var p in owned)if(!(p.Key=="randy-vk/cache/materials/bindings.tsv"&&manifest.ContainsKey("preparedMaterials"))&&!expected.ContainsKey(p.Key))return false;
    return true;
   }catch{return false;}
  }
 }
}
