using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Web.Script.Serialization;

namespace Horizon {
 public sealed class EditionBrush:System.Windows.Data.IValueConverter {
  public object Convert(object value,Type target,object parameter,CultureInfo culture){return new System.Windows.Media.SolidColorBrush(value is bool b&&b?System.Windows.Media.Color.FromRgb(200,172,124):System.Windows.Media.Color.FromRgb(73,96,103));}
  public object ConvertBack(object value,Type target,object parameter,CultureInfo culture){throw new NotSupportedException();}
 }
 public class Catalog {
  public int rendererRevision{get;set;} public List<Setting> settings{get;set;} public List<Preset> presets{get;set;}
  public static Catalog Load(){return new JavaScriptSerializer{MaxJsonLength=4000000}.Deserialize<Catalog>(App.ResourceText("settings.json"));}
 }
 public class Preset {public string name{get;set;}public Dictionary<string,double> values{get;set;}public override string ToString(){return name;}}
 public class Choice {public double Value{get;set;} public string Label{get;set;}public override string ToString(){return Label;}}
 public class Setting:INotifyPropertyChanged {
  public string key{get;set;}public string label{get;set;}public string section{get;set;}public int type{get;set;}
  public double min{get;set;}public double max{get;set;}public double step{get;set;}public double defaultValue{get;set;}
  public string parent{get;set;}public string choices{get;set;}
  double current;
  public double Value{get{return current;}set{if(Math.Abs(current-value)<.00000001)return;current=value;Notify("Value");Notify("DisplayValue");}}
  public string DisplayValue{get{return type==4?((int)Value).ToString("X6"):Value.ToString("0.###",CultureInfo.InvariantCulture);}}
  public string Title{get{string v;return Titles.TryGetValue(key,out v)?v:label;}}
  public string Description{get{return Titles.ContainsKey(key)&&!string.Equals(label,Title,StringComparison.OrdinalIgnoreCase)?label:"";}}
  public bool Quick{get{return Titles.ContainsKey(key);}}
  public List<Choice> Options(){
   var parts=(choices??"").Split(new[]{' '},StringSplitOptions.RemoveEmptyEntries);
   var words=label.Contains(":")?label.Substring(label.IndexOf(':')+1).Split('/'):new string[0];
   var result=new List<Choice>();
   for(int i=0;i<parts.Length;i++){double n;if(!double.TryParse(parts[i],NumberStyles.Float,CultureInfo.InvariantCulture,out n))continue;string title=i<words.Length?words[i].Trim():parts[i];
    if(key=="RVK_SunRes"||key=="RVK_PtRes")title=parts[i]+" px";
    if(key=="RVK_UiRate")title=n==0?"Every frame":parts[i]+" fps";
    if(key=="RVK_RenderScale")title=parts[i]+"%";
    if(key=="RVK_Aniso")title=n==1?"Off":parts[i]+"×";
    if(title.Length>65)title=parts[i];result.Add(new Choice{Value=n,Label=title});}
   return result;
  }
  public double Normalize(double v){
   if(double.IsNaN(v)||double.IsInfinity(v))throw new ArgumentException("Enter a finite number.");
   if(v<min-1e-6||v>max+1e-6)throw new ArgumentException("Choose a value between "+min.ToString("0.###")+" and "+max.ToString("0.###")+".");
   v=Math.Max(min,Math.Min(max,v));
   if(type==0)return v==0?0:1;
   if(type==1||type==4)v=Math.Round(v);
   if(type==3&&!Options().Any(o=>Math.Abs(o.Value-v)<.00001))throw new ArgumentException("Choose one of the listed values.");
   return v;
  }
  public string Serialize(double v){v=Normalize(v);return type==4?((int)v).ToString("X6"):v.ToString("0.#########",CultureInfo.InvariantCulture);}
  public double Parse(string v){if(type==4)return int.Parse(v.Trim().Replace("#","").Replace("0x",""),NumberStyles.HexNumber,CultureInfo.InvariantCulture);return double.Parse(v,NumberStyles.Float,CultureInfo.InvariantCulture);}
  public event PropertyChangedEventHandler PropertyChanged;
  void Notify(string name){PropertyChanged?.Invoke(this,new PropertyChangedEventArgs(name));}
  static readonly Dictionary<string,string> Titles=new Dictionary<string,string>{
   {"RVK_Enhance","Graphics enhancements"},{"RVK_ZoneScreen","Zoning screens"},{"RVK_ZoneClick","Click to continue after zoning"},
   {"RVK_RenderBg","Render while AO is in the background"},{"RVK_SunShadow","Sun shadows"},{"RVK_SunDist","Sun-shadow distance"},
   {"RVK_SunRes","Sun-shadow resolution"},{"RVK_GrassOn","Ground grass"},{"RVK_GrassDist","Grass distance"},
   {"RVK_GrassBlades","Grass density"},{"RVK_WaterOn","Enhanced water"},{"RVK_WaterQual","Water quality"},
   {"RVK_ModernTex","World textures"},{"RVK_EnhancedRocks","Enhanced rocks"},{"RVK_Hdr","HDR lighting"},
   {"RVK_AutoMat","Character & equipment materials"},{"RVK_CharacterStyle","Character & equipment artwork"},
   {"RVK_Exposure","Exposure"},{"RVK_Bloom","Bloom"},{"RVK_Sharpen","Sharpening"},{"RVK_Taa","Anti-aliasing"},
   {"RVK_RenderScale","Render resolution"},{"RVK_FrameGen","Frame generation"},{"RVK_AmbientMotes","Ambient life density"},
   {"RVK_FarScenery","Distant scenery"},{"RVK_ImmersiveShips","Immersive ships"},{"RVK_DayCycle","Day and night cycle"}
  };
 }
}
