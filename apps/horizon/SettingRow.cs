using System;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace Horizon {
 public class SettingRow:UserControl {
  Setting setting;bool syncing;CheckBox toggle;ComboBox choice;Slider slider;TextBox number;TextBlock readout,title,review;Button confirm;
  public SettingRow(){Loaded+=(s,e)=>Bind();Unloaded+=(s,e)=>Detach();DataContextChanged+=(s,e)=>Bind();}
  void Detach(){if(setting!=null)setting.PropertyChanged-=Changed;setting=null;}
  void Bind(){
   Detach();setting=DataContext as Setting;if(setting==null)return;setting.PropertyChanged+=Changed;
   syncing=true;toggle=null;choice=null;slider=null;number=null;readout=null;
   var grid=new Grid();grid.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(1,GridUnitType.Star)});grid.ColumnDefinitions.Add(new ColumnDefinition{Width=new GridLength(245)});
   var labels=new StackPanel{Margin=new Thickness(0,0,18,0),VerticalAlignment=VerticalAlignment.Center};
   if(!string.IsNullOrEmpty(setting.help))grid.ToolTip=setting.help;
   title=new TextBlock{Text=setting.Title,FontSize=13,TextTrimming=TextTrimming.CharacterEllipsis,TextWrapping=TextWrapping.NoWrap,ToolTip=setting.section+" · "+setting.label};labels.Children.Add(title);review=new TextBlock{Text="New option · Choose a value or confirm the current value",Foreground=Brushes.Salmon,FontSize=10,TextWrapping=TextWrapping.Wrap,Margin=new Thickness(0,5,0,0)};labels.Children.Add(review);grid.Children.Add(labels);
   if(!string.IsNullOrEmpty(setting.help)){title.ToolTip=setting.help;labels.Children.Insert(1,new TextBlock{Text=setting.help,FontSize=11,TextWrapping=TextWrapping.Wrap,Margin=new Thickness(0,4,0,0),Opacity=.8});}
   var controls=new StackPanel{VerticalAlignment=VerticalAlignment.Center,Margin=new Thickness(10,0,0,0)};Grid.SetColumn(controls,1);grid.Children.Add(controls);
   if(setting.type==0){toggle=new CheckBox{Style=(Style)FindResource("Toggle"),HorizontalAlignment=HorizontalAlignment.Right};toggle.Checked+=(s,e)=>Send(1);toggle.Unchecked+=(s,e)=>Send(0);controls.Children.Add(toggle);}
   else if(setting.type==3){choice=new ComboBox{ItemsSource=setting.Options(),MinWidth=210,HorizontalAlignment=HorizontalAlignment.Right};choice.SelectionChanged+=(s,e)=>{if(choice.SelectedItem is Choice item)Send(item.Value);};controls.Children.Add(choice);}
   else{
    var top=new DockPanel{LastChildFill=false};readout=new TextBlock{Text=setting.type==4?"Hex color":setting.min.ToString("0.###",CultureInfo.InvariantCulture)+" – "+setting.max.ToString("0.###",CultureInfo.InvariantCulture),Foreground=new SolidColorBrush(Color.FromRgb(135,161,170)),FontSize=10,VerticalAlignment=VerticalAlignment.Center};top.Children.Add(readout);
    number=new TextBox{Width=85,Padding=new Thickness(8,5,8,5),HorizontalContentAlignment=HorizontalAlignment.Right,Margin=new Thickness(10,0,0,0)};DockPanel.SetDock(number,Dock.Right);top.Children.Add(number);controls.Children.Add(top);
    number.LostKeyboardFocus+=(s,e)=>CommitText();number.KeyDown+=(s,e)=>{if(e.Key==System.Windows.Input.Key.Enter){CommitText();e.Handled=true;}};
    if(setting.type!=4){slider=new Slider{Minimum=setting.min,Maximum=setting.max,TickFrequency=setting.step>0?setting.step:1,IsSnapToTickEnabled=true,Margin=new Thickness(0,8,0,0),SmallChange=setting.step>0?setting.step:1};slider.ValueChanged+=(s,e)=>Send(e.NewValue);controls.Children.Add(slider);}
   }
   confirm=new Button{Content="Keep current value",Padding=new Thickness(8,4,8,4),Margin=new Thickness(0,6,0,0),HorizontalAlignment=HorizontalAlignment.Right,ToolTip="Save this value as your choice for the new option"};confirm.Click+=(s,e)=>Send(setting.Value);controls.Children.Add(confirm);
   Content=new Border{BorderBrush=new SolidColorBrush(Color.FromRgb(42,61,70)),BorderThickness=new Thickness(0,0,0,1),Padding=new Thickness(13,11,13,11),MinHeight=48,Child=grid};
   Update();syncing=false;
  }
  void Send(double value){if(syncing||setting==null)return;var owner=Window.GetWindow(this) as MainWindow;if(owner==null)return;try{value=setting.Normalize(value);owner.Change(setting,value);}catch(Exception ex){owner.Status("Check this setting.",ex.Message);}Update();}
  void CommitText(){if(number==null||setting==null)return;try{Send(setting.Normalize(setting.Parse(number.Text)));number.ClearValue(BorderBrushProperty);}catch(Exception ex){number.BorderBrush=Brushes.IndianRed;(Window.GetWindow(this) as MainWindow)?.Status("Enter a valid setting.",ex.Message);}}
  void Changed(object s,PropertyChangedEventArgs e){if(e.PropertyName=="Value"||e.PropertyName=="NeedsReview")Update();}
  void Update(){if(setting==null)return;bool was=syncing;syncing=true;try{if(setting.NeedsReview)title.Foreground=Brushes.Salmon;else title.ClearValue(TextBlock.ForegroundProperty);review.Visibility=confirm.Visibility=setting.NeedsReview?Visibility.Visible:Visibility.Collapsed;if(toggle!=null){toggle.IsChecked=setting.Value!=0;toggle.Content=setting.Value!=0?"On":"Off";}if(choice!=null)choice.SelectedItem=choice.Items.OfType<Choice>().FirstOrDefault(x=>Math.Abs(x.Value-setting.Value)<.00001);if(number!=null&&!number.IsKeyboardFocusWithin)number.Text=setting.DisplayValue;if(slider!=null)slider.Value=Math.Max(setting.min,Math.Min(setting.max,setting.Value));}finally{syncing=was;}}
 }
}
