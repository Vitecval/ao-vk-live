using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Web.Script.Serialization;
using System.Threading.Tasks;

namespace Horizon {
 public static class SelfTests {
  static readonly List<string> Passed=new List<string>();
  static void Check(bool value,string message){if(!value)throw new Exception(message);Passed.Add(message);}
  public static async Task Tray(string root){
   App.RenderingPreview=true;var w=new MainWindow{Left=-3000,Top=-3000,ShowActivated=false,ShowInTaskbar=false};
   try{w.Show();await Task.Delay(100);for(int i=0;i<3;i++){
    w.WindowState=System.Windows.WindowState.Minimized;await Task.Delay(100);
    if(w.IsVisible||!w.TrayVisible)throw new Exception("Minimize did not hide the window and register the tray icon.");
    w.RestoreFromTray();await Task.Delay(100);
    if(!w.IsVisible||w.WindowState!=System.Windows.WindowState.Normal||w.TrayVisible)throw new Exception("Tray restore did not restore the existing window.");
   }File.WriteAllText(Path.Combine(root,"TRAY-TEST-RESULTS.json"),"{\"result\":\"passed\",\"minimizeRestoreCycles\":3,\"retailClientsChanged\":false}");}
   finally{w.Close();}
  }
  public static void Run(string root){
   PresetTests.Run(root);
   var activity=new SetupActivity();var start=new DateTime(2026,11,16,0,0,0,DateTimeKind.Utc);
   activity.Start("Reading interrupted update record",start);
   Check(activity.Timing(start.AddSeconds(12)).Contains("No new progress report for 0:12"),"Setup elapsed time remains visible without worker output");
   activity.Report("Restoring previous installation: 20 / 80",25,start.AddSeconds(13));
   Check(activity.Message.Contains("20 / 80")&&activity.Percent==25&&!activity.Timing(start.AddSeconds(14)).Contains("No new"),"Recovery reports retain real counts and reset waiting feedback");
   Check(activity.Timing(start.AddSeconds(45)).Contains("security prompt"),"A silent wait suggests checking Windows prompts without claiming progress");
   activity.Stop(start.AddSeconds(50));Check(activity.Timing(start.AddMinutes(5))=="Elapsed 0:50","Completed setup timing stops advancing");
   activity.Start("Checking saved setup files",start.AddMinutes(6));Check(activity.Timing(start.AddMinutes(6))=="Elapsed 0:00","Retry clears the previous attempt's elapsed and waiting state");
   var catalog=Catalog.Load();Check(catalog.settings.Count>=200,"Catalog exports every visible renderer setting");
   Check(KeybindHelp.All.Length>=45&&KeybindHelp.All.All(k=>k.Keys.Length>0&&k.Title.Length>0&&k.Description.Length>0),"Keybinds provide searchable explanations for graphics, options, diagnostics, camera and console controls");
   Check(catalog.settings.Single(s=>s.key=="RVK_VividTone").defaultValue==0&&catalog.presets.All(p=>!p.values.ContainsKey("RVK_VividTone")),"Vivid Tone is off by default and presets preserve the user's choice");
   Check(catalog.settings.Select(x=>x.key).Distinct().Count()==catalog.settings.Count,"Settings keys are unique");
   foreach(var s in catalog.settings){s.Normalize(s.defaultValue);if(s.type==3)Check(s.Options().Count>0,"Choices: "+s.key);}
   Check(catalog.presets.Count==4&&catalog.presets[0].values["RVK_Enhance"]==0,"Original preset disables enhancements");
   Check(catalog.presets.All(p=>!p.values.ContainsKey("RVK_ZoneClick")),"Presets preserve the click-to-continue preference");
   var art=catalog.settings.Single(s=>s.key=="RVK_CharacterStyle");
   Check(art.Quick&&catalog.settings.Single(s=>s.key=="RVK_AutoMat").Quick,"Character materials and artwork appear in common settings");
   Check(art.defaultValue==0&&art.Options().Select(o=>o.Label).SequenceEqual(new[]{"Standard Enhanced","Reworked"}),"Artwork labels preserve the standard default");
   Check(catalog.presets.All(p=>!p.values.ContainsKey("RVK_CharacterStyle")),"Presets preserve the character artwork preference");
   string client=Path.Combine(root,"AO [personal] O'Brien 测试");Directory.CreateDirectory(client);File.WriteAllText(Path.Combine(client,"randy31.dll"),"dummy never loaded");File.WriteAllText(Path.Combine(client,"AnarchyOnline.exe"),"dummy never executed");
   var store=new SettingsStore(client,catalog.settings);string ini=store.IniPath;
   var beforeFirstLaunch=new SettingsStore(client,catalog.settings);
   beforeFirstLaunch.Save(new Dictionary<string,double>{{"RVK_ZoneScreen",1},{"RVK_ZoneClick",0}});
   Check(beforeFirstLaunch.Read()["RVK_ZoneScreen"]=="1"&&beforeFirstLaunch.Read()["RVK_ZoneClick"]=="0","Zoning enabled before the first AO launch is saved in the Renderer section");
   string original="; personal café 测试\r\n[Renderer]\r\nRVK_GrassDist=80\r\nRVK_ZoneClick=1\r\nPersonalUnknown=17\r\n[Particles]\r\nTemplates=1,2,3\r\n[Personal]\r\nName=客户\r\n";File.WriteAllText(ini,original,Encoding.Unicode);
   store.Save(new Dictionary<string,double>{{"RVK_GrassDist",73}});
   Check(store.Read()["RVK_GrassDist"]=="73","Writes survive literal and Unicode paths");
   string after=File.ReadAllText(ini);Check(after.Contains("PersonalUnknown=17")&&after.Contains("Templates=1,2,3")&&after.Contains("Name=客户")&&after.Contains("personal café"),"Unrelated keys, sections, Unicode and comments survive");
   store.Undo();Check(store.Read()["RVK_GrassDist"]=="80","Undo restores the previous setting");
   foreach(var p in catalog.presets){store.Save(p.values);Check(store.Read()["RVK_ZoneClick"]=="1","Preset preserves personal click preference: "+p.name);store.Undo();}
   string before=File.ReadAllText(ini);try{store.Save(new Dictionary<string,double>{{"RVK_GrassDist",double.NaN}});throw new Exception("NaN accepted");}catch(ArgumentException){}Check(before==File.ReadAllText(ini),"Invalid numbers leave the file unchanged");
   try{store.Save(new Dictionary<string,double>{{"Not_A_Setting",1}});throw new Exception("Unknown key accepted");}catch(ArgumentException){}Check(before==File.ReadAllText(ini),"Unknown settings cannot be written");
   store.Save(new Dictionary<string,double>{{"RVK_GrassDist",72}});var other=new SettingsStore(client,catalog.settings);other.Save(new Dictionary<string,double>{{"RVK_GrassDist",71}});
   try{store.Undo();throw new Exception("Undo overwrote a newer change");}catch(IOException){}Check(store.Read()["RVK_GrassDist"]=="71","Undo refuses to overwrite a newer external setting");
   before=File.ReadAllText(ini);try{SetupService.GuardUnder(root,Path.GetFullPath(Path.Combine(root,"..","elsewhere")));throw new Exception("Escaping path accepted");}catch(IOException){}Check(before==File.ReadAllText(ini),"Setup path traversal is rejected");
   Check(SetupService.UsesSelectedFolder(client,Path.Combine(client,"AnarchyOnline.exe")),"Setup identifies the selected running client");
   Check(!SetupService.UsesSelectedFolder(client,Path.Combine(client+" other","AnarchyOnline.exe")),"Setup does not confuse a sibling AO installation with the selected folder");
   Check(SetupService.UsesSelectedFolder(client.ToUpperInvariant(),Path.Combine(client,"Anarchy.exe")),"The selected-client guard respects Windows path casing");
   var lite=new JavaScriptSerializer{MaxJsonLength=32000000}.Deserialize<Dictionary<string,object>>(App.ResourceText("lite-manifest.json"));
   Check(!lite.ContainsKey("materialDownload")&&!lite.ContainsKey("preparedMaterials")&&!lite.ContainsKey("materialParts"),"Lite has no texture download or prepared-cache metadata");
   var full=new JavaScriptSerializer{MaxJsonLength=32000000}.Deserialize<Dictionary<string,object>>(App.ResourceText("full-manifest.json"));
   Check(((System.Collections.IList)full["materialParts"]).Count==19,"Full retains all 19 verified companions");
   Check(Convert.ToInt32(((Dictionary<string,object>)full["preparedMaterials"])["files"])==14345,"Full retains 14,345 prepared cache files");
   string appDir=Path.Combine(root,"Installed Horizon"),shortcut=Path.Combine(root,"Horizon test.lnk");
   string installed=SetupService.InstallAppAt(appDir,shortcut,false);Check(SetupService.Hash(installed)==SetupService.Hash(SetupService.ExePath),"App installation preserves every bundled EXE byte");
   SetupService.InstallAppAt(appDir,shortcut,false);Check(File.Exists(shortcut)&&File.Exists(Path.Combine(appDir,"horizon-install.json")),"Repeated app installation keeps a valid shortcut and ownership record");
   File.WriteAllText(installed,"personal modified application");try{SetupService.InstallAppAt(appDir,shortcut,false);throw new Exception("Modified app overwritten");}catch(IOException){}Check(File.ReadAllText(installed)=="personal modified application","Application install preserves a modified existing file");
   File.WriteAllText(Path.Combine(root,"TEST-RESULTS.json"),new JavaScriptSerializer().Serialize(new{result="passed",checks=Passed,retailClientsChanged=false}));
  }
  public static async Task Setup(string root,string stock){
   var j=new JavaScriptSerializer{MaxJsonLength=32000000};var lite=j.Deserialize<Dictionary<string,object>>(App.ResourceText("lite-manifest.json"));
   string client=Path.Combine(root,"AO isolated setup fixture");Directory.CreateDirectory(client);
   foreach(var p in (Dictionary<string,object>)lite["client"]){string source=Path.Combine(stock,p.Key);if(SetupService.Hash(source)!=(string)p.Value)throw new Exception("Stock fixture differs: "+p.Key);File.Copy(source,Path.Combine(client,p.Key),true);}
   File.WriteAllText(Path.Combine(client,"AnarchyOnline.exe"),"dummy never executed");File.WriteAllText(Path.Combine(client,"Anarchy.exe"),"dummy never executed");File.WriteAllText(Path.Combine(client,"randy-vk.ini"),"Personal=73");File.WriteAllText(Path.Combine(client,"personal.txt"),"preserve");
   Action<string,int> report=(m,p)=>File.AppendAllText(Path.Combine(root,"setup-test.log"),m+Environment.NewLine);
   string litePackage=Path.Combine(SetupService.PackageRoot,"Rubi-Ka Horizon r59 lite"),cachedScript=Path.Combine(litePackage,"install-live.ps1");
   foreach(string action in new[]{"Install","Update","Repair","Uninstall"}){
    if(action=="Update")File.WriteAllText(cachedScript,"old cached installer fixture; never execute");
    await SetupService.Run(action,client,"lite",report,true);
    if(action=="Update"&&!Directory.GetFiles(litePackage,"install-live.ps1.previous-*").Any(p=>File.ReadAllText(p)=="old cached installer fixture; never execute"))throw new Exception("Update did not refresh and preserve the older cached installer.");
   }
   if(File.Exists(Path.Combine(client,"ao-vk.dll"))||File.ReadAllText(Path.Combine(client,"randy-vk.ini"))!="Personal=73"||File.ReadAllText(Path.Combine(client,"personal.txt"))!="preserve")throw new Exception("Uninstall or personal file preservation failed.");
   await SetupService.Run("Reset settings",client,"lite",report,true);
   if(File.Exists(Path.Combine(client,"randy-vk.ini")))throw new Exception("Settings reset failed.");
   var packages=Path.Combine(SetupService.PackageRoot,"Rubi-Ka Horizon r59 lite");if(Directory.Exists(Path.Combine(packages,".ao-vk-downloads")))throw new Exception("Lite created a texture download cache.");
   await SetupService.Run("Reset settings",client,"full",report,true);
   var full=JsonFull();var first=(Dictionary<string,object>)((System.Collections.IList)full["materialParts"])[0];
   string fullPackage=Path.Combine(SetupService.PackageRoot,"Rubi-Ka Horizon r59 full"),cache=Path.Combine(fullPackage,".ao-vk-downloads");Directory.CreateDirectory(cache);
   string partial=Path.Combine(cache,(string)first["name"]+".partial");File.WriteAllText(partial,"resumable test data");
   string unknown=Path.Combine(fullPackage,"personal-cache-note.txt");File.WriteAllText(unknown,"preserve note");
   await SetupService.Run("Reset settings",client,"full",report,true);
   if(File.ReadAllText(partial)!="resumable test data"||File.ReadAllText(unknown)!="preserve note")throw new Exception("Full resumable data or unrelated files were removed.");
   File.WriteAllText(Path.Combine(root,"SETUP-TEST-RESULTS.json"),j.Serialize(new{result="passed",bundledExe=true,actions=new[]{"install","update","repair","uninstall","reset"},personalFilesPreserved=true,liteDownloads=false,retailClientsChanged=false}));
  }
  static Dictionary<string,object> JsonFull(){return new JavaScriptSerializer{MaxJsonLength=32000000}.Deserialize<Dictionary<string,object>>(App.ResourceText("full-manifest.json"));}
 }
}
