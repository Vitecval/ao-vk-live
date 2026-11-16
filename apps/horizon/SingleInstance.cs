using System;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Threading;
using System.Diagnostics;
using System.IO.MemoryMappedFiles;

namespace Horizon {
 internal sealed class SingleInstance:IDisposable {
  readonly Mutex gate;EventWaitHandle wake;RegisteredWaitHandle listener;MemoryMappedFile owner;bool owned;
  public static string ProductKey{get{return "Local\\RubiKaHorizon-"+WindowsIdentity.GetCurrent().User.Value;}}
  public SingleInstance(string key){Key=key;gate=new Mutex(false,key+"-instance");}
  public string Key{get;private set;}
  public bool Enter(){try{owned=gate.WaitOne(0);}catch(AbandonedMutexException){owned=true;}if(owned){owner=MemoryMappedFile.CreateOrOpen(Key+"-owner",4);using(var view=owner.CreateViewAccessor())view.Write(0,Process.GetCurrentProcess().Id);wake=new EventWaitHandle(false,EventResetMode.AutoReset,Key+"-activate");}return owned;}
  public void Listen(Action activate){if(!owned)throw new InvalidOperationException();listener=ThreadPool.RegisterWaitForSingleObject(wake,(s,timeout)=>activate(),null,Timeout.Infinite,false);}
  public bool Signal(){for(int i=0;i<30;i++){try{using(var e=EventWaitHandle.OpenExisting(Key+"-activate")){using(var map=MemoryMappedFile.OpenExisting(Key+"-owner",MemoryMappedFileRights.Read))using(var view=map.CreateViewAccessor(0,4,MemoryMappedFileAccess.Read))AllowSetForegroundWindow((uint)view.ReadInt32(0));e.Set();return true;}}catch(WaitHandleCannotBeOpenedException){Thread.Sleep(50);}catch(System.IO.FileNotFoundException){Thread.Sleep(50);}}return false;}
  public void Dispose(){if(listener!=null)listener.Unregister(null);if(wake!=null)wake.Dispose();if(owner!=null)owner.Dispose();if(owned){gate.ReleaseMutex();owned=false;}gate.Dispose();}
  [DllImport("user32.dll")]static extern bool AllowSetForegroundWindow(uint processId);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr FindWindow(string className,string title);
  [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
  [DllImport("user32.dll")]static extern bool ShowWindowAsync(IntPtr window,int command);
  [DllImport("user32.dll")]static extern bool SetForegroundWindow(IntPtr window);
  public static bool ActivateLegacy(){var window=FindWindow(null,"Rubi-Ka Horizon");if(window==IntPtr.Zero)return false;uint pid;GetWindowThreadProcessId(window,out pid);try{using(var p=Process.GetProcessById((int)pid)){if(FileVersionInfo.GetVersionInfo(p.MainModule.FileName).ProductName!="Rubi-Ka Horizon")return false;ShowWindowAsync(window,9);SetForegroundWindow(window);return true;}}catch{return false;}}
 }
}
