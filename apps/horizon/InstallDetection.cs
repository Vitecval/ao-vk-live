using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Web.Script.Serialization;

namespace Horizon {
 internal sealed class InstalledState {
  public bool AppPresent,AppManaged,AppCurrent,GraphicsPresent,GraphicsManaged,GraphicsCurrent;
  public string AppVersion="Not installed",GraphicsVersion="Not installed",Edition="",Warning="";
  public bool? MemoryEnabled;
  public string PrimaryAction {
   get {
    if(!GraphicsPresent)return AppCurrent?"Install graphics":"Install Horizon + graphics";
    if(!GraphicsCurrent)return AppCurrent?"Update graphics":"Update Horizon + graphics";
    if(!AppPresent)return "Install Horizon app";
    return AppCurrent?"Installed — up to date":"Update Horizon app";
   }
  }
 }
 internal static class InstallDetection {
  public static string AppDirectory{get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","Rubi-Ka Horizon");}}
  static string Stamp(string path){try{var f=new FileInfo(path);return f.Exists?f.Length+":"+f.LastWriteTimeUtc.Ticks:"-";}catch{return "?";}}
  public static string Key(string client,string edition){string app=AppDirectory;return client+"|"+edition+"|"+Stamp(Path.Combine(app,"Rubi-Ka Horizon.exe"))+"|"+Stamp(Path.Combine(app,"horizon-install.json"))+"|"+Stamp(Path.Combine(client,"ao-vk.dll"))+"|"+Stamp(Path.Combine(client,"ddraw.dll"))+"|"+Stamp(Path.Combine(client,"ao-vk-live.install.json"))+"|"+Stamp(Path.Combine(client,"AnarchyOnline.exe"));}
  static Dictionary<string,object> Read(string path){if(new FileInfo(path).Length>32000000)throw new IOException("Installation record is too large.");return new JavaScriptSerializer{MaxJsonLength=32000000}.Deserialize<Dictionary<string,object>>(File.ReadAllText(path));}
  public static InstalledState Read(string client,string edition,string appDirectory=null){
   var result=new InstalledState();string directory=appDirectory??AppDirectory,exe=Path.Combine(directory,"Rubi-Ka Horizon.exe"),record=Path.Combine(directory,"horizon-install.json");
   if(File.Exists(exe)){
    result.AppPresent=true;
    try{SetupService.CheckDirectorySafety(directory);exe=SetupService.GuardUnder(directory,exe);var version=FileVersionInfo.GetVersionInfo(exe);result.AppVersion=version.FileVersion??"Unknown version";
     if(File.Exists(record)){record=SetupService.GuardUnder(directory,record);var receipt=Read(record);if((string)receipt["product"]=="Rubi-Ka Horizon"&&SetupService.Hash(exe)==(string)receipt["sha256"]){result.AppManaged=true;result.AppVersion=Convert.ToString(receipt["version"]);result.AppCurrent=result.AppVersion==SetupService.Version;}}
     if(!result.AppManaged)result.Warning="The existing app has no matching ownership record; its files will be preserved.";
    }catch{result.AppVersion="Could not verify";result.Warning="The existing Horizon app could not be verified.";}
   }
   if(string.IsNullOrEmpty(client))return result;
   string renderer=Path.Combine(client,"ao-vk.dll");if(!File.Exists(renderer))return result;result.GraphicsPresent=true;result.GraphicsVersion="Detected — revision unknown";
   try{
    renderer=SetupService.GuardUnder(client,renderer);string hash=SetupService.Hash(renderer);var known=new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(App.ResourceText("graphics-versions.json"));string knownVersion;if(known.TryGetValue(hash,out knownVersion))result.GraphicsVersion=knownVersion;
    string path=Path.Combine(client,"ao-vk-live.install.json");if(File.Exists(path)){
     var receipt=Read(SetupService.GuardUnder(client,path));var addon=receipt.ContainsKey("addon")?receipt["addon"] as Dictionary<string,object>:null;
     result.GraphicsManaged=receipt.ContainsKey("product")&&(string)receipt["product"]=="AO-VK Live"&&receipt.ContainsKey("clientDirectory")&&string.Equals(Convert.ToString(receipt["clientDirectory"]).TrimEnd('\\'),Path.GetFullPath(client).TrimEnd('\\'),StringComparison.OrdinalIgnoreCase)&&addon!=null&&addon.ContainsKey("ao-vk.dll")&&Convert.ToString(addon["ao-vk.dll"])==hash;
     if(result.GraphicsManaged&&receipt.ContainsKey("edition")){string installed=Convert.ToString(receipt["edition"]);if(installed=="lite"||installed=="full")result.Edition=installed;}
    }
    result.GraphicsCurrent=result.GraphicsManaged&&UpdateService.GraphicsCurrent(client,edition);
    string game=SetupService.GuardUnder(client,Path.Combine(client,"AnarchyOnline.exe"));using(var f=File.OpenRead(game))using(var reader=new BinaryReader(f)){if(f.Length>256&&reader.ReadUInt16()==0x5a4d){f.Position=60;int offset=reader.ReadInt32();if(offset>=64&&offset<f.Length-24){f.Position=offset;if(reader.ReadUInt32()==0x4550){f.Position=offset+22;result.MemoryEnabled=(reader.ReadUInt16()&32)!=0;}}}}
   }catch{result.Warning="Some graphics installation details could not be verified. Repair can check the files.";}
   return result;
  }
 }
}
