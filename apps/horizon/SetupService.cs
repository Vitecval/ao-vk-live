using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace Horizon {
 public static class SetupService {
  public const string Version="0.7.8";
  public static string PackageRoot=Path.Combine(App.StateRoot,"packages");
  static JavaScriptSerializer Json(){return new JavaScriptSerializer{MaxJsonLength=32000000};}
  public static string ExePath{get{return Assembly.GetExecutingAssembly().Location;}}
  public static string Hash(string path){using(var s=File.OpenRead(path))using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(s)).Replace("-","").ToLowerInvariant();}
  static string Hash(Stream s,long length){using(var h=SHA256.Create()){var b=new byte[65536];while(length>0){int n=s.Read(b,0,(int)Math.Min(length,b.Length));if(n==0)throw new EndOfStreamException();h.TransformBlock(b,0,n,b,0);length-=n;}h.TransformFinalBlock(new byte[0],0,0);return BitConverter.ToString(h.Hash).Replace("-","").ToLowerInvariant();}}
  static Dictionary<string,object> ReadObject(string path){return Json().Deserialize<Dictionary<string,object>>(File.ReadAllText(path));}
  static Dictionary<string,object> Map(object o){return (Dictionary<string,object>)o;}
  public static string GuardUnder(string root,string child){
   string parent=Path.GetFullPath(root).TrimEnd('\\')+"\\",full=Path.GetFullPath(child);
   if(!full.StartsWith(parent,StringComparison.OrdinalIgnoreCase))throw new IOException("The operation would leave Horizon's own data folder.");
   CheckDirectorySafety(Path.GetDirectoryName(full));
   if((File.Exists(full)||Directory.Exists(full))&&(File.GetAttributes(full)&FileAttributes.ReparsePoint)!=0)throw new IOException("A linked work file or folder was preserved. Choose the actual folder.");
   return full;
  }
  public static void CheckDirectorySafety(string path){for(var d=new DirectoryInfo(Path.GetFullPath(path));d!=null;d=d.Parent)if(d.Exists&&(d.Attributes&FileAttributes.ReparsePoint)!=0)throw new IOException("The folder passes through a link. Choose its actual location.");}
  static void DeleteOwned(string root,string child){
   string full=GuardUnder(root,child);if(!Directory.Exists(full))return;
   if((File.GetAttributes(full)&FileAttributes.ReparsePoint)!=0){Directory.Delete(full);return;}
   foreach(string f in Directory.GetFiles(full))File.Delete(f);
   foreach(string d in Directory.GetDirectories(full))DeleteOwned(root,d);
   Directory.Delete(full);
  }
  static void VerifyCore(string directory,Action<string,int> progress){
   // The index is compiled from the pinned archive. Only bootstrap files
   // belong here; companion-only assets are verified by the existing backend.
   var files=Json().Deserialize<Dictionary<string,string>>(App.ResourceText("core-files.json"));
   int checkedFiles=0;foreach(var p in files){
    if(checkedFiles%16==0)progress("Checking saved setup files: "+checkedFiles+" / "+files.Count,100*checkedFiles/Math.Max(1,files.Count));checkedFiles++;
    string path=GuardUnder(directory,Path.Combine(directory,p.Key.Replace('/',Path.DirectorySeparatorChar)));
    if(!File.Exists(path)||Hash(path)!=p.Value)throw new IOException("A setup file changed or is missing: "+p.Key+". Keep the original Horizon installer and try a fresh copy.");
   }
   foreach(string name in new[]{"ao-vk.dll","ddraw.dll","install-live.ps1","setup-safety.ps1","reset-settings.ps1","material-download.ps1"})if(!File.Exists(Path.Combine(directory,name)))throw new IOException("A required setup file is missing: "+name);
  }
  internal static string EnsureCore(Action<string,int> progress){
   using(var f=File.OpenRead(ExePath)){
    if(f.Length<56)throw new IOException("The setup payload is missing.");
    f.Position=f.Length-56;var reader=new BinaryReader(f,Encoding.UTF8,true);
    if(Encoding.ASCII.GetString(reader.ReadBytes(8))!="RKHCORE1")throw new IOException("Use the bundled Rubi-Ka Horizon Setup.exe to install or repair graphics.");
    long offset=reader.ReadInt64(),length=reader.ReadInt64();string expected=BitConverter.ToString(reader.ReadBytes(32)).Replace("-","").ToLowerInvariant();
    var pin=Json().Deserialize<Dictionary<string,object>>(App.ResourceText("engine.json"));
    if(expected!=(string)pin["fullSetupSha256"]||length!=Convert.ToInt64(pin["fullSetupBytes"]))throw new IOException("The bundled engine differs from the approved graphics package.");
    if(offset<1024||length<1024||offset>f.Length-56||length!=f.Length-56-offset)throw new IOException("The setup payload is incomplete. Download a fresh installer.");
    string engineRoot=Path.Combine(App.StateRoot,"engine");CheckDirectorySafety(engineRoot);string engine=GuardUnder(engineRoot,Path.Combine(engineRoot,"r59-"+expected.Substring(0,16)));
    if(Directory.Exists(engine)){VerifyCore(engine,progress);return engine;}
    progress("Checking the bundled graphics files…",-1);f.Position=offset;if(Hash(f,length)!=expected)throw new IOException("The installer payload failed its integrity check.");
    Directory.CreateDirectory(engineRoot);string stage=GuardUnder(engineRoot,Path.Combine(engineRoot,"staging-"+Guid.NewGuid().ToString("N")));Directory.CreateDirectory(stage);
    try{
     // Copy only the verified overlay, not the executable, into a temporary ZIP.
     string zip=Path.Combine(stage,"payload.zip");f.Position=offset;
     using(var z=File.Create(zip)){var b=new byte[65536];long remaining=length;while(remaining>0){int n=f.Read(b,0,(int)Math.Min(remaining,b.Length));if(n==0)throw new EndOfStreamException();z.Write(b,0,n);remaining-=n;}}
     using(var z=ZipFile.OpenRead(zip)){
      foreach(var e in z.Entries){if(e.FullName.EndsWith("/"))continue;var parts=e.FullName.Split('/');if(parts.Length<2||parts.Any(x=>x==".."||x.Contains(":")))throw new IOException("Unsafe path in setup payload.");
       string rel=string.Join("/",parts.Skip(1));if(rel.Length==0)continue;string target=GuardUnder(stage,Path.Combine(stage,rel.Replace('/',Path.DirectorySeparatorChar)));Directory.CreateDirectory(Path.GetDirectoryName(target));
       using(var input=e.Open())using(var output=File.Create(target))input.CopyTo(output);
      }
     }File.Delete(zip);VerifyCore(stage,progress);
     try{Directory.Move(stage,engine);}catch(IOException){if(!Directory.Exists(engine))throw;VerifyCore(engine,progress);DeleteOwned(engineRoot,stage);}
     return engine;
    }catch{if(Directory.Exists(stage))DeleteOwned(engineRoot,stage);throw;}
   }
  }
  static string PS(string text){return "'"+text.Replace("'","''")+"'";}
  internal static void RefreshCachedFile(string root,string target,string expected,Action<string> write){
   target=GuardUnder(root,target);Directory.CreateDirectory(Path.GetDirectoryName(target));if(File.Exists(target)&&Hash(target)==expected)return;
   string stage=GuardUnder(root,Path.Combine(Path.GetDirectoryName(target),".horizon-"+Guid.NewGuid().ToString("N")+".staging"));
   try{write(stage);if(Hash(stage)!=expected)throw new IOException("The refreshed setup file failed verification: "+Path.GetFileName(target));
    if(File.Exists(target)){string backup=GuardUnder(root,target+".previous-"+Guid.NewGuid().ToString("N"));File.Replace(stage,target,backup);}else File.Move(stage,target);
   }finally{if(File.Exists(stage))File.Delete(GuardUnder(root,stage));}
  }
  [DllImport("kernel32.dll",SetLastError=true)]static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool QueryFullProcessImageName(IntPtr process,int flags,StringBuilder name,ref int size);
  [DllImport("kernel32.dll")]static extern bool CloseHandle(IntPtr handle);
  internal static bool UsesSelectedFolder(string folder,string image){return !string.IsNullOrEmpty(image)&&Path.GetFullPath(image).StartsWith(Path.GetFullPath(folder).TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase);}
  public static void AssertSelectedClientClosed(string client){
   // A quick executable-path check avoids starting a lengthy Full repair while
   // this client is open. The 32-bit backend still checks shared loader modules
   // immediately before any transaction. No other client is controlled.
   foreach(string name in new[]{"AnarchyOnline","Anarchy","AnarchyPatcher","randy_harness"})foreach(var process in Process.GetProcessesByName(name))using(process){
    IntPtr handle=OpenProcess(0x1000,false,process.Id);if(handle==IntPtr.Zero)continue;
    try{var path=new StringBuilder(32768);int size=path.Capacity;if(QueryFullProcessImageName(handle,0,path,ref size)&&UsesSelectedFolder(client,path.ToString()))throw new IOException("Close the game and launcher for "+client+", then try again. Other AO installations can stay open.");}finally{CloseHandle(handle);}
   }
  }
  public static async Task Run(string action,string client,string edition,Action<string,int> progress,bool skipMemoryChange=false){
   if(!new[]{"Install","Update","Repair","Uninstall","Reset settings"}.Contains(action))throw new ArgumentException("Unknown setup action.");
   if(edition!="lite"&&edition!="full")throw new ArgumentException("Choose Lite or Full HD.");
   await Task.Run(()=>{
    string report=GuardUnder(App.StateRoot,Path.Combine(App.StateRoot,"last-setup.log"));
    CheckDirectorySafety(App.StateRoot);
    if(File.Exists(report))File.Copy(report,GuardUnder(App.StateRoot,Path.Combine(App.StateRoot,"previous-setup.log")),true);
    File.WriteAllText(report,DateTime.UtcNow.ToString("o")+" Starting "+action+" "+edition+" "+client+Environment.NewLine,Encoding.UTF8);
    Action<string,int> preparation=(message,percent)=>{File.AppendAllText(report,DateTime.UtcNow.ToString("o")+" "+message+Environment.NewLine);progress(message,percent);};
    try{
    preparation("Checking saved setup files…",-1);
    AssertSelectedClientClosed(client);
    string core=EnsureCore(preparation),taskRoot=Path.Combine(App.StateRoot,"tasks");Directory.CreateDirectory(taskRoot);
    string task=GuardUnder(taskRoot,Path.Combine(taskRoot,Guid.NewGuid().ToString("N")));Directory.CreateDirectory(task);
    CheckDirectorySafety(PackageRoot);Directory.CreateDirectory(PackageRoot);
    string package=GuardUnder(PackageRoot,Path.Combine(PackageRoot,"Rubi-Ka Horizon r59 "+edition));
    FileStream setupLock=null;
    try{
     setupLock=new FileStream(Path.Combine(PackageRoot,"horizon-setup.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
     string marker=GuardUnder(package,Path.Combine(package,".horizon-owned"));
     if(Directory.Exists(package)&&(!File.Exists(marker)||File.ReadAllText(marker)!="Rubi-Ka Horizon r59 "+edition))throw new IOException("The work folder already contains unrecognized files. Choose another download/work folder.");
     Directory.CreateDirectory(package);File.WriteAllText(marker,"Rubi-Ka Horizon r59 "+edition);
     preparation("Preparing "+(edition=="lite"?"Lite — no texture downloads":"Full HD — existing downloads will be reused"),-1);
     var coreHashes=Json().Deserialize<Dictionary<string,string>>(App.ResourceText("core-files.json"));
     string[] coreFiles=Directory.GetFiles(core,"*",SearchOption.AllDirectories);int preparedFiles=0;
     foreach(string file in coreFiles){
      if(preparedFiles%16==0)preparation("Preparing saved setup files: "+preparedFiles+" / "+coreFiles.Length,100*preparedFiles/Math.Max(1,coreFiles.Length));preparedFiles++;
      string rel=file.Substring(core.Length+1),target=GuardUnder(package,Path.Combine(package,rel));Directory.CreateDirectory(Path.GetDirectoryName(target));
      if(rel.Equals("manifest.json",StringComparison.OrdinalIgnoreCase)){
       byte[] manifestBytes=new UTF8Encoding(false).GetBytes(App.ResourceText(edition+"-manifest.json"));using(var manifestStream=new MemoryStream(manifestBytes)){string pin=Hash(manifestStream,manifestStream.Length);RefreshCachedFile(package,target,pin,p=>File.WriteAllBytes(p,manifestBytes));}continue;
      }
      string resource=rel.Equals("install-live.ps1",StringComparison.OrdinalIgnoreCase)?"Horizon.InstallLive.ps1":rel.Equals("material-cache.ps1",StringComparison.OrdinalIgnoreCase)?"Horizon.MaterialCache.ps1":null;
      if(resource!=null){
       // Installer resources are independently verified against the bundled
       // bootstrap. The legacy work bucket keeps existing ZIPs reusable.
       using(var source=Assembly.GetExecutingAssembly().GetManifestResourceStream(resource)){
        using(var data=new MemoryStream()){source.CopyTo(data);byte[] bytes=data.ToArray();data.Position=0;string expected=Hash(data,data.Length);
         RefreshCachedFile(package,target,expected,p=>File.WriteAllBytes(p,bytes));
        }
       }continue;
      }
      string expectedCore=coreHashes[rel.Replace('\\','/')];RefreshCachedFile(package,target,expectedCore,p=>File.Copy(file,p));
     }
     string script=Path.Combine(task,"operation.ps1"),installer=Path.Combine(package,action=="Reset settings"?"reset-settings.ps1":"install-live.ps1");
     string args=" -ClientDir "+PS(Path.GetFullPath(client));
     if(action!="Reset settings")args+=" -PackageDir "+PS(package)+(action=="Uninstall"?" -Uninstall":action=="Update"||action=="Repair"?" -Upgrade":"")+(action=="Repair"?" -VerifyAllAssets":"")+(skipMemoryChange?" -DisableLargeAddressAware":"");
     string body="$ErrorActionPreference='Stop'\r\n$ProgressPreference='SilentlyContinue'\r\ntry {\r\n$cb={param($message,$percent=-1,$detail=$null) [Console]::WriteLine('HORIZON|'+(@{message=[string]$message;percent=[int]$percent}|ConvertTo-Json -Compress))}\r\n& "+PS(installer)+args+" -ProgressCallback $cb | Out-Null\r\n[Console]::WriteLine('HORIZON_DONE')\r\nexit 0\r\n} catch {[Console]::Error.WriteLine($_.Exception.Message);exit 1}\r\n";
     File.WriteAllText(script,body,new UTF8Encoding(true));
     string host=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"SysWOW64","WindowsPowerShell","v1.0","powershell.exe");
     if(!File.Exists(host))throw new IOException("Horizon setup requires 64-bit Windows with Windows PowerShell.");
     var info=new ProcessStartInfo(host,"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+script+"\""){CreateNoWindow=true,UseShellExecute=false,WindowStyle=ProcessWindowStyle.Hidden,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=package};
     bool complete=false;var errors=new StringBuilder();var logLock=new object();string lastMessage="Starting setup…";int lastPercent=-1;DateTime lastUpdate=DateTime.UtcNow;
     preparation("Starting the installation worker…",-1);
     using(var log=new StreamWriter(report,true,Encoding.UTF8){AutoFlush=true}){
     log.WriteLine(DateTime.UtcNow.ToString("o")+" "+action+" "+edition+" "+client);
     using(var process=new Process{StartInfo=info}){
      process.OutputDataReceived+=(s,e)=>{if(e.Data==null)return;lock(logLock){log.WriteLine(DateTime.UtcNow.ToString("o")+" "+e.Data);if(e.Data=="HORIZON_DONE")complete=true;else if(e.Data.StartsWith("HORIZON|")){try{var v=Json().Deserialize<Dictionary<string,object>>(e.Data.Substring(8));lastMessage=(string)v["message"];lastPercent=Convert.ToInt32(v["percent"]);lastUpdate=DateTime.UtcNow;progress(lastMessage,lastPercent);}catch{}}}};
      process.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)lock(logLock){if(errors.Length<65536)errors.AppendLine(e.Data);log.WriteLine(DateTime.UtcNow.ToString("o")+" ERROR "+e.Data);}};
      process.Start();process.BeginOutputReadLine();process.BeginErrorReadLine();
      while(!process.WaitForExit(10000))lock(logLock){int seconds=(int)(DateTime.UtcNow-lastUpdate).TotalSeconds;if(seconds>=10)log.WriteLine(DateTime.UtcNow.ToString("o")+" Waiting for worker output: "+seconds+" seconds. Last step: "+lastMessage);}
      process.WaitForExit(); // Drain redirected output before checking completion.
      log.WriteLine(DateTime.UtcNow.ToString("o")+" Worker exit: "+process.ExitCode);
      if(process.ExitCode!=0||!complete)throw new IOException(errors.Length>0?errors.ToString().Trim():"Setup did not complete. See last-setup.log in Horizon's data folder.");
     }
     }
    }finally{
     if(setupLock!=null){
      // Keep archives and partial downloads so interruptions can resume. Only
      // expanded assets absent from the bundled core are temporary duplicates.
      if(edition=="full"&&Directory.Exists(package)){
       try{
        string[] temporary=App.ResourceText("temporary-assets.txt").Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries);int checkedTemporary=0;
        foreach(var p in temporary){
         if(checkedTemporary%512==0)preparation("Cleaning temporary setup files: "+checkedTemporary+" / "+temporary.Length+" (downloads kept)",100*checkedTemporary/Math.Max(1,temporary.Length));checkedTemporary++;
         string rel=p.Replace('/',Path.DirectorySeparatorChar);
         string file=Path.Combine(package,rel);if(File.Exists(file)){file=GuardUnder(package,file);File.Delete(file);}
        }
       }catch(Exception ex){File.AppendAllText(Path.Combine(App.StateRoot,"cleanup.log"),ex.Message+Environment.NewLine);}
      }setupLock.Dispose();
     }DeleteOwned(taskRoot,task);
    }
    }catch(Exception ex){File.AppendAllText(report,DateTime.UtcNow.ToString("o")+" ERROR "+ex.Message+Environment.NewLine);throw;}
   });
  }
  public static string InstallApp(){
   string directory=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","Rubi-Ka Horizon");
   string target=InstallAppAt(directory,Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs),"Rubi-Ka Horizon.lnk"),true);installedCurrent=true;return target;
  }
  internal static string InstallAppAt(string directory,string shortcut,bool register){
   CheckDirectorySafety(directory);Directory.CreateDirectory(directory);
   string target=Path.Combine(directory,"Rubi-Ka Horizon.exe");
   string record=Path.Combine(directory,"horizon-install.json"),sourceHash=Hash(ExePath),previous="",previousHash="";
   Dictionary<string,object> old=null;
   if(File.Exists(record)){old=ReadObject(record);if(!old.ContainsKey("product")||(string)old["product"]!="Rubi-Ka Horizon")throw new IOException("An unrecognized installation record was preserved.");if(old.ContainsKey("previousFile")){previous=(string)old["previousFile"];previousHash=(string)old["previousSha256"];}}
   if(File.Exists(target)&&Hash(target)!=sourceHash&&(old==null||!old.ContainsKey("sha256")||Hash(target)!=(string)old["sha256"]))throw new IOException("A different or modified application already uses this file name. It was preserved.");
   System.Version currentFile,newFile;if(File.Exists(target)&&System.Version.TryParse(FileVersionInfo.GetVersionInfo(target).FileVersion,out currentFile)&&System.Version.TryParse(FileVersionInfo.GetVersionInfo(ExePath).FileVersion,out newFile)&&currentFile>newFile)throw new IOException("A newer Horizon application is already installed. It was preserved.");
   if(!string.Equals(Path.GetFullPath(ExePath),Path.GetFullPath(target),StringComparison.OrdinalIgnoreCase)&&(!File.Exists(target)||Hash(target)!=sourceHash)){
    string stage=Path.Combine(directory,"Horizon-"+Guid.NewGuid().ToString("N")+".installing");File.Copy(ExePath,stage);
    try{if(Hash(stage)!=sourceHash)throw new IOException("The application copy could not be verified.");if(File.Exists(target)){previous="Horizon-previous-"+Guid.NewGuid().ToString("N")+".exe";previousHash=Hash(target);File.Replace(stage,target,Path.Combine(directory,previous));}else File.Move(stage,target);}finally{if(File.Exists(stage))File.Delete(stage);}
   }
   Type shell=Type.GetTypeFromProgID("WScript.Shell");dynamic instance=Activator.CreateInstance(shell);dynamic link=instance.CreateShortcut(shortcut);
   if(File.Exists(shortcut)&&!string.Equals((string)link.TargetPath,target,StringComparison.OrdinalIgnoreCase))throw new IOException("A different Start-menu shortcut uses this name. It was preserved; the app files are in "+directory+".");
   link.TargetPath=target;link.WorkingDirectory=directory;link.Description="Graphics and live settings for Anarchy Online";link.Save();
   var receipt=new Dictionary<string,object>{{"product","Rubi-Ka Horizon"},{"version",Version},{"sha256",sourceHash},{"previousFile",previous},{"previousSha256",previousHash},{"shortcut",shortcut},{"shortcutSha256",Hash(shortcut)}};
   string stagedRecord=record+"."+Guid.NewGuid().ToString("N")+".installing";File.WriteAllText(stagedRecord,Json().Serialize(receipt));if(File.Exists(record))File.Replace(stagedRecord,record,null);else File.Move(stagedRecord,record);
   if(old!=null&&old.ContainsKey("previousFile")){string oldName=(string)old["previousFile"];if(oldName.Length>0&&oldName!=previous&&oldName==Path.GetFileName(oldName)&&oldName.StartsWith("Horizon-previous-",StringComparison.Ordinal)){string oldPath=GuardUnder(directory,Path.Combine(directory,oldName));if(File.Exists(oldPath)&&Hash(oldPath)==(string)old["previousSha256"])File.Delete(oldPath);}}
   if(register)
   using(var key=Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\RubiKaHorizon")){
    key.SetValue("DisplayName","Rubi-Ka Horizon");key.SetValue("DisplayVersion",Version);key.SetValue("Publisher","AsyncStuff");key.SetValue("InstallLocation",directory);key.SetValue("DisplayIcon",target);key.SetValue("UninstallString","\""+target+"\" --uninstall-app");key.SetValue("NoModify",1);key.SetValue("NoRepair",1);
   }return target;
  }
  static bool? installedCurrent;
  static string InstalledExe{get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","Rubi-Ka Horizon","Rubi-Ka Horizon.exe");}}
  public static bool AppInstalled{get{if(string.Equals(Path.GetFullPath(ExePath),InstalledExe,StringComparison.OrdinalIgnoreCase))return true;if(!installedCurrent.HasValue){try{installedCurrent=File.Exists(InstalledExe)&&Hash(InstalledExe)==Hash(ExePath);}catch{installedCurrent=false;}}return installedCurrent.Value;}}
  public static void RemoveApp(){
   if(!string.Equals(Path.GetFullPath(ExePath),InstalledExe,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("This is a portable copy. Remove its EXE when finished; no application uninstall is needed.");
   string exe=ExePath,dir=Path.GetDirectoryName(exe),shortcut=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs),"Rubi-Ka Horizon.lnk");
   string record=Path.Combine(dir,"horizon-install.json");if(!File.Exists(record))throw new IOException("The application ownership record is missing. Files were preserved.");
   var receipt=ReadObject(record);if((string)receipt["product"]!="Rubi-Ka Horizon"||Hash(exe)!=(string)receipt["sha256"])throw new IOException("The application differs from its ownership record. Files were preserved.");
   using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\RubiKaHorizon")){
    if(key!=null&&!string.Equals(key.GetValue("InstallLocation") as string,dir,StringComparison.OrdinalIgnoreCase))throw new IOException("The registered installation differs. No files were removed.");
   }
   Registry.CurrentUser.DeleteSubKeyTree(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\RubiKaHorizon",false);
   // The running executable is removed after this process exits. No recursive
   // deletion: unknown/personal files and all game/data folders stay intact.
   var files=new Dictionary<string,string>{{exe,(string)receipt["sha256"]},{record,Hash(record)}};
   if((string)receipt["shortcut"]==shortcut)files[shortcut]=(string)receipt["shortcutSha256"];
   string previous=(string)receipt["previousFile"];if(previous.Length>0&&previous==Path.GetFileName(previous)&&previous.StartsWith("Horizon-previous-",StringComparison.Ordinal))files[GuardUnder(dir,Path.Combine(dir,previous))]=(string)receipt["previousSha256"];
   string command="$ErrorActionPreference='SilentlyContinue';Wait-Process -Id "+Process.GetCurrentProcess().Id+";";
   foreach(var p in files)command+="if((Test-Path -LiteralPath "+PS(p.Key)+") -and ((Get-FileHash -LiteralPath "+PS(p.Key)+" -Algorithm SHA256).Hash -ieq "+PS(p.Value)+")){Remove-Item -LiteralPath "+PS(p.Key)+" -Force};";
   command+="[IO.Directory]::Delete("+PS(dir)+",$false);";
   string encoded=Convert.ToBase64String(Encoding.Unicode.GetBytes(command));
   Process.Start(new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32","WindowsPowerShell","v1.0","powershell.exe"),"-NoProfile -NonInteractive -EncodedCommand "+encoded){CreateNoWindow=true,UseShellExecute=false,WindowStyle=ProcessWindowStyle.Hidden});
  }
 }
}
