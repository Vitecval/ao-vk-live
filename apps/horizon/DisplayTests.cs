using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Imaging;

namespace Horizon {
 public static class DisplayTests {
  static void Check(bool condition,string name,List<string> checks){if(!condition)throw new Exception(name);checks.Add(name);}
  static List<DisplayMonitor> Monitors(){
   var first=new DisplayMonitor{Device=@"\\.\DISPLAY1",Label="Display 1 — 3840 × 2160"};
   first.Sizes.AddRange(new[]{new DisplaySize{Width=1920,Height=1080},new DisplaySize{Width=2560,Height=1440},new DisplaySize{Width=3840,Height=2160}});
   var second=new DisplayMonitor{Device=@"\\.\DISPLAY2",Label="Display 2 — 3440 × 1440"};second.Sizes.Add(new DisplaySize{Width=3440,Height=1440});
   return new List<DisplayMonitor>{new DisplayMonitor(),first,second};
  }
  static string Client(string root,string name){string path=Path.Combine(root,name);Directory.CreateDirectory(path);foreach(var file in new[]{"AnarchyOnline.exe","Anarchy.exe","randy31.dll","ao-vk.dll"})File.WriteAllText(Path.Combine(path,file),"isolated fixture, never loaded");return path;}
  static void Click(Button button){button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));}
  static void Capture(MainWindow window,string path){var visual=(FrameworkElement)window.Content;visual.Measure(new Size(window.Width,window.Height));visual.Arrange(new Rect(0,0,window.Width,window.Height));visual.UpdateLayout();var bitmap=new RenderTargetBitmap((int)window.Width,(int)window.Height,96,96,System.Windows.Media.PixelFormats.Pbgra32);bitmap.Render(visual);var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(bitmap));using(var file=File.Create(path))encoder.Save(file);}
  public static async Task Ui(string root){
   App.RenderingPreview=true;var checks=new List<string>();
   var actual=DisplayDiscovery.Read();Check(actual.Count>=1&&actual[0].Device=="","Actual read-only display discovery has Automatic fallback",checks);
   Check(actual.All(m=>m.Sizes.GroupBy(s=>s.Width+"x"+s.Height).All(g=>g.Count()==1)),"Actual mode enumeration deduplicates refresh variants by pixel size",checks);
   Check(actual.SelectMany(m=>m.Sizes).All(s=>s.Width>=320&&s.Height>=200&&s.Width<=16384&&s.Height<=16384),"Actual discovered sizes remain within display UI limits",checks);
   string first=Client(root,"AO display client one"),second=Client(root,"AO display client two");
   string ini=Path.Combine(first,"randy-vk.ini");File.WriteAllText(ini,"[Renderer]\r\nRVK_RenderScale=150\r\nRVK_ReworkSound=1\r\nFutureRendererKey=keep\r\n[OtherWrapper]\r\nUnknownValue=preserve me\r\n[Display]\r\nFutureDisplayKey=keep too\r\n");
   var catalog=Catalog.Load();foreach(var setting in catalog.settings)setting.Value=setting.defaultValue;
   new PresetStore(root).SaveNamed(first,"Display preservation",catalog);string presets=Path.Combine(root,"presets.json"),presetBefore=File.ReadAllText(presets);
   App.ClientArgument=first;var window=new MainWindow{Left=-3000,Top=-3000,ShowActivated=false,ShowInTaskbar=false};
   try{
    window.ReadDisplayMonitors=Monitors;window.SelectDisplayClient(first);window.Show();window.PreviewPage("Display");await Task.Delay(150);
    var mode=(ComboBox)window.FindName("DisplayMode");var monitor=(ComboBox)window.FindName("DisplayMonitorPicker");var resolution=(ComboBox)window.FindName("DisplayResolution");var save=(Button)window.FindName("SaveDisplayButton");var state=(TextBlock)window.FindName("DisplaySaveState");
    Check(mode.SelectedIndex==0&&!monitor.IsEnabled&&!resolution.IsEnabled&&!save.IsEnabled,"Actual Display page starts with Use game setting and no pending write",checks);
    Check(!File.ReadAllText(ini).Contains("Mode="),"Opening Display does not write defaults into the selected client",checks);
    mode.SelectedIndex=1;monitor.SelectedIndex=1;
    Check(resolution.Items.Cast<DisplaySize>().Select(s=>s.Width).SequenceEqual(new[]{0,1920,2560,3840}),"Windowed control offers actual selected-monitor sizes including 1080p, 1440p and 4K",checks);
    resolution.SelectedItem=resolution.Items.Cast<DisplaySize>().Single(s=>s.Width==3840);Click(save);
    var store=new SettingsStore(first,catalog.settings);var selected=DisplayPreferences.Read(store);
    Check(selected.Mode=="Windowed"&&selected.Width==3840&&selected.Height==2160&&selected.Monitor==@"\\.\DISPLAY1","Real Save control writes exact Windowed 4K values to the selected client's Display section",checks);
    Check(state.Text.Contains("Restart")&&!save.IsEnabled,"Saved output changes are clearly marked restart-required",checks);
    Check(store.Read()["RVK_RenderScale"]=="150"&&store.Read()["RVK_ReworkSound"]=="1"&&store.Read()["FutureRendererKey"]=="keep"&&store.ReadSection("OtherWrapper")["UnknownValue"]=="preserve me"&&store.ReadSection("Display")["FutureDisplayKey"]=="keep too","Display saves preserve RenderScale, Audio and unknown keys/sections",checks);
    Capture(window,Path.Combine(root,"display-windowed-4k.png"));
    monitor.SelectedIndex=2;Check(resolution.Items.Count==2&&((DisplaySize)resolution.Items[1]).Width==3440,"Changing monitors refreshes the resolution list without importing modes from another monitor",checks);
    resolution.SelectedIndex=1;Click(save);selected=DisplayPreferences.Read(store);
    Check(selected.Width==3440&&selected.Height==1440&&selected.Monitor==@"\\.\DISPLAY2","Actual controls save the supported ultrawide size and monitor identity",checks);
    mode.SelectedIndex=2;Check(!resolution.IsEnabled&&monitor.IsEnabled,"Borderless uses the selected monitor's desktop size instead of exposing a stretched resolution choice",checks);Click(save);
    selected=DisplayPreferences.Read(store);Check(selected.Mode=="Borderless"&&selected.Width==0&&selected.Height==0&&selected.Monitor==@"\\.\DISPLAY2","Borderless persists desktop sizing without a fake refresh-rate override",checks);
    Capture(window,Path.Combine(root,"display-borderless.png"));
    window.SelectDisplayClient(second);Check(mode.SelectedIndex==0&&monitor.SelectedIndex==0,"Changing the selected installation loads its own default display preferences",checks);
    Check(!File.Exists(Path.Combine(second,"randy-vk.ini")),"Viewing another installation does not copy the first client's choices",checks);
    mode.SelectedIndex=1;Click(save);var secondStore=new SettingsStore(second,catalog.settings);
    Check(DisplayPreferences.Read(secondStore).Mode=="Windowed"&&DisplayPreferences.Read(store).Mode=="Borderless","Saving the second client cannot change the first client's display preferences",checks);
    window.SelectDisplayClient(first);Check(mode.SelectedIndex==2&&((DisplayMonitor)monitor.SelectedItem).Device==@"\\.\DISPLAY2","Returning to the first installation restores its saved Borderless/monitor choices",checks);
    mode.SelectedIndex=0;Click(save);selected=DisplayPreferences.Read(store);Check(selected.Mode=="Game"&&selected.Width==0&&selected.Height==0&&selected.Monitor=="","Use game setting provides an explicit per-client recovery choice",checks);
    Check(File.ReadAllText(presets)==presetBefore,"Display choices do not rewrite named graphics presets",checks);
    store.SaveDisplay(new DisplayPreferences{Mode="Windowed",Monitor=@"\\.\DISPLAY99",Width=5120,Height=1440});window.SelectDisplayClient(first);
    Check(((DisplayMonitor)monitor.SelectedItem).Available==false&&!resolution.IsEnabled&&!save.IsEnabled,"A disconnected saved monitor is shown honestly and never silently replaced or saved",checks);
    mode.SelectedIndex=0;Click(save);Check(DisplayPreferences.Read(store).Mode=="Game"&&monitor.SelectedIndex==0,"Default game mode remains available when a saved monitor disappears and shows the saved Automatic choice",checks);
    window.Width=920;window.Height=610;Capture(window,Path.Combine(root,"display-minimum.png"));
    File.WriteAllText(Path.Combine(root,"DISPLAY-UI-RESULTS.json"),new JavaScriptSerializer().Serialize(new{result="passed",checks,actualDetectedMonitors=actual.Count-1,actualGameRenderingVerified=false,retailClientsChanged=false,desktopSettingsChanged=false,gameLaunched=false}));
   }finally{window.Close();}
  }
 }
}
