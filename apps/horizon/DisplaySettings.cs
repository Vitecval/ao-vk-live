using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;

namespace Horizon {
 public sealed class DisplayPreferences {
  public string Mode="Game",Monitor="";public int Width,Height;
  public static DisplayPreferences Read(SettingsStore store){
   var values=store.ReadSection("Display");string text;int width,height;
   var p=new DisplayPreferences();
   if(values.TryGetValue("Mode",out text))p.Mode=new[]{"Game","Windowed","Borderless"}.FirstOrDefault(x=>x.Equals(text,StringComparison.OrdinalIgnoreCase))??"Game";
   if(values.TryGetValue("Monitor",out text))p.Monitor=text;
   if(values.TryGetValue("Width",out text)&&int.TryParse(text,NumberStyles.Integer,CultureInfo.InvariantCulture,out width))p.Width=width;
   if(values.TryGetValue("Height",out text)&&int.TryParse(text,NumberStyles.Integer,CultureInfo.InvariantCulture,out height))p.Height=height;
   if(p.Width<0||p.Height<0||p.Width>16384||p.Height>16384||(p.Width==0)!=(p.Height==0))p.Width=p.Height=0;
   return p;
  }
  public void Validate(){
   if(Mode!="Game"&&Mode!="Windowed"&&Mode!="Borderless")throw new ArgumentException("Choose a listed display mode.");
   if(Width<0||Height<0||Width>16384||Height>16384||(Width==0)!=(Height==0)||(Width!=0&&(Width<320||Height<200)))throw new ArgumentException("Choose a supported resolution or Use game setting.");
   if((Monitor??"").Length>128||(Monitor??"").Any(c=>char.IsControl(c)))throw new ArgumentException("Choose a listed monitor.");
  }
  public Dictionary<string,string> Values(){return new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase){{"Mode",Mode},{"Width",Width.ToString(CultureInfo.InvariantCulture)},{"Height",Height.ToString(CultureInfo.InvariantCulture)},{"Monitor",Monitor??""}};}
 }
 public sealed class DisplaySize {
  public int Width,Height;public bool Available=true;public string Label;
  public override string ToString(){return Label??(Width==0?"Use game setting":Width+" × "+Height+(Available?"":" (unavailable)"));}
 }
 public sealed class DisplayMonitor {
  public string Device="",Label="Automatic — AO window / primary monitor";public bool Available=true;
  public readonly List<DisplaySize> Sizes=new List<DisplaySize>();
  public override string ToString(){return Label;}
 }
 public static class DisplayDiscovery {
  // Unicode DEVMODE uses its display union, never its printer union. Enumeration
  // reads Windows' supported modes; no ChangeDisplaySettings call is made.
  [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct DevMode {
   [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string device;
   public ushort spec,driver,size,extra;public uint fields;
   public int x,y;public uint orientation,fixedOutput;
   public short colour,duplex,yResolution,ttOption,collate;
   [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string form;
   public ushort logPixels;public uint bits,width,height,flags,frequency,icmMethod,icmIntent,media,dither,reserved1,reserved2,panningWidth,panningHeight;
  }
  [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplaySettingsEx(string device,int mode,ref DevMode data,uint flags);
  static DevMode Mode(){return new DevMode{size=(ushort)Marshal.SizeOf(typeof(DevMode))};}
  public static List<DisplayMonitor> Read(){
   var result=new List<DisplayMonitor>{new DisplayMonitor()};
   foreach(var screen in System.Windows.Forms.Screen.AllScreens.OrderByDescending(x=>x.Primary).ThenBy(x=>x.DeviceName)){
    var current=Mode();EnumDisplaySettingsEx(screen.DeviceName,-1,ref current,0);
    var monitor=new DisplayMonitor{Device=screen.DeviceName,Label=screen.DeviceName.Replace(@"\\.\DISPLAY","Display ")+(screen.Primary?" (primary)":"")+" — "+(current.width>0?current.width+" × "+current.height:"current size unavailable")};
    var unique=new HashSet<string>();
    for(int index=0;index<4096;index++){
     var mode=Mode();if(!EnumDisplaySettingsEx(screen.DeviceName,index,ref mode,0))break;
     if(mode.bits<24||mode.width<320||mode.height<200||mode.width>16384||mode.height>16384)continue;
     string key=mode.width+"x"+mode.height;if(unique.Add(key))monitor.Sizes.Add(new DisplaySize{Width=(int)mode.width,Height=(int)mode.height});
    }
    monitor.Sizes.Sort((a,b)=>a.Width==b.Width?a.Height.CompareTo(b.Height):a.Width.CompareTo(b.Width));result.Add(monitor);
   }
   return result;
  }
 }
}
