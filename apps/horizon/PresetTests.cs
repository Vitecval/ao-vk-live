using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace Horizon {
 public static class PresetTests {
  static void Check(bool value,string message,List<string> checks){if(!value)throw new Exception(message);checks.Add(message);}
  static Setting Option(string key,double value=0){return new Setting{key=key,label=key,section="Test",type=0,min=0,max=1,defaultValue=value,Value=value};}
  static Catalog Definitions(params Setting[] settings){return new Catalog{settings=settings.ToList(),presets=new List<Preset>{new Preset{name="Balanced",values=new Dictionary<string,double>{{"A",1}}}}};}
  public static void Run(string root){
   var checks=new List<string>();var old=Definitions(Option("A",1),Option("Personal",1));var current=Definitions(Option("A",1),Option("Personal",1),Option("New",1),Option("Another"));string directory=Path.Combine(root,"named-presets"),client=Path.Combine(root,"client-one"),second=Path.Combine(root,"client-two");Directory.CreateDirectory(directory);
   var store=new PresetStore(directory);var custom=store.SaveNamed(client,"Night play",old);Check(custom.values.Count==2&&custom.values["A"]==1,"Named preset captures the current catalog values",checks);
   var reopened=new PresetStore(directory);Check(reopened.ActiveCustom(client).Name=="Night play"&&reopened.ActiveCustom(second)==null,"Named presets and selected client survive restart without selecting another client",checks);
   Check(reopened.Missing(client,current).SetEquals(new[]{"New","Another"}),"New catalog options remain unreviewed in an old custom preset",checks);
   Check(!reopened.Values(custom,current).ContainsKey("New"),"Applying an old custom preset does not write an unreviewed new default",checks);
   reopened.RecordEdits(client,new Dictionary<string,double>{{"New",0}});Check(reopened.Missing(client,current).SetEquals(new[]{"Another"})&&reopened.ActiveCustom(client).Values["New"]==0,"Explicitly choosing off reviews only that new option",checks);
   reopened=new PresetStore(directory);Check(!reopened.Missing(client,current).Contains("New")&&reopened.Missing(client,current).Contains("Another"),"Review choices persist while the other new option stays pending",checks);
   var copy=reopened.SaveNamed(client,"Night play copy",current);Check(!copy.values.ContainsKey("Another"),"Save as does not silently acknowledge an unreviewed new option",checks);
   string path=Path.Combine(directory,"presets.json"),before=File.ReadAllText(path);try{reopened.SaveNamed(second,"Night play",old);throw new Exception("Duplicate name accepted");}catch(ArgumentException){}Check(File.ReadAllText(path)==before,"Duplicate names cannot replace another preset",checks);
   try{reopened.SaveNamed(client,"Balanced",old);throw new Exception("Built-in name accepted");}catch(ArgumentException){}Check(File.ReadAllText(path)==before,"Custom names cannot replace built-in presets",checks);
   reopened.Select(second,old.presets[0],old);var defaults=reopened.NewBuiltInValues(second,current);Check(defaults.Count==2&&defaults["New"]==1&&defaults["Another"]==0&&!defaults.ContainsKey("Personal"),"Built-in upgrade supplies only new defaults and keeps existing personal choices",checks);
   reopened.AcknowledgeBuiltIn(second,current);Check(new PresetStore(directory).NewBuiltInValues(second,current).Count==0,"Built-in defaults are applied once across restarts",checks);
   var remembered=reopened.ActiveCustom(client).Values["A"];before=File.ReadAllText(path);using(var held=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)){try{reopened.RecordEdits(client,new Dictionary<string,double>{{"A",0}});throw new Exception("Locked preset file replaced");}catch(IOException){}}
   Check(File.ReadAllText(path)==before&&reopened.ActiveCustom(client).Values["A"]==remembered,"Failed atomic save preserves both on-disk and in-memory preset values",checks);
   Check(File.Exists(path+".backup")&&new JavaScriptSerializer().Deserialize<PresetData>(File.ReadAllText(path+".backup")).Format==1,"Preset replacement keeps a readable previous file",checks);
   string corrupt=Path.Combine(root,"corrupt-presets");Directory.CreateDirectory(corrupt);File.WriteAllText(Path.Combine(corrupt,"presets.json"),"invalid-json");try{new PresetStore(corrupt);throw new Exception("Corrupt presets accepted");}catch(IOException){}Check(File.ReadAllText(Path.Combine(corrupt,"presets.json"))=="invalid-json","Unreadable preset files are preserved instead of overwritten",checks);
   File.WriteAllText(Path.Combine(root,"PRESET-TEST-RESULTS.json"),new JavaScriptSerializer().Serialize(new{result="passed",checks,retailClientsChanged=false}));
  }
  static IEnumerable<DependencyObject> Descendants(DependencyObject root){for(int i=0;i<VisualTreeHelper.GetChildrenCount(root);i++){var child=VisualTreeHelper.GetChild(root,i);yield return child;foreach(var nested in Descendants(child))yield return nested;}}
  static void Capture(MainWindow window,string path){var visual=(FrameworkElement)window.Content;visual.Measure(new Size(window.Width,window.Height));visual.Arrange(new Rect(0,0,window.Width,window.Height));visual.UpdateLayout();var bitmap=new System.Windows.Media.Imaging.RenderTargetBitmap((int)window.Width,(int)window.Height,96,96,PixelFormats.Pbgra32);bitmap.Render(visual);var encoder=new System.Windows.Media.Imaging.PngBitmapEncoder();encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bitmap));using(var file=File.Create(path))encoder.Save(file);}
  public static async Task Ui(string root){
   var checks=new List<string>();App.RenderingPreview=true;string client=Path.Combine(root,"AO preset UI fixture");Directory.CreateDirectory(client);foreach(var name in new[]{"AnarchyOnline.exe","Anarchy.exe","randy31.dll","ao-vk.dll"})File.WriteAllText(Path.Combine(client,name),"dummy never loaded");App.ClientArgument=client;
   var catalog=Catalog.Load();foreach(var s in catalog.settings)s.Value=s.defaultValue;var old=new Catalog{settings=catalog.settings.Where(s=>s.key!="RVK_VividTone").ToList(),presets=catalog.presets};new PresetStore(App.StateRoot).SaveNamed(client,"UI custom",old);
   var window=new MainWindow{Left=-3000,Top=-3000,ShowActivated=false,ShowInTaskbar=false};try{window.Show();await Task.Delay(150);window.PreviewPage("Graphics");var filter=(ComboBox)window.FindName("SectionFilter");filter.SelectedItem="New options";window.UpdateLayout();await Task.Delay(50);var rows=Descendants(window).OfType<SettingRow>().ToList();var vivid=rows.FirstOrDefault(r=>(r.DataContext as Setting)?.key=="RVK_VividTone");Check(vivid!=null,"New options filter realizes the pending graphics row",checks);Check(((Setting)vivid.DataContext).NeedsReview&&Descendants(vivid).OfType<TextBlock>().Any(t=>t.Foreground==System.Windows.Media.Brushes.Salmon),"New option is visible and marked red in the real graphics row",checks);
    Capture(window,Path.Combine(root,"new-options.png"));var keep=Descendants(vivid).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Keep current value");keep.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));await Task.Delay(600);Check(!((Setting)vivid.DataContext).NeedsReview&&!new PresetStore(App.StateRoot).Missing(client,catalog).Contains("RVK_VividTone"),"Keep current value saves and clears the red mark in the real UI",checks);Check(Descendants(vivid).OfType<TextBlock>().Any(t=>t.Text==((Setting)vivid.DataContext).Title&&((SolidColorBrush)t.Foreground).Color==Color.FromRgb(231,235,235)),"Reviewed option restores the readable normal text colour",checks);
    window.SetUpdateNotice(true,"Horizon test update is available.");window.UpdateLayout();Check(((FrameworkElement)window.FindName("UpdateNotice")).Visibility==Visibility.Visible,"Available update notice is visible on Graphics",checks);var updateLink=Descendants(window.FindName("UpdateNotice") as DependencyObject).OfType<Button>().Single();updateLink.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));Check(((FrameworkElement)window.FindName("UpdatesPage")).Visibility==Visibility.Visible,"Update notice link opens Updates",checks);
    Capture(window,Path.Combine(root,"updates-notice.png"));window.PreviewPage("Patch notes");Check(((FrameworkElement)window.FindName("PatchNotesPage")).Visibility==Visibility.Visible&&((TextBlock)window.FindName("PatchNotesText")).Text.Contains("0.7.5"),"Patch notes page contains this release",checks);Capture(window,Path.Combine(root,"patch-notes.png"));
    window.PreviewPage("Suggestions");window.UpdateLayout();await Task.Delay(50);((TextBox)window.FindName("SuggestionTitle")).Text="Test feature idea";((TextBox)window.FindName("SuggestionDescription")).Text="A prepared suggestion, never transmitted.";var prepare=Descendants(window.FindName("SuggestionsPage") as DependencyObject).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Prepare suggestion");prepare.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));string preview=((TextBox)window.FindName("SuggestionPreview")).Text;Check(preview.Contains("never transmitted")&&!preview.Contains("--- horizon-")&&((Button)window.FindName("SendSuggestionButton")).IsEnabled,"Suggestions prepare reviewable text without collecting logs or sending it",checks);
    Capture(window,Path.Combine(root,"suggestions.png"));window.PreviewPage("Graphics");filter.SelectedItem="Common settings";window.Width=920;window.Height=610;Capture(window,Path.Combine(root,"graphics-minimum.png"));window.WindowState=WindowState.Minimized;await Task.Delay(100);Check(window.TrayLaunchEnabled,"Tray menu includes Launch Anarchy Online for the selected ready client",checks);
    File.WriteAllText(Path.Combine(root,"FEATURE-UI-RESULTS.json"),new JavaScriptSerializer().Serialize(new{result="passed",checks,suggestionsSent=false,gameLaunched=false,retailClientsChanged=false}));
   }finally{window.Close();}
  }
 }
}
