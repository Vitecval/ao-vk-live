using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;

namespace Horizon {
 public partial class MainWindow {
  internal bool MinimizeToTrayEnabled { get; private set; }

  void LoadTrayPreference(IDictionary<string,string> values){
   string value;
   MinimizeToTrayEnabled=values!=null&&values.TryGetValue("minimizeToTray",out value)&&string.Equals(value,"true",StringComparison.OrdinalIgnoreCase);
   RefreshTrayPreference();
  }
  void RefreshTrayPreference(){
   MinimizeToTrayOption.IsChecked=MinimizeToTrayEnabled;
   MinimizeWindowButton.ToolTip=MinimizeToTrayEnabled?"Minimize to system tray":"Minimize";
   TrayPreferenceDetail.Text=MinimizeToTrayEnabled?"Minimizing hides Horizon in the notification area. Click its icon to reopen. Closing still exits Horizon.":"Off by default. Minimizing keeps Horizon on the taskbar. Closing still exits Horizon.";
  }
  void MinimizeToTray_Click(object sender,RoutedEventArgs e){
   bool previous=MinimizeToTrayEnabled;
   MinimizeToTrayEnabled=MinimizeToTrayOption.IsChecked==true;
   try{SavePreferences();}
   catch(Exception ex){MinimizeToTrayEnabled=previous;RefreshTrayPreference();TrayPreferenceDetail.Text="This choice could not be saved. "+ex.Message;return;}
   RefreshTrayPreference();
   if(!MinimizeToTrayEnabled&&TrayVisible)RestoreFromTray();
  }
  void HandleTrayWindowStateChanged(){
   if(WindowState==WindowState.Minimized){if(MinimizeToTrayEnabled)HideInTray();}
   else{trayRestoreState=WindowState;if(trayIcon!=null)trayIcon.Visible=false;}
  }

  internal static async Task TrayPreferenceChecks(string root){
   bool oldPreview=App.RenderingPreview;string oldClient=App.ClientArgument,oldTicket=App.UpdateTicket;
   string oldRoot=App.StateRoot,oldPackage=SetupService.PackageRoot;var oldShutdown=Application.Current.ShutdownMode;
   string path=Path.Combine(root,"preferences.json");bool hadPreferences=File.Exists(path);byte[] original=hadPreferences?File.ReadAllBytes(path):null;
   var checks=new List<string>();var json=new JavaScriptSerializer();MainWindow window=null;
   Action<bool,string> check=(condition,label)=>{if(!condition)throw new Exception(label);checks.Add(label);};
   Func<MainWindow> open=()=>new MainWindow{Left=-3000,Top=-3000,WindowStartupLocation=WindowStartupLocation.Manual,ShowActivated=false,ShowInTaskbar=true};
   try{
    App.RenderingPreview=true;App.ClientArgument="";App.UpdateTicket="";App.StateRoot=root;
    Application.Current.ShutdownMode=ShutdownMode.OnExplicitShutdown;
    File.WriteAllText(path,json.Serialize(new Dictionary<string,string>{{"client",""},{"work",SetupService.PackageRoot},{"autoUpdates","false"}}));
    window=open();window.Show();await Task.Delay(100);
    check(!window.MinimizeToTrayEnabled&&window.MinimizeToTrayOption.IsChecked!=true,"Existing preferences without a tray key default to off");
    check((string)window.MinimizeWindowButton.ToolTip=="Minimize","Default minimize button describes taskbar behavior");
    window.WindowState=WindowState.Minimized;await Task.Delay(100);
    check(window.IsVisible&&window.ShowInTaskbar&&!window.TrayVisible&&window.WindowState==WindowState.Minimized,"Default minimize keeps the window on the taskbar and creates no tray icon");
    window.WindowState=WindowState.Normal;await Task.Delay(100);
    window.MinimizeToTrayOption.IsChecked=true;window.MinimizeToTray_Click(null,null);
    check(window.MinimizeToTrayEnabled&&json.Deserialize<Dictionary<string,string>>(File.ReadAllText(path))["minimizeToTray"]=="true","About toggle explicitly enables and saves tray minimization");
    for(int cycle=0;cycle<3;cycle++){
     window.WindowState=WindowState.Minimized;await Task.Delay(100);
     check(!window.IsVisible&&window.TrayVisible,"Opt-in minimize hides the window and registers its tray icon, cycle "+cycle);
     window.RestoreFromTray();await Task.Delay(100);
     check(window.IsVisible&&window.WindowState==WindowState.Normal&&!window.TrayVisible,"Tray restore reopens the existing window, cycle "+cycle);
    }
    window.Close();window=null;
    window=open();window.Show();await Task.Delay(100);
    check(window.MinimizeToTrayEnabled&&window.MinimizeToTrayOption.IsChecked==true,"Saved opt-in survives reopening Horizon");
    window.WindowState=WindowState.Minimized;await Task.Delay(100);
    window.MinimizeToTrayOption.IsChecked=false;window.MinimizeToTray_Click(null,null);await Task.Delay(100);
    check(!window.MinimizeToTrayEnabled&&window.IsVisible&&!window.TrayVisible&&window.WindowState==WindowState.Normal,"Disabling tray behavior restores a hidden window immediately");
    check(json.Deserialize<Dictionary<string,string>>(File.ReadAllText(path))["minimizeToTray"]=="false","Opt-out is saved");
    using(var locked=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)){
     window.MinimizeToTrayOption.IsChecked=true;window.MinimizeToTray_Click(null,null);
     check(!window.MinimizeToTrayEnabled&&window.MinimizeToTrayOption.IsChecked!=true,"Failed preference save rolls back the displayed and effective choice");
    }
    window.Close();window=null;
    window=open();window.Show();await Task.Delay(100);
    check(!window.MinimizeToTrayEnabled,"Saved opt-out survives reopening Horizon");
    window.LoadTrayPreference(new Dictionary<string,string>{{"minimizeToTray","unrecognized"}});
    check(!window.MinimizeToTrayEnabled,"Malformed legacy preference safely defaults to off");
    window.LoadTrayPreference(new Dictionary<string,string>());
    check(!window.MinimizeToTrayEnabled,"Missing tray preference defaults to off independently of old behavior");
    File.WriteAllText(Path.Combine(root,"TRAY-TEST-RESULTS.json"),json.Serialize(new{result="passed",checks,minimizeRestoreCycles=3,retailClientsChanged=false}));
   }finally{
    if(window!=null)window.Close();
    if(hadPreferences)File.WriteAllBytes(path,original);else if(File.Exists(path))File.Delete(path);
    App.RenderingPreview=oldPreview;App.ClientArgument=oldClient;App.UpdateTicket=oldTicket;
    App.StateRoot=oldRoot;SetupService.PackageRoot=oldPackage;Application.Current.ShutdownMode=oldShutdown;
   }
  }
 }
}
