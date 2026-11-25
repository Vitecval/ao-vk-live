using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace Horizon {
 public partial class MainWindow {
  sealed class DisplayModeChoice {public string Value,Label;public override string ToString(){return Label;}}
  List<DisplayMonitor> displayMonitors;
  internal Func<List<DisplayMonitor>> ReadDisplayMonitors=DisplayDiscovery.Read;
  bool loadingDisplay,displayDirty,displayLoaded;
  void InitializeDisplay(){
   DisplayMode.ItemsSource=new[]{new DisplayModeChoice{Value="Game",Label="Use game setting (default)"},new DisplayModeChoice{Value="Windowed",Label="Windowed"},new DisplayModeChoice{Value="Borderless",Label="Borderless"}};
   DisplayMode.SelectedIndex=0;RefreshDisplayPreferences();
  }
  void RefreshDisplayPreferences(){
   loadingDisplay=true;displayLoaded=false;displayDirty=false;
   try{
    var value=store==null?new DisplayPreferences():DisplayPreferences.Read(store);
    displayMonitors=ReadDisplayMonitors();
    if(!displayMonitors.Any(x=>x.Device.Equals(value.Monitor,StringComparison.OrdinalIgnoreCase)))displayMonitors.Add(new DisplayMonitor{Device=value.Monitor,Label=value.Monitor+" (disconnected — choose another monitor)",Available=false});
    DisplayMode.SelectedItem=((IEnumerable<DisplayModeChoice>)DisplayMode.ItemsSource).Single(x=>x.Value==value.Mode);
    DisplayMonitorPicker.ItemsSource=displayMonitors;
    DisplayMonitorPicker.SelectedItem=displayMonitors.First(x=>x.Device.Equals(value.Monitor,StringComparison.OrdinalIgnoreCase));
    PopulateDisplaySizes(value.Width,value.Height);displayLoaded=true;
    DisplaySaveState.Text=store==null?"Choose an AO folder to edit its display settings.":"Saved choices for this AO folder. Changes take effect after restarting AO.";
   }catch(Exception ex){DisplaySaveState.Text="Could not read display settings: "+ex.Message;}
   finally{loadingDisplay=false;UpdateDisplayState();}
  }
  void PopulateDisplaySizes(int width,int height){
   var monitor=DisplayMonitorPicker.SelectedItem as DisplayMonitor;
   var mode=DisplayMode.SelectedItem as DisplayModeChoice;
   if(mode!=null&&mode.Value=="Borderless"){DisplayResolution.ItemsSource=new[]{new DisplaySize{Label="Current desktop size (borderless)"}};DisplayResolution.SelectedIndex=0;return;}
   // Automatic follows AO's window, which may be on another monitor.
   var sizes=new List<DisplaySize>{new DisplaySize()};
   if(monitor!=null&&monitor.Device.Length>0&&monitor.Available)sizes.AddRange(monitor.Sizes);
   var selected=sizes.FirstOrDefault(x=>x.Width==width&&x.Height==height);
   if(selected==null){selected=new DisplaySize{Width=width,Height=height,Available=false};sizes.Add(selected);}
   DisplayResolution.ItemsSource=sizes;DisplayResolution.SelectedItem=selected;
  }
  void UpdateDisplayState(){
   if(DisplayMode==null||DisplayMonitorPicker==null)return;
   var mode=DisplayMode.SelectedItem as DisplayModeChoice;var monitor=DisplayMonitorPicker.SelectedItem as DisplayMonitor;var size=DisplayResolution.SelectedItem as DisplaySize;
   bool edit=store!=null&&!busy&&displayLoaded,game=mode==null||mode.Value=="Game",borderless=mode!=null&&mode.Value=="Borderless";
   DisplayMode.IsEnabled=edit;DisplayMonitorPicker.IsEnabled=edit&&!game;
   DisplayResolution.IsEnabled=edit&&!game&&!borderless&&monitor!=null&&monitor.Device.Length>0&&monitor.Available;
   SaveDisplayButton.IsEnabled=edit&&displayDirty&&(game||(monitor!=null&&monitor.Available&&(borderless||(size!=null&&size.Available))));
   DisplayModeHelp.Text=game?"AO keeps control of its window and resolution. This is the safe default.":borderless?"Borderless fills the selected monitor at its current desktop resolution. Windows keeps its current refresh rate.":monitor==null||monitor.Device.Length==0?"Choose a specific monitor to select a supported window resolution, or keep automatic monitor and game sizing.":"Listed sizes are reported by Windows for this monitor. Use game setting keeps AO's requested size; very large windows may not fit the work area.";
  }
  void DisplayEdited(){if(loadingDisplay||!displayLoaded)return;displayDirty=true;DisplaySaveState.Text="Not saved. Save for this AO folder, then restart AO.";UpdateDisplayState();}
  void DisplayMode_Changed(object sender,SelectionChangedEventArgs e){if(loadingDisplay)return;loadingDisplay=true;try{PopulateDisplaySizes(0,0);}finally{loadingDisplay=false;}DisplayEdited();}
  void DisplayResolution_Changed(object sender,SelectionChangedEventArgs e){DisplayEdited();}
  void DisplayMonitor_Changed(object sender,SelectionChangedEventArgs e){if(loadingDisplay)return;loadingDisplay=true;try{PopulateDisplaySizes(0,0);}finally{loadingDisplay=false;}DisplayEdited();}
  void SaveDisplay_Click(object sender,RoutedEventArgs e){
   if(store==null||busy||!SaveDisplayButton.IsEnabled)return;
   try{
    var mode=(DisplayModeChoice)DisplayMode.SelectedItem;var monitor=DisplayMonitorPicker.SelectedItem as DisplayMonitor;var size=DisplayResolution.SelectedItem as DisplaySize;
    var value=new DisplayPreferences{Mode=mode.Value,Monitor=mode.Value=="Game"?"":monitor?.Device??"",Width=mode.Value=="Windowed"?size?.Width??0:0,Height=mode.Value=="Windowed"?size?.Height??0:0};
    store.SaveDisplay(value);
    if(value.Mode=="Game"){loadingDisplay=true;try{DisplayMonitorPicker.SelectedIndex=0;PopulateDisplaySizes(0,0);}finally{loadingDisplay=false;}}
    displayDirty=false;DisplaySaveState.Text="Saved for this AO folder. Restart AO and its launcher to apply.";
    Status("Display settings saved — restart AO.","Your desktop, graphics preset and other AO folders are unchanged.");UpdateDisplayState();
   }catch(Exception ex){DisplaySaveState.Text="Display settings could not be saved: "+ex.Message;}
  }
  void RefreshDisplays_Click(object sender,RoutedEventArgs e){if(!busy)RefreshDisplayPreferences();}
  internal void SelectDisplayClient(string folder){Choose(folder,false);}
 }
}
