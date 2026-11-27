using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;

namespace Horizon {
 public static class AppOnlyUpdateTests {
  public static Task Run(string root){return MainWindow.AppOnlyUpdateChecks(root);}
 }
 public partial class MainWindow {
  const string ExistingGraphicsScope="app-and-existing-graphics";
  sealed class AppUpdateCalls {public int Installs,Graphics,ClosedChecks,Starts,Closes;public string StartedFile="",StartedArgs="";}
  static void AppUpdateCheck(bool condition,string label,List<string> checks){if(!condition)throw new Exception(label);checks.Add(label);}
  static Dictionary<string,string> AppUpdateSnapshot(string directory){
   var result=new Dictionary<string,string>(StringComparer.Ordinal);
   if(string.IsNullOrEmpty(directory)){result["<empty>"]="";return result;}
   if(!Directory.Exists(directory)){result["<absent>"]=Path.GetFullPath(directory);return result;}
   result["<root>"]="";
   foreach(var path in Directory.GetDirectories(directory,"*",SearchOption.AllDirectories))result["directory:"+path.Substring(directory.Length)]="";
   foreach(var path in Directory.GetFiles(directory,"*",SearchOption.AllDirectories)){var info=new FileInfo(path);result["file:"+path.Substring(directory.Length)]=SetupService.Hash(path)+":"+info.Length+":"+info.LastWriteTimeUtc.Ticks;}
   return result;
  }
  static bool AppUpdateSame(Dictionary<string,string> a,Dictionary<string,string> b){return a.Count==b.Count&&a.All(p=>b.ContainsKey(p.Key)&&b[p.Key]==p.Value);}
  static string AppUpdateClient(string root,string name,bool graphics,bool ini){
   string directory=Path.Combine(root,name);Directory.CreateDirectory(directory);
   foreach(var file in new[]{"AnarchyOnline.exe","Anarchy.exe","randy31.dll","personal.txt"})File.WriteAllText(Path.Combine(directory,file),"isolated fixture, never executed or loaded: "+file);
   Directory.CreateDirectory(Path.Combine(directory,"personal"));File.WriteAllText(Path.Combine(directory,"personal","keep.txt"),"preserve private player files");
   if(graphics){File.WriteAllText(Path.Combine(directory,"ao-vk.dll"),"stale renderer, never loaded");File.WriteAllText(Path.Combine(directory,"ao-vk-live.install.json"),"{\"product\":\"AO-VK Live\",\"edition\":\"full\",\"build\":\"older-owned-installation\",\"assets\":{},\"addon\":{}}");}
   if(ini)File.WriteAllText(Path.Combine(directory,"randy-vk.ini"),"[Renderer]\r\nRVK_ReworkSound=0\r\nUnknownPlayerPreference=keep\r\n[Display]\r\nMode=Game\r\n");
   return directory;
  }
  static MainWindow AppUpdateWindow(string clientPath,string ticket,bool completed){
   App.ClientArgument=clientPath;App.UpdateTicket=ticket;App.AppUpdateComplete=completed;
   var window=new MainWindow{Left=-3000,Top=-3000,ShowActivated=false,ShowInTaskbar=false};
   window.saveTimer.Stop();window.refreshTimer.Stop();window.activityTimer.Stop();return window;
  }
  static void AppUpdateEnd(MainWindow window){if(window==null)return;window.saveTimer.Stop();window.refreshTimer.Stop();window.activityTimer.Stop();window.pending.Clear();bool preview=App.RenderingPreview;App.RenderingPreview=true;try{window.Close();}finally{App.RenderingPreview=preview;}}
  static void AppUpdateInject(MainWindow window,AppUpdateCalls calls,string installedDirectory,List<string> checks){
   window.AppInstaller=()=>{Interlocked.Increment(ref calls.Installs);return SetupService.InstallAppAt(installedDirectory,Path.Combine(installedDirectory,"Horizon isolated test.lnk"),false);};
   window.GraphicsSetupRunner=(action,target,edition,progress,skip)=>{calls.Graphics++;throw new Exception("Downloaded app update attempted graphics setup: "+action);};
   window.ClientClosedCheck=target=>{calls.ClosedChecks++;throw new Exception("Downloaded app update checked an AO process");};
   window.UpdatedProcessStarter=(file,args)=>{calls.Starts++;calls.StartedFile=file;calls.StartedArgs=args;};
   window.UpdateWindowCloser=()=>{
    calls.Closes++;AppUpdateCheck(window.SaveChangesForClose(),"App transition close succeeds without saving pending AO edits",checks);
    bool preview=App.RenderingPreview;App.RenderingPreview=false;try{window.Close();}finally{App.RenderingPreview=preview;}
   };
  }
  static Dictionary<string,object> AppUpdateTicket(string clientPath,object scope,bool includeScope=true){
   var ticket=new Dictionary<string,object>{{"format",1},{"parentPid",int.MaxValue},{"parentStartUtc",0L},{"createdUtc",DateTime.UtcNow.Ticks},{"tag",UpdateService.VersionTag},{"sha256",SetupService.Hash(SetupService.ExePath)},{"client",clientPath},{"edition","full"},{"skipMemoryChange",true}};
   if(includeScope)ticket["updateScope"]=scope;return ticket;
  }
  static string[] AppUpdateHandoffs(){string directory=Path.Combine(App.StateRoot,"updates");return Directory.Exists(directory)?Directory.GetFiles(directory,"handoff-*.json",SearchOption.AllDirectories):new string[0];}
  static JavaScriptSerializer UpdatePolicyJson(){return new JavaScriptSerializer{MaxJsonLength=32000000};}
  static void AppUpdateManaged(string target,string core,string edition,bool current){
   foreach(var name in new[]{"ao-vk.dll","ddraw.dll"})File.Copy(Path.Combine(core,name),Path.Combine(target,name),true);
   var m=UpdatePolicyJson().Deserialize<Dictionary<string,object>>(App.ResourceText(edition+"-manifest.json"));
   var r=new Dictionary<string,object>{{"product","AO-VK Live"},{"clientDirectory",target},{"edition",edition},{"build",current?m["build"]:"older-managed-fixture"},{"addon",m["addon"]},{"assets",current?m["assets"]:new Dictionary<string,object>()}};
   File.WriteAllText(Path.Combine(target,"ao-vk-live.install.json"),UpdatePolicyJson().Serialize(r));
  }
  static void AppUpdateRemoveGraphics(string target){foreach(var name in new[]{"ao-vk.dll","ddraw.dll","ao-vk-live.install.json"})File.Delete(Path.Combine(target,name));}
  static string AppUpdateSharedLoader(string target,string fixtureRoot,string name){
   string loader=Path.Combine(fixtureRoot,name);Directory.CreateDirectory(loader);File.WriteAllText(Path.Combine(loader,"AnarchyOnline.exe"),"dummy shared launcher, never executed");
   File.Move(Path.Combine(target,"ddraw.dll"),Path.Combine(loader,"ddraw.dll"));string path=Path.Combine(target,"ao-vk-live.install.json");var receipt=UpdatePolicyJson().Deserialize<Dictionary<string,object>>(File.ReadAllText(path));receipt["loaderDirectory"]=loader;File.WriteAllText(path,UpdatePolicyJson().Serialize(receipt));return loader;
  }
  static async Task<AppUpdateCalls> PolicyReceiver(string label,string target,string scope,string edition,int closed,string installed,string presetsFile,List<string> checks,List<object> evidence,string ticketPath=null,bool removeWhenClosed=false,bool pendingClose=false){
   var before=AppUpdateSnapshot(target);var expectedAfter=before;string presetBefore=SetupService.Hash(presetsFile);var calls=new AppUpdateCalls();MainWindow w=null;
   try{
    w=AppUpdateWindow(target,ticketPath??"receiver test marker",false);AppUpdateCheck(w.store==null&&w.client=="","Receiver constructor skips choosing and migrating AO: "+label,checks);
    w.FullChoice.IsChecked=edition!="full";w.LiteChoice.IsChecked=edition=="full";AppUpdateInject(w,calls,installed,checks);
    if(pendingClose){w.store=new SettingsStore(target,w.catalog.settings);w.pending["RVK_ReworkSound"]=1;}
    w.ClientClosedCheck=path=>{calls.ClosedChecks++;AppUpdateCheck(path==target,"Existing update checks only its target",checks);if(removeWhenClosed){AppUpdateRemoveGraphics(target);expectedAfter=AppUpdateSnapshot(target);}};
    w.GraphicsSetupRunner=(action,path,chosen,progress,skip)=>{calls.Graphics++;AppUpdateCheck(edition.Length>0&&action=="Update existing"&&path==target&&chosen==edition&&skip,"Existing-only update preserves installed edition despite opposite UI/ticket choices: "+label,checks);return Task.CompletedTask;};
    var ticket=AppUpdateTicket(target,scope);ticket["edition"]=edition=="full"?"lite":"full";
    if(ticketPath==null)await w.CompleteAppUpdate(ticket);else await w.FinishDownloadedUpdate();
    AppUpdateCheck(calls.Installs==1&&calls.Starts==1&&calls.Closes==1,"App updates and restarts once: "+label,checks);
    AppUpdateCheck(calls.Graphics==(edition.Length>0?1:0)&&calls.ClosedChecks==closed,"Only eligible existing graphics invoke setup: "+label,checks);
    AppUpdateCheck(calls.StartedArgs.Contains("--app-update-complete")&&!calls.StartedArgs.Contains("--client"),"Restart suppresses preset migration",checks);
    if(pendingClose)AppUpdateCheck(w.pending.Count==1,"Update close leaves receiver-side pending edits unsaved",checks);
    AppUpdateCheck(AppUpdateSame(expectedAfter,AppUpdateSnapshot(target))&&SetupService.Hash(presetsFile)==presetBefore,"INI, personal files, presets and client timestamps stay unchanged: "+label,checks);
    AppUpdateCheck(SetupService.Hash(calls.StartedFile)==SetupService.Hash(SetupService.ExePath),"Private installed app equals the verified candidate",checks);
    evidence.Add(new{workflow=label,client=target,scope,expectedEdition=edition,before,controlledRemovalSnapshot=removeWhenClosed?expectedAfter:null,after=AppUpdateSnapshot(target),graphicsRuns=calls.Graphics,closedChecks=calls.ClosedChecks});return calls;
   }finally{AppUpdateEnd(w);}
  }
  internal static async Task AppOnlyUpdateChecks(string root){
   string savedRoot=App.StateRoot,savedClient=App.ClientArgument,savedTicket=App.UpdateTicket,savedPackage=SetupService.PackageRoot;bool savedPreview=App.RenderingPreview,savedCompleted=App.AppUpdateComplete;var shutdown=Application.Current.ShutdownMode;
   var checks=new List<string>();var evidence=new List<object>();var json=UpdatePolicyJson();int absentRuns=0,liteRuns=0,fullRuns=0,currentRuns=0,removedRuns=0,unknownRuns=0,legacyAbsentRuns=0,legacyExistingRuns=0,explicitGraphics=0,sharedCurrentRuns=0,sharedStaleRuns=0;bool actualLegacy=false;
   try{
    App.StateRoot=root;App.RenderingPreview=true;Application.Current.ShutdownMode=ShutdownMode.OnExplicitShutdown;
    string fixtures=Path.Combine(root,"workflow-clients");Directory.CreateDirectory(fixtures);
    string core=await Task.Run(()=>SetupService.EnsureCore((message,percent)=>{}));
    string noGraphics=AppUpdateClient(fixtures,"uninstalled Full selection",false,true),missing=Path.Combine(fixtures,"missing selected folder");
    string lite=AppUpdateClient(fixtures,"managed stale Lite",false,true),full=AppUpdateClient(fixtures,"managed stale Full",false,true);AppUpdateManaged(lite,core,"lite",false);AppUpdateManaged(full,core,"full",false);
    string currentLite=AppUpdateClient(fixtures,"managed current Lite",false,true),currentFull=AppUpdateClient(fixtures,"managed current Full",false,true);AppUpdateManaged(currentLite,core,"lite",true);AppUpdateManaged(currentFull,core,"full",true);
    AppUpdateCheck(UpdateService.InstalledGraphicsEdition(lite)=="lite"&&UpdateService.InstalledGraphicsEdition(full)=="full","Real addon bytes and matching ownership receipts identify Lite and Full",checks);
    AppUpdateCheck(UpdateService.GraphicsCurrent(currentLite,"lite")&&UpdateService.GraphicsCurrent(currentFull,"full"),"Current fixtures match actual bundled receipt and addon hashes",checks);
    string sharedCurrent=AppUpdateClient(fixtures,"managed current Full shared loader",false,true),sharedStale=AppUpdateClient(fixtures,"managed stale Lite shared loader",false,true);AppUpdateManaged(sharedCurrent,core,"full",true);AppUpdateManaged(sharedStale,core,"lite",false);
    string currentLoader=AppUpdateSharedLoader(sharedCurrent,fixtures,"current private shared loader"),staleLoader=AppUpdateSharedLoader(sharedStale,fixtures,"stale private shared loader");
    AppUpdateCheck(UpdateService.InstalledGraphicsEdition(sharedCurrent)=="full"&&UpdateService.InstalledGraphicsEdition(sharedStale)=="lite"&&UpdateService.GraphicsCurrent(sharedCurrent,"full")&&!UpdateService.GraphicsCurrent(sharedStale,"lite"),"Real shared-loader receipts distinguish current Full from stale Lite using the actual loader path",checks);
    string unknown=AppUpdateClient(fixtures,"unknown wrapper",false,true),malformed=AppUpdateClient(fixtures,"malformed receipt",false,true),wrongClient=AppUpdateClient(fixtures,"receipt for another client",false,true),unowned=AppUpdateClient(fixtures,"addon without ownership receipt",false,true),wrongProduct=AppUpdateClient(fixtures,"receipt for another product",false,true);
    AppUpdateManaged(unknown,core,"full",false);File.WriteAllText(Path.Combine(unknown,"ddraw.dll"),"unknown wrapper, preserve exact bytes");
    AppUpdateManaged(malformed,core,"full",false);File.WriteAllText(Path.Combine(malformed,"ao-vk-live.install.json"),"{broken receipt");
    AppUpdateManaged(wrongClient,core,"full",false);var wrong=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(Path.Combine(wrongClient,"ao-vk-live.install.json")));wrong["clientDirectory"]=noGraphics;File.WriteAllText(Path.Combine(wrongClient,"ao-vk-live.install.json"),json.Serialize(wrong));
    foreach(var name in new[]{"ao-vk.dll","ddraw.dll"})File.Copy(Path.Combine(core,name),Path.Combine(unowned,name));
    AppUpdateManaged(wrongProduct,core,"full",false);var foreign=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(Path.Combine(wrongProduct,"ao-vk-live.install.json")));foreign["product"]="Unrelated graphics product";File.WriteAllText(Path.Combine(wrongProduct,"ao-vk-live.install.json"),json.Serialize(foreign));
    AppUpdateCheck(new[]{noGraphics,missing,"",unknown,malformed,wrongClient,unowned,wrongProduct}.All(p=>UpdateService.InstalledGraphicsEdition(p)==""),"Absent, unowned, malformed, mismatched and unknown-wrapper installations cannot auto-update graphics",checks);
    string migration=AppUpdateClient(fixtures,"preset migration must not create INI",false,false);AppUpdateManaged(migration,core,"full",false);
    var definitions=Catalog.Load();new PresetStore(root).Select(migration,definitions.presets.First(),definitions);string presetsFile=Path.Combine(root,"presets.json");var presetData=json.Deserialize<PresetData>(File.ReadAllText(presetsFile));presetData.Clients[Path.GetFullPath(migration)].KnownKeys.Remove("RVK_ReworkSound");File.WriteAllText(presetsFile,json.Serialize(presetData));
    AppUpdateCheck(new PresetStore(root).NewBuiltInValues(migration,definitions).ContainsKey("RVK_ReworkSound"),"Migration fixture has an actual unacknowledged option that could create an INI",checks);
    string installed=Path.Combine(root,"private-installed-app");Directory.CreateDirectory(installed);
    foreach(var target in new[]{noGraphics,missing,""})absentRuns+=(await PolicyReceiver("absent stays absent",target,ExistingGraphicsScope,"",0,installed,presetsFile,checks,evidence)).Graphics;
    foreach(var target in new[]{unknown,malformed,wrongClient,unowned,wrongProduct})unknownRuns+=(await PolicyReceiver("unmanaged protected",target,ExistingGraphicsScope,"",0,installed,presetsFile,checks,evidence)).Graphics;
    liteRuns=(await PolicyReceiver("existing Lite preserved",lite,ExistingGraphicsScope,"lite",1,installed,presetsFile,checks,evidence)).Graphics;
    fullRuns=(await PolicyReceiver("existing Full preserved",full,ExistingGraphicsScope,"full",1,installed,presetsFile,checks,evidence,pendingClose:true)).Graphics;
    foreach(var target in new[]{currentLite,currentFull})currentRuns+=(await PolicyReceiver("current graphics skip",target,ExistingGraphicsScope,"",0,installed,presetsFile,checks,evidence)).Graphics;
    {var before=AppUpdateSnapshot(currentLoader);sharedCurrentRuns=(await PolicyReceiver("current shared loader skips setup",sharedCurrent,ExistingGraphicsScope,"",0,installed,presetsFile,checks,evidence)).Graphics;AppUpdateCheck(AppUpdateSame(before,AppUpdateSnapshot(currentLoader)),"Current external loader files and timestamps stay untouched",checks);}
    {var before=AppUpdateSnapshot(staleLoader);sharedStaleRuns=(await PolicyReceiver("stale shared loader preserves Lite",sharedStale,ExistingGraphicsScope,"lite",1,installed,presetsFile,checks,evidence)).Graphics;AppUpdateCheck(AppUpdateSame(before,AppUpdateSnapshot(staleLoader)),"Injected stale-loader routing leaves the separate loader tree unchanged",checks);}
    await PolicyReceiver("app scope excludes graphics",full,"app","",0,installed,presetsFile,checks,evidence);
    await PolicyReceiver("migration trap",migration,"app","",0,installed,presetsFile,checks,evidence);
    {var before=AppUpdateSnapshot(migration);string presetBefore=SetupService.Hash(presetsFile);MainWindow w=null;try{w=AppUpdateWindow(migration,"",true);AppUpdateCheck(AppUpdateSame(before,AppUpdateSnapshot(migration))&&!File.Exists(Path.Combine(migration,"randy-vk.ini"))&&SetupService.Hash(presetsFile)==presetBefore,"Normal post-update restart cannot create INI defaults or migrate presets",checks);}finally{AppUpdateEnd(w);}}
    string downloads=Path.Combine(root,"updates","sender-fixture");Directory.CreateDirectory(downloads);string download=Path.Combine(downloads,UpdateService.DownloadName);File.Copy(SetupService.ExePath,download,true);var release=new HorizonRelease{Tag=UpdateService.VersionTag,Sha256=SetupService.Hash(download),Bytes=new FileInfo(download).Length};
    string removed=AppUpdateClient(fixtures,"removed after handoff",false,true);AppUpdateManaged(removed,core,"full",false);string removalTicket=UpdateService.PrepareHandoff(download,release,removed,"lite",true);var removalRaw=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(removalTicket));AppUpdateCheck((string)removalRaw["updateScope"]==ExistingGraphicsScope&&(string)removalRaw["edition"]=="full","Sender records managed Full instead of the opposite UI choice",checks);AppUpdateRemoveGraphics(removed);
    removedRuns+=(await PolicyReceiver("removed during handoff",removed,ExistingGraphicsScope,"",0,installed,presetsFile,checks,evidence,removalTicket)).Graphics;
    string race=AppUpdateClient(fixtures,"removed during closed check",false,true);AppUpdateManaged(race,core,"full",false);removedRuns+=(await PolicyReceiver("removed immediately before runner",race,ExistingGraphicsScope,"",1,installed,presetsFile,checks,evidence,removeWhenClosed:true)).Graphics;
    string legacyMarker=Path.Combine(root,"legacy-handoff-input.json");
    if(File.Exists(legacyMarker)){
     var input=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(legacyMarker));string path=(string)input["ticket"],target=(string)input["client"];
     AppUpdateCheck((string)input["senderVersion"]=="0.7.9"&&SetupService.Hash((string)input["sender"])==(string)input["senderSha256"]&&SetupService.Hash(path)==(string)input["ticketSha256"],"Legacy fixture binds actual published 0.7.9 sender and original ticket bytes",checks);
     var raw=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path));AppUpdateCheck(!raw.ContainsKey("updateScope")&&(string)raw["edition"]=="full"&&(string)UpdateService.ReadHandoff(path)["updateScope"]==ExistingGraphicsScope,"Legacy missing scope means update existing graphics only",checks);
     legacyAbsentRuns=(await PolicyReceiver("actual079 legacy absent",target,ExistingGraphicsScope,"",0,installed,presetsFile,checks,evidence,path)).Graphics;
     AppUpdateManaged(target,core,"lite",false);legacyExistingRuns=(await PolicyReceiver("actual079 legacy installed Lite",target,ExistingGraphicsScope,"lite",1,installed,presetsFile,checks,evidence,path)).Graphics;
     AppUpdateCheck(SetupService.Hash(path)==(string)input["ticketSha256"],"Original legacy ticket is unchanged across absent and installed scenarios",checks);actualLegacy=true;
    }
    foreach(var target in new[]{"",noGraphics,missing,lite,full}){
     var before=AppUpdateSnapshot(target);string presetBefore=SetupService.Hash(presetsFile);var handoffsBefore=AppUpdateHandoffs();var calls=new AppUpdateCalls();MainWindow w=null;
     try{w=AppUpdateWindow(target,"sender marker",false);w.client=target;string edition=UpdateService.InstalledGraphicsEdition(target);w.FullChoice.IsChecked=edition!="full";w.LiteChoice.IsChecked=edition=="full";w.availableUpdate=release;w.downloadedUpdate=download;AppUpdateInject(w,calls,installed,checks);await w.ApplyDownloadedAppUpdate();string[] added=AppUpdateHandoffs().Except(handoffsBefore).ToArray();
      AppUpdateCheck(added.Length==1&&calls.Starts==1&&calls.Closes==1&&calls.Installs==0&&calls.Graphics==0&&calls.ClosedChecks==0,"Sender only starts the verified handoff, with no setup or AO process check",checks);
      var ticket=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(added[0]));AppUpdateCheck((string)ticket["updateScope"]==(edition.Length>0?ExistingGraphicsScope:"app")&&(edition.Length==0||(string)ticket["edition"]==edition),"New sender derives intent and edition from actual ownership",checks);
      AppUpdateCheck(AppUpdateSame(before,AppUpdateSnapshot(target))&&SetupService.Hash(presetsFile)==presetBefore,"Sender preserves all client files, timestamps and presets",checks);
     }finally{AppUpdateEnd(w);}
    }
    {var before=AppUpdateSnapshot(noGraphics);var handoffsBefore=AppUpdateHandoffs();var calls=new AppUpdateCalls();MainWindow w=null;try{w=AppUpdateWindow(noGraphics,"pending marker",false);w.client=noGraphics;w.store=new SettingsStore(noGraphics,w.catalog.settings);w.pending["RVK_ReworkSound"]=1;w.availableUpdate=release;w.downloadedUpdate=download;AppUpdateInject(w,calls,installed,checks);await w.ApplyDownloadedAppUpdate();AppUpdateCheck(w.pending.Count==1&&AppUpdateSame(before,AppUpdateSnapshot(noGraphics))&&AppUpdateHandoffs().Except(handoffsBefore).Count()==0&&calls.Starts==0&&calls.Closes==0&&calls.Graphics==0,"Pending edits block handoff without saving or discarding them",checks);}finally{AppUpdateEnd(w);}}
    foreach(object scope in new object[]{"graphics","full","",null,7}){string path=Path.Combine(root,"updates","invalid-scope-"+Guid.NewGuid().ToString("N")+".json");File.WriteAllText(path,json.Serialize(AppUpdateTicket(noGraphics,scope)));bool rejected=false;try{UpdateService.ReadHandoff(path);}catch(IOException){rejected=true;}AppUpdateCheck(rejected,"Explicit unsupported update scope is rejected",checks);}
    {var calls=new AppUpdateCalls();MainWindow w=null;var before=AppUpdateSnapshot(noGraphics);try{w=AppUpdateWindow(noGraphics,"explicit marker",false);w.client=noGraphics;w.store=new SettingsStore(noGraphics,w.catalog.settings);w.FullChoice.IsChecked=true;AppUpdateInject(w,calls,installed,checks);w.ClientClosedCheck=target=>{calls.ClosedChecks++;};w.GraphicsSetupRunner=(action,target,edition,progress,skip)=>{calls.Graphics++;AppUpdateCheck(action=="Update"&&target==noGraphics&&edition=="full","Explicit Installation remains distinct from the existing-only updater",checks);return Task.CompletedTask;};await w.Operation("Update");explicitGraphics=calls.Graphics;AppUpdateCheck(calls.Graphics==1&&calls.ClosedChecks==1&&AppUpdateSame(before,AppUpdateSnapshot(noGraphics)),"Explicit graphics setup positive control runs once, without real graphics execution",checks);}finally{AppUpdateEnd(w);}}
    {var calls=new AppUpdateCalls();MainWindow w=null;var before=AppUpdateSnapshot(full);string path=Path.Combine(root,"updates","failure-ticket.json");File.WriteAllText(path,json.Serialize(AppUpdateTicket(full,ExistingGraphicsScope)));try{w=AppUpdateWindow(full,path,false);AppUpdateInject(w,calls,installed,checks);w.ClientClosedCheck=target=>{calls.ClosedChecks++;};w.GraphicsSetupRunner=(action,target,edition,progress,skip)=>{calls.Graphics++;throw new IOException("Injected existing-graphics failure");};await w.FinishDownloadedUpdate();AppUpdateCheck(calls.Installs==1&&calls.Graphics==1&&calls.Starts==0&&calls.Closes==0&&File.Exists(Path.Combine(installed,"Rubi-Ka Horizon.exe")),"Graphics failure keeps updated app and window open for an explicit retry",checks);AppUpdateCheck(AppUpdateSame(before,AppUpdateSnapshot(full)),"Failed update fixture leaves AO content and settings unchanged",checks);}finally{AppUpdateEnd(w);}}
    File.WriteAllText(Path.Combine(root,"APP-ONLY-UPDATE-RESULTS.json"),json.Serialize(new{result="passed",policy="app-and-existing-graphics-only",checks,evidence,runningExecutable=SetupService.ExePath,runningExecutableSha256=SetupService.Hash(SetupService.ExePath),actualLegacy079Ticket=actualLegacy,absentGraphicsRuns=absentRuns,existingLiteRuns=liteRuns,existingFullRuns=fullRuns,currentGraphicsRuns=currentRuns,removedGraphicsRuns=removedRuns,unknownGraphicsRuns=unknownRuns,sharedCurrentRuns,sharedStaleRuns,legacyAbsentRuns,legacyExistingRuns,explicitGraphicsRuns=explicitGraphics,installedEditionPreserved=true,neverInstallMissing=true,clientFilesUnchanged=true,coreExtractedLocally=true,networkContacted=false,retailClientsChanged=false,gameLaunched=false,gpuUsed=false}));
   }finally{App.StateRoot=savedRoot;App.ClientArgument=savedClient;App.UpdateTicket=savedTicket;App.AppUpdateComplete=savedCompleted;App.RenderingPreview=savedPreview;SetupService.PackageRoot=savedPackage;Application.Current.ShutdownMode=shutdown;}
  }
 }
}
