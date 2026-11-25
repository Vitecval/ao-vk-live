using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace Horizon {
 public sealed class SettingsStore {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetPrivateProfileSection(string section,char[] buffer,uint size,string path);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool WritePrivateProfileString(string section,string key,string value,string path);
  public string DirectoryPath{get;private set;}public string IniPath{get;private set;}
  readonly Dictionary<string,Setting> definitions;
  readonly char[] profileBuffer=new char[262144];
  readonly object readLock=new object();
  readonly Stack<Change> history=new Stack<Change>();
  class Change {public Dictionary<string,string> Before,After;}
  public bool CanUndo{get{return history.Count>0;}}
  public SettingsStore(string directory,IEnumerable<Setting> settings){
   DirectoryPath=Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar);
   if(!Directory.Exists(DirectoryPath)||!File.Exists(Path.Combine(DirectoryPath,"AnarchyOnline.exe"))||!File.Exists(Path.Combine(DirectoryPath,"randy31.dll")))throw new InvalidOperationException("Choose the classic AO folder containing AnarchyOnline.exe and randy31.dll.");
   CheckLinks(DirectoryPath);IniPath=Path.Combine(DirectoryPath,"randy-vk.ini");
   if(File.Exists(IniPath)&&((File.GetAttributes(IniPath)&FileAttributes.ReparsePoint)!=0))throw new IOException("The settings file is a link. Choose the actual AO installation.");
   definitions=settings.ToDictionary(x=>x.key,StringComparer.OrdinalIgnoreCase);
  }
  static void CheckLinks(string path){for(var d=new DirectoryInfo(path);d!=null;d=d.Parent)if((d.Attributes&FileAttributes.ReparsePoint)!=0)throw new IOException("The selected folder passes through a link. Choose the actual AO folder.");}
  public Dictionary<string,string> Read(){return ReadSection("Renderer");}
  public Dictionary<string,string> ReadSection(string section){
   lock(readLock){
   if(File.Exists(IniPath))using(var check=new FileStream(IniPath,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){}
   var buffer=profileBuffer;uint n=GetPrivateProfileSection(section,buffer,(uint)buffer.Length,IniPath);
   if(n>=buffer.Length-2)throw new IOException("The settings section is too large to read safely.");
   var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
   foreach(var line in new string(buffer,0,(int)n).Split('\0')){int at=line.IndexOf('=');if(at<1)continue;string key=line.Substring(0,at).Trim();if(!result.ContainsKey(key))result[key]=line.Substring(at+1).Trim();}
   return result;
   }
  }
  public void Save(IDictionary<string,double> changes){
   if(changes.Count==0)return;
   var values=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
   foreach(var p in changes){Setting s;if(!definitions.TryGetValue(p.Key,out s))throw new ArgumentException("Unknown graphics option.");values.Add(s.key,s.Serialize(p.Value));}
   Write(values,true);
  }
  public void SaveDisplay(DisplayPreferences value){value.Validate();Write(value.Values(),false,null,"Display");}
  string MutexName(){using(var h=SHA256.Create())return "Local\\RubiKaHorizon.Settings."+BitConverter.ToString(h.ComputeHash(Encoding.UTF8.GetBytes(IniPath.ToUpperInvariant()))).Replace("-","");}
  void Write(Dictionary<string,string> values,bool remember,Dictionary<string,string> expected=null,string section="Renderer"){
   using(var mutex=new Mutex(false,MutexName())){
    bool held=false;
    try{
     try{held=mutex.WaitOne(3000);}catch(AbandonedMutexException){held=true;}
     if(!held)throw new IOException("Another Horizon window is saving settings. Try again shortly.");
     CheckLinks(DirectoryPath);
     var current=ReadSection(section);var before=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);var done=new List<string>();
     if(expected!=null)foreach(var p in expected){string now;current.TryGetValue(p.Key,out now);if(now!=p.Value)throw new IOException("These options were changed elsewhere. Refresh before undoing to preserve the newer settings.");}
     foreach(var p in values){string old;before[p.Key]=current.TryGetValue(p.Key,out old)?old:null;}
     try{
      foreach(var p in values){
       if(!WritePrivateProfileString(section,p.Key,p.Value,IniPath))throw new IOException("Windows could not save "+p.Key+". Check access to the AO folder.",new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()));
       done.Add(p.Key);
      }
      var after=ReadSection(section);foreach(var p in values){string actual;if(p.Value==null){if(after.ContainsKey(p.Key))throw new IOException("Windows could not remove the setting.");}else if(!after.TryGetValue(p.Key,out actual)||!string.Equals(actual,p.Value,StringComparison.Ordinal))throw new IOException("The setting changed while it was being saved. Refresh and try again.");}
     }catch{
      // Restore only our successful writes, never another writer's newer value.
      var latest=ReadSection(section);foreach(var key in done){string now;latest.TryGetValue(key,out now);if(now==values[key])WritePrivateProfileString(section,key,before[key],IniPath);}throw;
     }
     if(remember){history.Push(new Change{Before=before,After=values});while(history.Count>100){var keep=history.Take(100).Reverse().ToArray();history.Clear();foreach(var x in keep)history.Push(x);}}
    }finally{if(held)mutex.ReleaseMutex();}
   }
  }
  public void Undo(){
   if(history.Count==0)return;var c=history.Peek();var latest=Read();
   foreach(var p in c.After){string now;latest.TryGetValue(p.Key,out now);if(now!=p.Value)throw new IOException("These options were changed elsewhere. Refresh before undoing to preserve the newer settings.");}
   Write(c.Before,false,c.After);history.Pop();
  }
 }
}
