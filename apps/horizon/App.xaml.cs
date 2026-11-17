using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Text;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

[assembly: AssemblyTitle("Rubi-Ka Horizon")]
[assembly: AssemblyProduct("Rubi-Ka Horizon")]
[assembly: AssemblyVersion("0.7.0.0")]
[assembly: AssemblyFileVersion("0.7.5.0")]
namespace Horizon {
 public partial class App:Application {
  public static string StateRoot=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"RubiKaHorizon");
  public static string ClientArgument="";
  public static bool RenderingPreview;
  public static string UpdateTicket="";
  SingleInstance instance;
  protected override void OnStartup(StartupEventArgs e){
   base.OnStartup(e);
   // A static companion should not compete with the game's GPU.
   RenderOptions.ProcessRenderMode=RenderMode.SoftwareOnly;
   for(int i=0;i<e.Args.Length;i++){
    if(e.Args[i]=="--state-root"&&i+1<e.Args.Length)StateRoot=Path.GetFullPath(e.Args[++i]);
    else if(e.Args[i]=="--client"&&i+1<e.Args.Length)ClientArgument=e.Args[++i];
    else if(e.Args[i]=="--finish-update"&&i+1<e.Args.Length)UpdateTicket=e.Args[++i];
   }
   try{SetupService.CheckDirectorySafety(StateRoot);Directory.CreateDirectory(StateRoot);}catch(Exception ex){MessageBox.Show(ex.Message,"Horizon data folder");Shutdown(1);return;}
   Trace("Starting "+SetupService.Version);
   DispatcherUnhandledException+=(s,args)=>{File.AppendAllText(Path.Combine(StateRoot,"app-errors.log"),DateTime.UtcNow+" "+args.Exception+Environment.NewLine);MessageBox.Show(args.Exception.Message,"Rubi-Ka Horizon",MessageBoxButton.OK,MessageBoxImage.Error);args.Handled=true;};
   int fixture=Array.IndexOf(e.Args,"--stock-fixture-dir");
   if(Array.IndexOf(e.Args,"--self-test-setup")>=0&&fixture>=0&&fixture+1<e.Args.Length){RunSetupTests(e.Args[fixture+1]);return;}
   if(Array.IndexOf(e.Args,"--uninstall-app")>=0){try{if(MessageBox.Show("Remove the Horizon application? AO graphics, preferences and downloaded files will stay in place. Use Installation → Remove graphics first if you also want to remove the graphics add-on.","Uninstall Rubi-Ka Horizon",MessageBoxButton.OKCancel,MessageBoxImage.Question)==MessageBoxResult.OK)SetupService.RemoveApp();Shutdown(0);}catch(Exception ex){MessageBox.Show(ex.Message,"Rubi-Ka Horizon");Shutdown(1);}return;}
   if(Array.IndexOf(e.Args,"--self-test")>=0){try{SelfTests.Run(StateRoot);Shutdown(0);}catch(Exception ex){File.WriteAllText(Path.Combine(StateRoot,"TEST-FAILURE.txt"),ex.ToString());Shutdown(1);}return;}
   if(Array.IndexOf(e.Args,"--self-test-updates")>=0){RunUpdateTests();return;}
   if(Array.IndexOf(e.Args,"--self-test-tray")>=0){RunTrayTests();return;}
   if(Array.IndexOf(e.Args,"--self-test-feature-ui")>=0){RunFeatureUiTests();return;}
   int probe=Array.IndexOf(e.Args,"--singleton-probe");if(probe>=0&&probe+1<e.Args.Length){try{Guid scope;if(!Guid.TryParseExact(e.Args[probe+1],"N",out scope))throw new IOException("Invalid probe scope.");using(var gate=new SingleInstance(SingleInstance.ProductKey+"-test-"+scope.ToString("N")))using(var signal=new System.Threading.ManualResetEvent(false)){bool owner=gate.Enter();if(owner)gate.Listen(()=>signal.Set());File.WriteAllText(Path.Combine(StateRoot,"probe-ready.txt"),owner?"owner":"secondary");bool activated=owner?signal.WaitOne(10000):gate.Signal();File.WriteAllText(Path.Combine(StateRoot,"probe-result.txt"),(owner?"owner":"secondary")+":"+activated);}Shutdown(0);}catch(Exception ex){File.WriteAllText(Path.Combine(StateRoot,"probe-failure.txt"),ex.ToString());Shutdown(1);}return;}
   int render=Array.IndexOf(e.Args,"--render-preview");
   RenderingPreview=render>=0;
   if(!RenderingPreview){
    try{
     // An update successor must wait for its parent before taking the same
     // product-wide lock; ordinary duplicate launches only activate the owner.
     if(UpdateTicket.Length>0)UpdateService.WaitForParent(UpdateService.ReadHandoff(UpdateTicket));
     int wait=Array.IndexOf(e.Args,"--wait-horizon-parent");if(wait>=0){if(wait+2>=e.Args.Length)throw new IOException("Incomplete Horizon restart handoff.");UpdateService.WaitForParent(new System.Collections.Generic.Dictionary<string,object>{{"parentPid",int.Parse(e.Args[wait+1])},{"parentStartUtc",long.Parse(e.Args[wait+2])}});}
     instance=new SingleInstance(SingleInstance.ProductKey);
     if(!instance.Enter()){instance.Signal();instance.Dispose();instance=null;Shutdown(0);return;}
     if(SingleInstance.ActivateLegacy()){if(UpdateTicket.Length>0)MessageBox.Show("Another Horizon window is still open. Close the other copy, then retry the update. Your graphics have not changed.","Close the other Horizon copy");instance.Dispose();instance=null;Shutdown(0);return;}
    }catch(Exception ex){if(instance!=null){instance.Dispose();instance=null;}MessageBox.Show(ex.Message,"Horizon startup");Shutdown(1);return;}
   }
   Trace("Opening main window");var w=new MainWindow();MainWindow=w;Trace("Main window constructed");
   if(instance!=null)instance.Listen(()=>Dispatcher.BeginInvoke(new Action(()=>{if(Dispatcher.HasShutdownStarted||!w.IsLoaded)return;if(w.WindowState==WindowState.Minimized)w.WindowState=WindowState.Normal;w.Show();w.Activate();})));
   int page=Array.IndexOf(e.Args,"--page");if(page>=0&&page+1<e.Args.Length)w.PreviewPage(e.Args[page+1]);
   if(RenderingPreview&&Array.IndexOf(e.Args,"--preview-busy")>=0)w.PreviewBusy();
   if(render>=0&&render+1<e.Args.Length){string output=e.Args[render+1];w.Dispatcher.BeginInvoke(new Action(async()=>{
    try{await w.PreviewInstalled();if(Array.IndexOf(e.Args,"--preview-report")>=0)w.PreviewReport();if(Array.IndexOf(e.Args,"--preview-setup-activity")>=0)w.PreviewSetupActivity();if(Array.IndexOf(e.Args,"--preview-keybinds")>=0)w.PreviewKeybinds();var root=(FrameworkElement)w.Content;root.Measure(new Size(w.Width,w.Height));root.Arrange(new Rect(0,0,w.Width,w.Height));root.UpdateLayout();var bmp=new RenderTargetBitmap((int)w.Width,(int)w.Height,96,96,PixelFormats.Pbgra32);bmp.Render(root);var enc=new PngBitmapEncoder();enc.Frames.Add(BitmapFrame.Create(bmp));using(var f=File.Create(output))enc.Save(f);Shutdown(0);}catch(Exception ex){File.WriteAllText(output+".error",ex.ToString());Shutdown(1);}
   }),DispatcherPriority.ApplicationIdle);}else {w.ShowActivated=false;w.Show();Trace("Window shown");}
  }
  public static string ResourceText(string name){bool compressed=name.EndsWith("-manifest.json",StringComparison.Ordinal)||name=="temporary-assets.txt";using(var source=GetResourceStream(new Uri("/Rubi-Ka Horizon;component/Assets/"+name+(compressed?".gz":""),UriKind.Relative)).Stream){using(Stream data=compressed?(Stream)new GZipStream(source,CompressionMode.Decompress,true):source)using(var r=new StreamReader(data,Encoding.UTF8))return r.ReadToEnd();}}
  async void RunSetupTests(string stock){try{await SelfTests.Setup(StateRoot,stock);Shutdown(0);}catch(Exception ex){File.WriteAllText(Path.Combine(StateRoot,"SETUP-TEST-FAILURE.txt"),ex.ToString());Shutdown(1);}}
  async void RunUpdateTests(){try{await UpdateTests.Run(StateRoot);Shutdown(0);}catch(Exception ex){File.WriteAllText(Path.Combine(StateRoot,"UPDATE-TEST-FAILURE.txt"),ex.ToString());Shutdown(1);}}
  async void RunTrayTests(){try{await SelfTests.Tray(StateRoot);Shutdown(0);}catch(Exception ex){File.WriteAllText(Path.Combine(StateRoot,"TRAY-TEST-FAILURE.txt"),ex.ToString());Shutdown(1);}}
  async void RunFeatureUiTests(){try{await PresetTests.Ui(StateRoot);Shutdown(0);}catch(Exception ex){File.WriteAllText(Path.Combine(StateRoot,"FEATURE-UI-FAILURE.txt"),ex.ToString());Shutdown(1);}}
  public static void Trace(string text){try{File.AppendAllText(Path.Combine(StateRoot,"startup.log"),DateTime.UtcNow.ToString("o")+" "+text+Environment.NewLine);}catch{}}
  protected override void OnExit(ExitEventArgs e){if(instance!=null){instance.Dispose();instance=null;}base.OnExit(e);}
 }
}
