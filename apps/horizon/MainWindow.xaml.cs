using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace Horizon {
 public partial class MainWindow:Window {
  readonly Catalog catalog;
  SettingsStore store;
  readonly Dictionary<string,double> pending=new Dictionary<string,double>(StringComparer.OrdinalIgnoreCase);
  readonly DispatcherTimer saveTimer=new DispatcherTimer{Interval=TimeSpan.FromMilliseconds(350)};
  readonly DispatcherTimer refreshTimer=new DispatcherTimer{Interval=TimeSpan.FromSeconds(2)};
  readonly Dictionary<string,Button> nav=new Dictionary<string,Button>();
  string client="";bool busy,ready;
  readonly string preferences=Path.Combine(App.StateRoot,"preferences.json");
  public MainWindow(){
   InitializeComponent();App.Trace("Interface loaded");catalog=Catalog.Load();App.Trace("Settings catalogue loaded");
   foreach(var s in catalog.settings)s.Value=s.defaultValue;
   AddNav("Graphics","\uE713");AddNav("Installation","\uE896");AddNav("About","\uE946");
   SectionFilter.Items.Add("Common settings");SectionFilter.Items.Add("All settings");foreach(var section in catalog.settings.Select(x=>x.section).Distinct().OrderBy(x=>x))SectionFilter.Items.Add(section);SectionFilter.SelectedIndex=0;
   PresetPicker.ItemsSource=catalog.presets;PresetPicker.SelectedIndex=-1;
   Search.ToolTip="Search "+catalog.settings.Count+" graphics settings";
   saveTimer.Tick+=(s,e)=>{saveTimer.Stop();SavePending();};refreshTimer.Tick+=(s,e)=>{if(!busy){RefreshSettings(false);UpdateState();}};
   string chosen=App.ClientArgument;
   if(File.Exists(preferences)){try{var p=new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(File.ReadAllText(preferences));if(chosen.Length==0&&p.ContainsKey("client"))chosen=p["client"];if(p.ContainsKey("work"))SetupService.PackageRoot=p["work"];}catch{}}
   WorkFolder.Text=SetupService.PackageRoot;
   ready=true;App.Trace("Reading selected AO folder");if(chosen.Length>0)Choose(chosen,false);else UpdateState();App.Trace("AO folder ready");
   ShowPage(chosen.Length==0?"Installation":"Graphics");Filter();refreshTimer.Start();
   Closing+=(s,e)=>{if(App.RenderingPreview)return;if(busy){e.Cancel=true;MessageBox.Show(this,"Setup is still working. Keep Horizon open until it finishes so it can verify or recover the operation.","Setup in progress",MessageBoxButton.OK,MessageBoxImage.Information);}else if(!SavePending()){e.Cancel=MessageBox.Show(this,"The last changes could not be saved. Close and discard only those unsaved changes?","Unsaved changes",MessageBoxButton.YesNo,MessageBoxImage.Question)!=MessageBoxResult.Yes;if(!e.Cancel)pending.Clear();}};
   Closed+=(s,e)=>{saveTimer.Stop();refreshTimer.Stop();};
  }
  void AddNav(string title,string icon){
   var line=new StackPanel{Orientation=Orientation.Horizontal};line.Children.Add(new TextBlock{Text=icon,FontFamily=new FontFamily("Segoe MDL2 Assets"),FontSize=15,Width=30,VerticalAlignment=VerticalAlignment.Center});line.Children.Add(new TextBlock{Text=title,FontSize=13});
   var button=new Button{Content=line,HorizontalContentAlignment=HorizontalAlignment.Left,Padding=new Thickness(12,11,12,11),Margin=new Thickness(0,0,0,7),BorderBrush=Brushes.Transparent,Background=Brushes.Transparent};button.Click+=(s,e)=>ShowPage(title);Navigation.Children.Add(button);nav.Add(title,button);
  }
  void ShowPage(string page){
   GraphicsPage.Visibility=page=="Graphics"?Visibility.Visible:Visibility.Collapsed;
   InstallationPage.Visibility=page=="Installation"?Visibility.Visible:Visibility.Collapsed;
   AboutPage.Visibility=page=="About"?Visibility.Visible:Visibility.Collapsed;
   foreach(var b in nav)b.Value.Background=b.Key==page?new SolidColorBrush(Color.FromRgb(37,58,65)):Brushes.Transparent;
   PageTitle.Text=page;
   PageSubtitle.Text=page=="Graphics"?"Changes apply while AO is running.":page=="Installation"?"Manage the selected AO installation.":"Application details.";
  }
  public void PreviewPage(string page){if(nav.ContainsKey(page))ShowPage(page);}
  public void PreviewBusy(){busy=true;UpdateState();Status("Checking installed files…","Full HD repair verifies the artwork and cache. This can take several minutes.");}
  void Choose(string folder,bool save){
   if(!SavePending())return;
   try{
    var next=new SettingsStore(folder,catalog.settings);client=next.DirectoryPath;store=next;pending.Clear();
    if(save)SavePreferences();
    FolderPath.Text=client;SidebarClient.Text=client;
    RefreshSettings(true);Status("Settings ready.","Saved changes are picked up by AO, usually within half a second.");
    var receipt=Path.Combine(client,"ao-vk-live.install.json");if(File.Exists(receipt)){try{var m=new JavaScriptSerializer{MaxJsonLength=32000000}.Deserialize<Dictionary<string,object>>(File.ReadAllText(receipt));if(m.ContainsKey("edition")&&(string)m["edition"]=="full")FullChoice.IsChecked=true;}catch{}}
   }catch(Exception ex){Status("Could not open this AO installation.",ex.Message);}
   UpdateState();
  }
  void UpdateState(){
   bool has=store!=null,installed=has&&File.Exists(Path.Combine(client,"ao-vk.dll"));
   GraphicsPage.IsEnabled=installed&&!busy;InstallationPage.IsEnabled=!busy;LaunchButton.IsEnabled=has&&!busy;InstallButton.IsEnabled=has&&!busy;
   InstallButton.Content=SetupService.AppInstalled?"Install / update graphics":"Install Horizon + graphics";
   UndoButton.IsEnabled=store!=null&&store.CanUndo&&!busy;
   if(has&&!installed&&!busy)SaveDetail.Text="Install graphics from the Installation page to use live settings.";
  }
  void RefreshSettings(bool force){
   if(store==null)return;
   try{var values=store.Read();foreach(var setting in catalog.settings){if(pending.ContainsKey(setting.key))continue;string v;double n=setting.defaultValue;if(values.TryGetValue(setting.key,out v)){try{n=setting.Normalize(setting.Parse(v));}catch{}}
     if(force||Math.Abs(setting.Value-n)>.000001)setting.Value=n;
    }}catch(Exception ex){if(force)Status("Could not read graphics settings.",ex.Message);}
  }
  public void Change(Setting setting,double value){
   if(store==null||busy)return;
   try{value=setting.Normalize(value);setting.Value=value;pending[setting.key]=value;Status("Saving your changes…","Only this AO installation will change.");saveTimer.Stop();saveTimer.Start();}
   catch(Exception ex){Status("Check this setting.",ex.Message);}
  }
  bool SavePending(){
   if(pending.Count==0||store==null)return true;
   try{store.Save(pending);int count=pending.Count;pending.Clear();Status(count==1?"Setting saved.":count+" settings saved.","AO reads saved changes automatically, including while it's in the background.");UpdateState();return true;}
   catch(Exception ex){Status("The change could not be saved.",ex.Message+" Your other settings are preserved.");return false;}
  }
  public void Status(string title,string detail){SaveStatus.Text=title;SaveDetail.Text=detail;}
  void Filter(){if(!ready||catalog==null)return;string text=Search.Text.Trim(),section=SectionFilter.SelectedItem as string??"Common settings";
   var result=catalog.settings.Where(x=>(section=="Common settings"?x.Quick:section=="All settings"||x.section==section)&&
    (text.Length==0||(x.Title+" "+x.label+" "+x.section+" "+x.key).IndexOf(text,StringComparison.OrdinalIgnoreCase)>=0)).ToList();
   // Searching always reaches every setting, independent of the recommended list.
   if(text.Length>0&&section=="Common settings")result=catalog.settings.Where(x=>(x.Title+" "+x.label+" "+x.section+" "+x.key).IndexOf(text,StringComparison.OrdinalIgnoreCase)>=0).ToList();
   SettingsList.ItemsSource=result;
  }
  void Filter_Changed(object sender,TextChangedEventArgs e){Filter();}void Section_Changed(object sender,SelectionChangedEventArgs e){Filter();}
  void Graphics_Click(object s,RoutedEventArgs e){ShowPage("Graphics");}void Installation_Click(object s,RoutedEventArgs e){ShowPage("Installation");}
  void Browse_Click(object s,RoutedEventArgs e){if(busy)return;using(var dialog=new System.Windows.Forms.FolderBrowserDialog{Description="Choose the classic Anarchy Online folder",SelectedPath=client,ShowNewFolderButton=false})if(dialog.ShowDialog()==System.Windows.Forms.DialogResult.OK)Choose(dialog.SelectedPath,true);}
  void Refresh_Click(object s,RoutedEventArgs e){if(SavePending()){RefreshSettings(true);Status("Settings refreshed.","Changes made in the game are shown here too.");}}
  void Preset_Click(object s,RoutedEventArgs e){if(store==null||busy||!SavePending())return;var p=PresetPicker.SelectedItem as Preset;if(p==null)return;try{store.Save(p.values);RefreshSettings(true);Status(p.name+" preset saved.","Personal options excluded by this preset are preserved.");UpdateState();}catch(Exception ex){Status("Preset could not be saved.",ex.Message);}}
  void Undo_Click(object s,RoutedEventArgs e){if(store==null||busy||!SavePending())return;try{store.Undo();RefreshSettings(true);Status("Previous settings restored.","AO will pick up the restored values automatically.");UpdateState();}catch(Exception ex){Status("Undo paused to preserve newer settings.",ex.Message);}}
  async Task Operation(string action){
   if(busy)return;if(store==null){Status("Choose an AO folder first.","Use Change folder or Browse on the Installation page.");return;}if(!SavePending())return;
   try{SetupService.AssertSelectedClientClosed(client);}catch(Exception ex){Status("Close this AO client before setup.",ex.Message);return;}
   if((action=="Uninstall"||action=="Reset settings")&&MessageBox.Show(this,action=="Uninstall"?"Remove the graphics add-on from this AO folder? Personal files and preferences are kept.":"Reset graphics preferences to their defaults? The current settings file is backed up first. Close AO and its launcher before continuing.",action=="Uninstall"?"Remove graphics":"Reset settings",MessageBoxButton.OKCancel,MessageBoxImage.Question)!=MessageBoxResult.OK)return;
   busy=true;ShowPage("Installation");SetupProgress.Visibility=Visibility.Visible;SetupProgress.IsIndeterminate=true;UpdateState();
   OperationText.Text=action=="Repair"?"Checking the installed files. Full HD includes the artwork and prepared cache; this can take several minutes.":"Preparing setup…";
   Status(action=="Repair"?"Repair in progress…":"Setup in progress…","Settings are temporarily locked while setup checks this installation.");
   try{
    SavePreferences();
    if((action=="Install"||action=="Update")&&!SetupService.AppInstalled){OperationText.Text="Installing the Horizon application…";await Task.Run(()=>SetupService.InstallApp());}
    await SetupService.Run(action,client,FullChoice.IsChecked==true?"full":"lite",(message,percent)=>Dispatcher.BeginInvoke(new Action(()=>{OperationText.Text=message;SetupProgress.IsIndeterminate=percent<0;if(percent>=0)SetupProgress.Value=Math.Max(0,Math.Min(100,percent));})),MemoryOption.IsChecked!=true);
    OperationText.Text=action=="Uninstall"?"Graphics removed. Your preferences and personal files were preserved.":action=="Reset settings"?"Settings reset. Defaults will be created when AO starts again.":"Installation finished. You can launch AO from the sidebar.";
    if(action=="Reset settings")store=new SettingsStore(client,catalog.settings);RefreshSettings(true);Status("Finished.",OperationText.Text);
   }catch(Exception ex){OperationText.Text=ex.Message;Status("Setup did not finish.","Your installation remains protected by the existing recovery checks. See the setup log for details.");MessageBox.Show(this,ex.Message,"Horizon setup",MessageBoxButton.OK,MessageBoxImage.Information);}
   finally{busy=false;SetupProgress.IsIndeterminate=false;SetupProgress.Visibility=Visibility.Collapsed;UpdateState();}
  }
  async void Install_Click(object s,RoutedEventArgs e){await Operation(File.Exists(Path.Combine(client,"ao-vk-live.install.json"))?"Update":"Install");}
  async void Repair_Click(object s,RoutedEventArgs e){await Operation("Repair");}async void Reset_Click(object s,RoutedEventArgs e){await Operation("Reset settings");}async void Uninstall_Click(object s,RoutedEventArgs e){await Operation("Uninstall");}
  async void InstallApp_Click(object s,RoutedEventArgs e){if(busy)return;busy=true;UpdateState();try{SavePreferences();var target=await Task.Run(()=>SetupService.InstallApp());Status("Horizon added to your Start menu.",target);}catch(Exception ex){Status("The app could not be installed.",ex.Message);}finally{busy=false;UpdateState();}}
  void Launch_Click(object s,RoutedEventArgs e){if(store==null||busy||!SavePending())return;try{Process.Start(new ProcessStartInfo(Path.Combine(client,"Anarchy.exe")){WorkingDirectory=client,UseShellExecute=true});}catch(Exception ex){Status("AO could not be launched.",ex.Message);}}
  void OpenData_Click(object s,RoutedEventArgs e){Process.Start(new ProcessStartInfo("explorer.exe","\""+App.StateRoot+"\""){UseShellExecute=true});}
  void SavePreferences(){File.WriteAllText(preferences,new JavaScriptSerializer().Serialize(new Dictionary<string,string>{{"client",client},{"work",SetupService.PackageRoot}}));}
  void WorkFolder_Click(object s,RoutedEventArgs e){if(busy)return;using(var dialog=new System.Windows.Forms.FolderBrowserDialog{Description="Choose a folder for resumable Full HD downloads and temporary setup files. Existing Preview 24 material ZIPs in this folder can be reused.",SelectedPath=SetupService.PackageRoot})if(dialog.ShowDialog()==System.Windows.Forms.DialogResult.OK){SetupService.PackageRoot=Path.GetFullPath(dialog.SelectedPath);WorkFolder.Text=SetupService.PackageRoot;SavePreferences();}}
  void Minimize_Click(object s,RoutedEventArgs e){WindowState=WindowState.Minimized;}void Maximize_Click(object s,RoutedEventArgs e){WindowState=WindowState==WindowState.Maximized?WindowState.Normal:WindowState.Maximized;}void Close_Click(object s,RoutedEventArgs e){Close();}
 }
}
