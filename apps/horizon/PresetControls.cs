using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Media;

namespace Horizon {
 public partial class MainWindow {
  PresetStore presets;string presetError="";
  System.Windows.Forms.ToolStripItem trayLaunch;
  internal bool TrayLaunchEnabled{get{return trayLaunch!=null&&trayLaunch.Enabled;}}
  void InitializePresets(){try{presets=new PresetStore(App.StateRoot);}catch(Exception ex){presetError=ex.Message;App.Trace(presetError);}SectionFilter.Items.Add("New options");RefreshPresetChoices();}
  void RefreshPresetChoices(){string custom=presets?.ActiveCustom(client.Length==0?App.StateRoot:client)?.Id;var selection=client.Length==0?null:presets?.Selection(client);var choices=presets==null?catalog.presets:presets.Choices(catalog);PresetPicker.ItemsSource=choices;PresetPicker.SelectedItem=choices.FirstOrDefault(p=>custom!=null?p.customId==custom:selection!=null&&p.customId==null&&p.name==selection.BuiltIn);PresetName.Text=custom==null?"":presets.ActiveCustom(client).Name;RefreshPresetReview();}
  void LoadPresetSelection(){if(presets!=null&&!App.AppUpdateComplete&&App.UpdateTicket.Length==0&&System.IO.File.Exists(System.IO.Path.Combine(client,"ao-vk.dll"))){var defaults=presets.NewBuiltInValues(client,catalog);if(defaults.Count>0){store.Save(defaults);presets.AcknowledgeBuiltIn(client,catalog);RefreshSettings(true);}}RefreshPresetChoices();}
  void RefreshPresetReview(){var missing=presets!=null&&client.Length>0?presets.Missing(client,catalog):new HashSet<string>();foreach(var setting in catalog.settings)setting.NeedsReview=missing.Contains(setting.key);var custom=client.Length==0?null:presets?.ActiveCustom(client);PresetStateText.Text=presetError.Length>0?presetError:missing.Count>0?missing.Count+(missing.Count==1?" new option needs":" new options need")+" your choice. Red options are not yet saved in this custom preset.":custom!=null?custom.Name+" · Changes save automatically to this custom preset.":"Save your current graphics settings with a name.";PresetStateText.Foreground=missing.Count>0?Brushes.Salmon:new SolidColorBrush(Color.FromRgb(167,187,195));ReviewOptionsButton.Visibility=missing.Count>0?Visibility.Visible:Visibility.Collapsed;}
  void ApplySelectedPreset(){if(store==null||busy||!SavePending())return;var preset=PresetPicker.SelectedItem as Preset;if(preset==null)return;if(presets==null){Status("Saved presets are unavailable.",presetError);return;}try{store.Save(presets.Values(preset,catalog));presets.Select(client,preset,catalog);RefreshSettings(true);RefreshPresetChoices();Filter();Status(preset.name+" preset applied.","Missing custom options stay red until you choose or confirm a value.");UpdateState();}catch(Exception ex){Status("Preset could not be applied.",ex.Message);}}
  void SavePreset_Click(object sender,RoutedEventArgs e){if(store==null||busy||!SavePending())return;if(presets==null){Status("Saved presets are unavailable.",presetError);return;}try{RefreshSettings(true);var saved=presets.SaveNamed(client,PresetName.Text,catalog);RefreshPresetChoices();Filter();Status(saved.name+" preset saved.","New options stay marked until you explicitly choose or confirm their values.");}catch(Exception ex){Status("Preset could not be saved.",ex.Message);}}
  void ReviewOptions_Click(object sender,RoutedEventArgs e){ShowPage("Graphics");Search.Text="";SectionFilter.SelectedItem="New options";}
  void ViewUpdates_Click(object sender,RoutedEventArgs e){ShowPage("Updates");}
  internal void SetUpdateNotice(bool available,string message){UpdateNoticeText.Text=message;UpdateNotice.Visibility=available?Visibility.Visible:Visibility.Collapsed;}
 }
}
