using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Threading;
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
  bool checkingUpdate,downloadingUpdate,graphicsUpdate,sendingReport;HorizonRelease availableUpdate;string downloadedUpdate="";IssueReport preparedReport;
  readonly CancellationTokenSource lifetime=new CancellationTokenSource();
  InstalledState detected;string detectedKey="";bool detecting,memoryEdited;
  bool MemoryChanged{get{return memoryEdited&&(detected==null||!detected.MemoryEnabled.HasValue||detected.MemoryEnabled.Value!=(MemoryOption.IsChecked==true));}}
  readonly string preferences=Path.Combine(App.StateRoot,"preferences.json");
  public MainWindow(){
   InitializeComponent();App.Trace("Interface loaded");catalog=Catalog.Load();App.Trace("Settings catalogue loaded");
   foreach(var s in catalog.settings)s.Value=s.defaultValue;
   AddNav("Graphics","\uE713");AddNav("Installation","\uE896");AddNav("Updates","\uE895");AddNav("Report issue","\uE90A");AddNav("About","\uE946");
   SidebarVersion.Text=SetupService.Version;AboutVersion.Text=SetupService.Version+" · Stable release";CurrentVersionText.Text="Running Horizon: "+SetupService.Version;
   SectionFilter.Items.Add("Common settings");SectionFilter.Items.Add("All settings");foreach(var section in catalog.settings.Select(x=>x.section).Distinct().OrderBy(x=>x))SectionFilter.Items.Add(section);SectionFilter.SelectedIndex=0;
   PresetPicker.ItemsSource=catalog.presets;PresetPicker.SelectedIndex=-1;
   Search.ToolTip="Search "+catalog.settings.Count+" graphics settings";
   saveTimer.Tick+=(s,e)=>{saveTimer.Stop();SavePending();};refreshTimer.Tick+=async(s,e)=>{if(!busy){RefreshSettings(false);await RefreshInstalled(false);UpdateState();}};
   string chosen=App.ClientArgument;
   if(File.Exists(preferences)){try{var p=new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(File.ReadAllText(preferences));if(chosen.Length==0&&p.ContainsKey("client"))chosen=p["client"];if(p.ContainsKey("work"))SetupService.PackageRoot=p["work"];if(p.ContainsKey("autoUpdates"))AutoUpdates.IsChecked=p["autoUpdates"]!="false";}catch{}}
   WorkFolder.Text=SetupService.PackageRoot;
   MemoryOption.Click+=(s,e)=>{memoryEdited=true;UpdateState();};
   LiteChoice.Checked+=Edition_Changed;FullChoice.Checked+=Edition_Changed;
   ready=true;App.Trace("Reading selected AO folder");if(chosen.Length>0)Choose(chosen,false);else UpdateState();App.Trace("AO folder ready");
   ShowPage(chosen.Length==0?"Installation":"Graphics");Filter();refreshTimer.Start();
   if(store==null){var ignored=RefreshInstalled(true);}
   Closing+=(s,e)=>{if(App.RenderingPreview)return;if(busy){e.Cancel=true;MessageBox.Show(this,"Setup is still working. Keep Horizon open until it finishes so it can verify or recover the operation.","Setup in progress",MessageBoxButton.OK,MessageBoxImage.Information);}else if(!SavePending()){e.Cancel=MessageBox.Show(this,"The last changes could not be saved. Close and discard only those unsaved changes?","Unsaved changes",MessageBoxButton.YesNo,MessageBoxImage.Question)!=MessageBoxResult.Yes;if(!e.Cancel)pending.Clear();}};
   Closed+=(s,e)=>{saveTimer.Stop();refreshTimer.Stop();lifetime.Cancel();};
   Loaded+=async(s,e)=>{if(App.RenderingPreview)return;if(App.UpdateTicket.Length>0){await FinishDownloadedUpdate();return;}if(AutoUpdates.IsChecked==true){try{await Task.Delay(1500,lifetime.Token);await CheckForUpdates(false);}catch(OperationCanceledException){}}};
  }
  void AddNav(string title,string icon){
   var line=new StackPanel{Orientation=Orientation.Horizontal};line.Children.Add(new TextBlock{Text=icon,FontFamily=new FontFamily("Segoe MDL2 Assets"),FontSize=15,Width=30,VerticalAlignment=VerticalAlignment.Center});line.Children.Add(new TextBlock{Text=title,FontSize=13});
   var button=new Button{Content=line,HorizontalContentAlignment=HorizontalAlignment.Left,Padding=new Thickness(12,11,12,11),Margin=new Thickness(0,0,0,7),BorderBrush=Brushes.Transparent,Background=Brushes.Transparent};button.Click+=(s,e)=>ShowPage(title);Navigation.Children.Add(button);nav.Add(title,button);
  }
  void ShowPage(string page){
   GraphicsPage.Visibility=page=="Graphics"?Visibility.Visible:Visibility.Collapsed;
   InstallationPage.Visibility=page=="Installation"?Visibility.Visible:Visibility.Collapsed;
   AboutPage.Visibility=page=="About"?Visibility.Visible:Visibility.Collapsed;
   UpdatesPage.Visibility=page=="Updates"?Visibility.Visible:Visibility.Collapsed;ReportPage.Visibility=page=="Report issue"?Visibility.Visible:Visibility.Collapsed;
   foreach(var b in nav)b.Value.Background=b.Key==page?new SolidColorBrush(Color.FromRgb(37,58,65)):Brushes.Transparent;
   PageTitle.Text=page;
   PageSubtitle.Text=page=="Graphics"?"Settings are also available in game with Ctrl+Shift+F12.":page=="Installation"?"Manage the selected AO installation.":page=="Updates"?"Keep Horizon and its graphics up to date.":page=="Report issue"?"Prepare a diagnostic report.":"Application details.";
  }
  public void PreviewPage(string page){if(nav.ContainsKey(page))ShowPage(page);}
  public void PreviewBusy(){busy=true;UpdateState();Status("Checking installed files…","Full HD repair verifies the artwork and cache. This can take several minutes.");}
  public void PreviewReport(){if(App.RenderingPreview){IssueDescription.Text="Example diagnostic report";PrepareReport_Click(null,null);}}
  public async Task PreviewInstalled(){while(detecting)await Task.Delay(20);await RefreshInstalled(true);}
  async Task RefreshInstalled(bool force){
   if(force)while(detecting)await Task.Delay(20);if(busy||detecting)return;string target=client,edition=FullChoice.IsChecked==true?"full":"lite",key=InstallDetection.Key(target,edition);if(!force&&key==detectedKey)return;
   detecting=true;try{var info=await Task.Run(()=>InstallDetection.Read(target,edition));if(target!=client||edition!=(FullChoice.IsChecked==true?"full":"lite"))return;detected=info;detectedKey=key;
    string graphics=!info.GraphicsPresent?"Not installed":info.GraphicsVersion+(info.Edition=="full"?" · Full HD":info.Edition=="lite"?" · Lite":" · edition unknown");
    GraphicsInstallStatus.Text="Graphics: "+graphics;GraphicsInstallDetail.Text=info.GraphicsPresent?(info.GraphicsManaged?(info.GraphicsCurrent?"Matches the bundled graphics. Repair is available if you need a full file check.":"An update or edition change is available."):"Detected files have no matching installation record. Update or Repair will verify them."):"Install graphics into this AO folder to use the enhancements.";
    AppInstallStatus.Text=info.AppPresent?"Installed app: "+info.AppVersion+(info.AppManaged?"":" (unverified)"):"Horizon is running as a portable copy; the app is not installed in the Start menu.";
    if(!memoryEdited&&info.GraphicsManaged&&info.MemoryEnabled.HasValue)MemoryOption.IsChecked=info.MemoryEnabled.Value;
    if(info.Warning.Length>0)AppInstallStatus.Text+="\n"+info.Warning;
    AppInstallButton.Content=info.AppPresent?(info.AppCurrent?"Start-menu app is installed":"Update Start-menu app"):"Install app only (Start menu)";AppInstallButton.IsEnabled=!info.AppCurrent;
    DetectedVersionsText.Text="Installed Horizon app: "+info.AppVersion+"\nSelected AO graphics: "+(target.Length==0?"No AO folder selected":graphics);
    if(graphicsUpdate&&info.AppCurrent){
     if(!info.GraphicsPresent||info.GraphicsCurrent){graphicsUpdate=false;ApplyUpdateButton.Visibility=Visibility.Collapsed;UpdateMessage.Text=info.GraphicsPresent?"Horizon app and selected graphics are up to date.":"Horizon app is installed and up to date.";}
     else ApplyUpdateButton.Content="Update graphics";
    }
   }catch(Exception ex){GraphicsInstallDetail.Text="Could not read installation details: "+ex.Message;}
   finally{detecting=false;UpdateState();}
  }
  void Choose(string folder,bool save){
   if(!SavePending())return;
   try{
    var next=new SettingsStore(folder,catalog.settings);client=next.DirectoryPath;store=next;pending.Clear();
    graphicsUpdate=false;preparedReport=null;SendReportButton.IsEnabled=false;OpenReportButton.IsEnabled=false;ReportPreview.Clear();ReportReviewBlock.Visibility=Visibility.Collapsed;detected=null;detectedKey="";memoryEdited=false;
    if(save)SavePreferences();
    FolderPath.Text=client;SidebarClient.Text=client;
    RefreshSettings(true);Status("Settings ready.","Saved changes are picked up by AO, usually within half a second.");
    LiteChoice.IsChecked=false;FullChoice.IsChecked=false;
    var receipt=Path.Combine(client,"ao-vk-live.install.json");if(File.Exists(receipt)){try{var m=new JavaScriptSerializer{MaxJsonLength=32000000}.Deserialize<Dictionary<string,object>>(File.ReadAllText(receipt));if(m.ContainsKey("edition")){FullChoice.IsChecked=(string)m["edition"]=="full";LiteChoice.IsChecked=(string)m["edition"]=="lite";}}catch{}}
   }catch(Exception ex){Status("Could not open this AO installation.",ex.Message);}
   UpdateState();var ignored=RefreshInstalled(true);
  }
  void UpdateState(){
   bool has=store!=null,installed=has&&File.Exists(Path.Combine(client,"ao-vk.dll"));
   GraphicsPage.IsEnabled=installed&&!busy;InstallationPage.IsEnabled=!busy;LaunchButton.IsEnabled=has&&!busy;InstallButton.IsEnabled=has&&!busy;
   InstallButton.Content=detected==null?"Checking installation…":MemoryChanged&&detected.GraphicsCurrent&&detected.AppCurrent?"Apply memory setting":detected.PrimaryAction;InstallButton.IsEnabled=has&&!busy&&detected!=null&&(!(detected.AppCurrent&&detected.GraphicsCurrent)||MemoryChanged);
   if(LiteChoice.IsChecked!=true&&FullChoice.IsChecked!=true){InstallButton.Content="Choose Lite or Full HD above";InstallButton.IsEnabled=false;}
   RemoveFullHdButton.IsEnabled=!busy&&detected!=null&&detected.Edition=="full";
   UndoButton.IsEnabled=store!=null&&store.CanUndo&&!busy;
   CheckUpdatesButton.IsEnabled=!busy&&!checkingUpdate&&!downloadingUpdate;ApplyUpdateButton.IsEnabled=!busy&&!checkingUpdate&&!downloadingUpdate;
   if(has&&!installed&&!busy)SaveDetail.Text="Install graphics from the Installation page to use live settings.";
  }
  async void Edition_Changed(object sender,RoutedEventArgs e){if(!ready||busy)return;UpdateState();await RefreshInstalled(true);}
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
   if((action=="Install"||action=="Update"||action=="Repair")&&LiteChoice.IsChecked!=true&&FullChoice.IsChecked!=true){ShowPage("Installation");Status("Choose Lite or Full HD first.","Lite keeps the original textures. Full HD installs the new artwork.");return;}
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
   finally{busy=false;memoryEdited=false;SetupProgress.IsIndeterminate=false;SetupProgress.Visibility=Visibility.Collapsed;await RefreshInstalled(true);UpdateState();}
  }
  async void Install_Click(object s,RoutedEventArgs e){await RefreshInstalled(true);if(detected!=null&&detected.GraphicsCurrent&&!MemoryChanged){await InstallAppOnly();return;}await Operation(File.Exists(Path.Combine(client,"ao-vk-live.install.json"))?"Update":"Install");await RefreshInstalled(true);}
  async void RemoveFullHd_Click(object s,RoutedEventArgs e){
   if(busy||detected==null||detected.Edition!="full")return;
   if(MessageBox.Show(this,"Remove the Full HD artwork and keep the Lite graphics engine, Horizon app and your settings? Close AO and its launcher first.","Remove Full HD",MessageBoxButton.OKCancel,MessageBoxImage.Question)!=MessageBoxResult.OK)return;
   LiteChoice.IsChecked=true;await Operation("Update");
  }
  async void Repair_Click(object s,RoutedEventArgs e){await Operation("Repair");}async void Reset_Click(object s,RoutedEventArgs e){await Operation("Reset settings");}async void Uninstall_Click(object s,RoutedEventArgs e){await Operation("Uninstall");}
  async void InstallApp_Click(object s,RoutedEventArgs e){await InstallAppOnly();}
  async Task InstallAppOnly(){if(busy)return;busy=true;UpdateState();try{SavePreferences();var target=await Task.Run(()=>SetupService.InstallApp());Status("Horizon installed in your Start menu.",target);}catch(Exception ex){Status("The app could not be installed.",ex.Message);}finally{busy=false;await RefreshInstalled(true);UpdateState();}}
  void Launch_Click(object s,RoutedEventArgs e){if(store==null||busy||!SavePending())return;try{Process.Start(new ProcessStartInfo(Path.Combine(client,"Anarchy.exe")){WorkingDirectory=client,UseShellExecute=true});}catch(Exception ex){Status("AO could not be launched.",ex.Message);}}
  void OpenData_Click(object s,RoutedEventArgs e){Process.Start(new ProcessStartInfo("explorer.exe","\""+App.StateRoot+"\""){UseShellExecute=true});}
  void SavePreferences(){File.WriteAllText(preferences,new JavaScriptSerializer().Serialize(new Dictionary<string,string>{{"client",client},{"work",SetupService.PackageRoot},{"autoUpdates",AutoUpdates.IsChecked==true?"true":"false"}}));}
  void AutoUpdates_Click(object s,RoutedEventArgs e){try{SavePreferences();}catch(Exception ex){Status("Could not save the update preference.",ex.Message);}}
  async void CheckUpdates_Click(object s,RoutedEventArgs e){await CheckForUpdates(true);}
  async Task CheckForUpdates(bool manual){
   if(busy||checkingUpdate||downloadingUpdate)return;checkingUpdate=true;if(manual)ShowPage("Updates");UpdateMessage.Text="Checking GitHub for updates…";UpdateDetail.Text="";UpdateState();
   try{
    await RefreshInstalled(true);string selected=client,edition=FullChoice.IsChecked==true?"full":"lite";var release=await UpdateService.Check(lifetime.Token);
    bool newer=UpdateService.Compare(release.Tag,UpdateService.VersionTag)>0;
    if(availableUpdate==null||availableUpdate.Tag!=release.Tag||availableUpdate.Sha256!=release.Sha256)downloadedUpdate="";availableUpdate=release;
    bool needsGraphics=selected==client&&detected!=null&&detected.GraphicsPresent&&!detected.GraphicsCurrent,needsApp=detected!=null&&!detected.AppCurrent;
    graphicsUpdate=!newer&&(needsGraphics||needsApp);
    ApplyUpdateButton.Visibility=newer||graphicsUpdate?Visibility.Visible:Visibility.Collapsed;ApplyUpdateButton.Content=newer?(downloadedUpdate.Length>0?"Apply update":"Download update"):needsGraphics?(needsApp?"Update app + graphics":"Update graphics"):detected!=null&&detected.AppPresent?"Update installed app":"Install Horizon app";
    UpdateMessage.Text=newer?"Horizon "+release.Tag.TrimStart('v')+" is available.":needsGraphics?"The running app is up to date. The selected graphics need an update.":needsApp?"The running app is up to date; install or update its Start-menu copy.":selected.Length==0?"Horizon is up to date. No AO folder is selected.":detected!=null&&!detected.GraphicsPresent?"Horizon is up to date. Graphics are not installed in this AO folder.":"Horizon app and selected graphics are up to date.";
    UpdateDetail.Text=newer?"Download: "+(release.Bytes/1000000.0).ToString("0.0",CultureInfo.InvariantCulture)+" MB. The app verifies the download before you apply it.":"Checked GitHub's latest stable app and this installation against the bundled graphics. No texture packs were downloaded.";
    if(!manual&&newer)Status("A Horizon update is available.","Open Updates to download it. Your game continues running.");
   }catch(OperationCanceledException){if(!lifetime.IsCancellationRequested){UpdateMessage.Text="The update check timed out.";UpdateDetail.Text="Try again when the connection is available. AO is unchanged.";}}
   catch(Exception ex){UpdateMessage.Text="Could not check for updates.";UpdateDetail.Text=ex.Message;}
   finally{checkingUpdate=false;UpdateState();}
  }
  async void ApplyUpdate_Click(object s,RoutedEventArgs e){
   if(busy||checkingUpdate||downloadingUpdate)return;
   if(graphicsUpdate){await RefreshInstalled(true);if(detected!=null&&!detected.AppCurrent)await InstallAppOnly();if(detected!=null&&detected.GraphicsPresent&&!detected.GraphicsCurrent)await Operation("Update");await RefreshInstalled(true);return;}
   var release=availableUpdate;if(release==null||UpdateService.Compare(release.Tag,UpdateService.VersionTag)<=0)return;
   if(downloadedUpdate.Length==0){
    downloadingUpdate=true;UpdateState();UpdateProgress.Visibility=Visibility.Visible;UpdateMessage.Text="Downloading Horizon…";long last=0;
    try{downloadedUpdate=await UpdateService.Download(release,(done,total)=>{long now=Environment.TickCount;if(done!=total&&now-last<100)return;last=now;Dispatcher.BeginInvoke(new Action(()=>{UpdateProgress.Value=100.0*done/total;UpdateDetail.Text=(done/1000000.0).ToString("0.0")+" / "+(total/1000000.0).ToString("0.0")+" MB";}));},lifetime.Token);ApplyUpdateButton.Content="Apply update";UpdateMessage.Text="Update downloaded and verified.";UpdateDetail.Text="Close the selected AO client and launcher, then apply. Horizon will restart; settings and texture downloads are kept.";}
    catch(OperationCanceledException){if(!lifetime.IsCancellationRequested){UpdateMessage.Text="The update download timed out.";UpdateDetail.Text="Try again. Nothing was installed.";}}
    catch(Exception ex){UpdateMessage.Text="Update was not installed.";UpdateDetail.Text=ex.Message;}
    finally{downloadingUpdate=false;UpdateProgress.Visibility=Visibility.Collapsed;UpdateState();}return;
   }
   if(store==null){UpdateMessage.Text="Choose your AO folder before applying the update.";return;}if(!SavePending())return;
   try{SavePreferences();string ticket=UpdateService.PrepareHandoff(downloadedUpdate,release,client,FullChoice.IsChecked==true?"full":"lite",MemoryOption.IsChecked!=true);Process.Start(new ProcessStartInfo(downloadedUpdate,"--finish-update \""+ticket+"\" --state-root \""+App.StateRoot+"\" --client \""+client+"\""){UseShellExecute=true,WorkingDirectory=Path.GetDirectoryName(downloadedUpdate)});Close();}
   catch(Exception ex){UpdateMessage.Text="Could not apply the update.";UpdateDetail.Text=ex.Message;}
  }
  async Task FinishDownloadedUpdate(){
   ShowPage("Updates");busy=true;UpdateState();UpdateProgress.Visibility=Visibility.Visible;UpdateProgress.IsIndeterminate=true;UpdateMessage.Text="Applying the verified Horizon update…";
   try{
    var ticket=await Task.Run(()=>UpdateService.ReadHandoff(App.UpdateTicket));string target=(string)ticket["client"],edition=(string)ticket["edition"];Choose(target,false);FullChoice.IsChecked=edition=="full";LiteChoice.IsChecked=edition=="lite";MemoryOption.IsChecked=!Convert.ToBoolean(ticket["skipMemoryChange"]);
    string installed=await Task.Run(()=>{SetupService.AssertSelectedClientClosed(target);return SetupService.InstallApp();});
    bool current=await Task.Run(()=>UpdateService.GraphicsCurrent(target,edition));
    if(!current){UpdateMessage.Text="Horizon updated. Updating the selected AO graphics…";await SetupService.Run(File.Exists(Path.Combine(target,"ao-vk-live.install.json"))?"Update":"Install",target,edition,(message,percent)=>Dispatcher.BeginInvoke(new Action(()=>{UpdateDetail.Text=message;UpdateProgress.IsIndeterminate=percent<0;if(percent>=0)UpdateProgress.Value=percent;})),Convert.ToBoolean(ticket["skipMemoryChange"]));}
    SavePreferences();UpdateMessage.Text="Update finished.";UpdateDetail.Text=current?"The bundled graphics are already installed; no texture verification or download was needed.":"Horizon and the selected AO graphics are updated.";
    // Start the installed app, so future launches use the updated Start-menu copy.
    using(var parent=Process.GetCurrentProcess())Process.Start(new ProcessStartInfo(installed,"--state-root \""+App.StateRoot+"\" --client \""+target+"\" --wait-horizon-parent "+parent.Id+" "+parent.StartTime.ToUniversalTime().Ticks){UseShellExecute=true});busy=false;Close();
   }catch(Exception ex){UpdateMessage.Text="The update could not finish.";UpdateDetail.Text=ex.Message+" Any completed Horizon app update is kept; AO graphics use the installer's recovery checks.";App.Trace("Update failed: "+ex.Message);}
   finally{busy=false;UpdateProgress.IsIndeterminate=false;UpdateProgress.Visibility=Visibility.Collapsed;UpdateState();}
  }
  void PrepareReport_Click(object s,RoutedEventArgs e){
   if(sendingReport)return;try{preparedReport=IssueReports.Prepare(client,IssueTitle.Text,IssueDescription.Text,IncludeLogs.IsChecked==true);ReportPreview.Text=preparedReport.Body;ReportReviewBlock.Visibility=Visibility.Visible;SendReportButton.IsEnabled=true;OpenReportButton.IsEnabled=true;ReportMessage.Text="Review the report below. Send report emails these excerpts to dev@asyncstuff.com through asyncstuff.com. A ZIP is also saved locally.";}
   catch(Exception ex){ReportMessage.Text=ex.Message;preparedReport=null;ReportReviewBlock.Visibility=Visibility.Collapsed;SendReportButton.IsEnabled=false;OpenReportButton.IsEnabled=false;}
  }
  async void SendReport_Click(object s,RoutedEventArgs e){if(preparedReport==null||sendingReport)return;if(preparedReport.Description!=IssueDescription.Text||preparedReport.RawTitle!=IssueTitle.Text||preparedReport.IncludesLogs!=(IncludeLogs.IsChecked==true)){ReportMessage.Text="Your report options changed. Prepare and review the report again before sending.";return;}sendingReport=true;SendReportButton.IsEnabled=false;try{ReportMessage.Text="Sending the reviewed report…";ReportMessage.Text=await IssueReports.Send(preparedReport,lifetime.Token);}catch(OperationCanceledException){if(!lifetime.IsCancellationRequested)ReportMessage.Text="The confirmation timed out. You can retry this report; the service uses its ID to avoid duplicate mail.";}catch(Exception ex){ReportMessage.Text=ex.Message;}finally{sendingReport=false;SendReportButton.IsEnabled=true;}}
  void OpenReport_Click(object s,RoutedEventArgs e){if(preparedReport!=null)try{Process.Start(new ProcessStartInfo("explorer.exe","/select,\""+preparedReport.Archive+"\""){UseShellExecute=true});}catch(Exception ex){ReportMessage.Text=ex.Message;}}
  void WorkFolder_Click(object s,RoutedEventArgs e){if(busy)return;using(var dialog=new System.Windows.Forms.FolderBrowserDialog{Description="Choose a folder for resumable Full HD downloads and temporary setup files. Existing Preview 24 material ZIPs in this folder can be reused.",SelectedPath=SetupService.PackageRoot})if(dialog.ShowDialog()==System.Windows.Forms.DialogResult.OK){SetupService.PackageRoot=Path.GetFullPath(dialog.SelectedPath);WorkFolder.Text=SetupService.PackageRoot;SavePreferences();}}
  void Minimize_Click(object s,RoutedEventArgs e){WindowState=WindowState.Minimized;}void Maximize_Click(object s,RoutedEventArgs e){WindowState=WindowState==WindowState.Maximized?WindowState.Normal:WindowState.Maximized;}void Close_Click(object s,RoutedEventArgs e){Close();}
 }
}
