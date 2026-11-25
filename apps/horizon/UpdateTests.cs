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
   string work=Path.Combine(root,"cached-setup-refresh");Directory.CreateDirectory(work);string script=Path.Combine(work,"install-live.ps1");File.WriteAllText(script,"older setup version");string archive=Path.Combine(work,"verified-materials.zip");File.WriteAllText(archive,"reusable texture bytes");string archiveHash=SetupService.Hash(archive);byte[] latest=Encoding.UTF8.GetBytes("new verified setup version");
   SetupService.RefreshCachedFile(work,script,Sha(latest),p=>File.WriteAllBytes(p,latest));Check(File.ReadAllText(script)=="new verified setup version"&&Directory.GetFiles(work,"install-live.ps1.previous-*").Any(p=>File.ReadAllText(p)=="older setup version")&&SetupService.Hash(archive)==archiveHash,"Refreshing owned setup code preserves its older copy and leaves reusable texture archives untouched");
   Refused(()=>SetupService.RefreshCachedFile(work,script,new string('a',64),p=>File.WriteAllText(p,"wrong replacement")),"A bad refreshed setup file cannot replace the working cached version");Check(File.ReadAllText(script)=="new verified setup version","Failed setup refresh preserves the last verified cached file");
   // A matching receipt skips a full texture scan during app-only updates.
   var lite=json.Deserialize<Dictionary<string,object>>(App.ResourceText("lite-manifest.json"));var addon=(Dictionary<string,object>)lite["addon"];
   var coreIndex=json.Deserialize<Dictionary<string,string>>(App.ResourceText("core-files.json"));Check(coreIndex.ContainsKey("ao-vk.dll"),"The update keeps the renderer's pinned bootstrap integrity index");
   string core=await Task.Run(()=>SetupService.EnsureCore((m,p)=>{}));foreach(var name in new[]{"ao-vk.dll","ddraw.dll"})File.Copy(Path.Combine(core,name),Path.Combine(clientDir,name),true);
   var full=json.Deserialize<Dictionary<string,object>>(App.ResourceText("full-manifest.json"));string receipt=Path.Combine(clientDir,"ao-vk-live.install.json");File.WriteAllText(receipt,json.Serialize(full));
   Check(UpdateService.GraphicsCurrent(clientDir,"full"),"A full-sized matching receipt skips texture verification during app-only updates");
   var installedAssets=(Dictionary<string,object>)full["assets"];string retired="randy-vk/packs/ao-cyber-world/1010004_999001_d.png";installedAssets[retired]=new string('a',64);File.WriteAllText(receipt,json.Serialize(full));Check(!UpdateService.GraphicsCurrent(clientDir,"full"),"A removal-only texture release cannot skip retiring an older owned asset");installedAssets.Remove(retired);
   string replaced=installedAssets.Keys.First(k=>k.EndsWith("_d.png",StringComparison.Ordinal));object savedHash=installedAssets[replaced];installedAssets[replaced]=new string('b',64);File.WriteAllText(receipt,json.Serialize(full));Check(!UpdateService.GraphicsCurrent(clientDir,"full"),"Changed texture hashes require an update even when the renderer version is identical");installedAssets[replaced]=savedHash;File.WriteAllText(receipt,json.Serialize(full));
   Check(!UpdateService.GraphicsCurrent(clientDir,"lite"),"Changing edition still requests a graphics update");
   File.WriteAllText(Path.Combine(clientDir,"ao-vk.dll"),"changed renderer");Check(!UpdateService.GraphicsCurrent(clientDir,"full"),"A changed renderer cannot be treated as already current");File.Copy(Path.Combine(core,"ao-vk.dll"),Path.Combine(clientDir,"ao-vk.dll"),true);
   full["clientDirectory"]=clientDir;File.WriteAllText(receipt,json.Serialize(full));
   var installedState=InstallDetection.Read(clientDir,"full",appDir);Check(installedState.AppPresent&&installedState.AppManaged&&installedState.AppCurrent&&installedState.AppVersion==SetupService.Version,"Installation detection recognizes the owned app independently of the running copy's location");
   var bundledEngine=json.Deserialize<Dictionary<string,object>>(App.ResourceText("engine.json"));
   var knownVersions=json.Deserialize<Dictionary<string,string>>(App.ResourceText("graphics-versions.json"));
   string bundledLabel=knownVersions[(string)bundledEngine["rendererSha256"]];
   Check(installedState.GraphicsManaged&&installedState.GraphicsCurrent&&installedState.GraphicsVersion==bundledLabel&&bundledLabel.StartsWith("r"+Convert.ToString(bundledEngine["revision"]),StringComparison.Ordinal)&&installedState.Edition=="full","Installation detection identifies the bundled Full HD revision from the actual renderer hash, packaged label and matching receipt");
   Check(installedState.PrimaryAction.Contains("up to date"),"A fully current app and graphics are not offered another installation");
   var noApp=InstallDetection.Read(clientDir,"full",Path.Combine(root,"no-app"));Check(noApp.GraphicsCurrent&&!noApp.AppPresent&&noApp.PrimaryAction=="Install Horizon app","Existing graphics without a Start-menu app offer an app-only install");
   record["version"]="0.7.0";File.WriteAllText(ownership,json.Serialize(record));var older=InstallDetection.Read(clientDir,"full",appDir);Check(older.AppPresent&&!older.AppCurrent&&older.AppVersion=="0.7.0"&&older.PrimaryAction=="Update Horizon app","An older installed app is shown as installed and offers an update rather than a fresh combined install");
   var missing=InstallDetection.Read(Path.Combine(root,"no-game"),"lite",Path.Combine(root,"no-app"));Check(!missing.GraphicsPresent&&!missing.AppPresent&&missing.PrimaryAction=="Install Horizon + graphics","A genuinely missing installation offers installation of both components");
   string scope=Guid.NewGuid().ToString("N"),ownerRoot=Path.Combine(root,"singleton-owner"),secondRoot=Path.Combine(root,"singleton-secondary");Directory.CreateDirectory(ownerRoot);Directory.CreateDirectory(secondRoot);
   Func<string,System.Diagnostics.Process> startProbe=dir=>System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(SetupService.ExePath,"--singleton-probe "+scope+" --state-root \""+dir+"\""){UseShellExecute=false,CreateNoWindow=true,WindowStyle=System.Diagnostics.ProcessWindowStyle.Hidden});
   using(var first=startProbe(ownerRoot)){
    for(int i=0;i<100&&!File.Exists(Path.Combine(ownerRoot,"probe-ready.txt"));i++)await Task.Delay(50);
    Check(File.Exists(Path.Combine(ownerRoot,"probe-ready.txt")),"The first isolated process acquires the single-instance gate");
    var parentWait=new Dictionary<string,object>{{"parentPid",first.Id},{"parentStartUtc",first.StartTime.ToUniversalTime().Ticks}};var waiting=Task.Run(()=>UpdateService.WaitForParent(parentWait));await Task.Delay(100);Check(!waiting.IsCompleted,"An update successor waits while its actual parent process is still alive");
    using(var second=startProbe(secondRoot)){bool secondExit=await Task.Run(()=>second.WaitForExit(10000));bool firstExit=await Task.Run(()=>first.WaitForExit(10000));Check(secondExit&&firstExit&&second.ExitCode==0&&first.ExitCode==0&&File.ReadAllText(Path.Combine(secondRoot,"probe-result.txt"))=="secondary:True"&&File.ReadAllText(Path.Combine(ownerRoot,"probe-result.txt"))=="owner:True","A second real process exits and activates the first without opening a second window");}
    await waiting;Check(first.HasExited,"The installed app restart can proceed only after the updater parent has exited");
   }
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
