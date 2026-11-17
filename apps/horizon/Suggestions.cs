using System;
using System.Diagnostics;
using System.Windows;

namespace Horizon {
 public partial class MainWindow {
  IssueReport preparedSuggestion;bool sendingSuggestion;
  void PrepareSuggestion_Click(object sender,RoutedEventArgs e){if(sendingSuggestion)return;try{if(string.IsNullOrWhiteSpace(SuggestionTitle.Text)||string.IsNullOrWhiteSpace(SuggestionDescription.Text))throw new ArgumentException("Enter a title and describe your suggestion first.");preparedSuggestion=IssueReports.Prepare(client,"Horizon suggestion: "+SuggestionTitle.Text,SuggestionDescription.Text,false);SuggestionPreview.Text=preparedSuggestion.Body;SuggestionPreview.Visibility=Visibility.Visible;SendSuggestionButton.IsEnabled=true;SaveSuggestionButton.IsEnabled=true;SuggestionMessage.Text="Review the text below. Send suggestion emails it privately to dev@asyncstuff.com through asyncstuff.com. A ZIP has also been saved locally. No logs are included.";}catch(Exception ex){preparedSuggestion=null;SendSuggestionButton.IsEnabled=false;SaveSuggestionButton.IsEnabled=false;SuggestionPreview.Visibility=Visibility.Collapsed;SuggestionMessage.Text=ex.Message;}}
  async void SendSuggestion_Click(object sender,RoutedEventArgs e){if(preparedSuggestion==null||sendingSuggestion)return;if(preparedSuggestion.RawTitle!="Horizon suggestion: "+SuggestionTitle.Text||preparedSuggestion.Description!=SuggestionDescription.Text){SuggestionMessage.Text="Your suggestion changed. Prepare and review it again before sending.";return;}sendingSuggestion=true;SendSuggestionButton.IsEnabled=false;try{SuggestionMessage.Text="Sending the reviewed suggestion…";SuggestionMessage.Text=await IssueReports.Send(preparedSuggestion,lifetime.Token);}catch(Exception ex){SuggestionMessage.Text=ex.Message;}finally{sendingSuggestion=false;SendSuggestionButton.IsEnabled=true;}}
  void SaveSuggestion_Click(object sender,RoutedEventArgs e){if(preparedSuggestion==null)return;try{Process.Start(new ProcessStartInfo("explorer.exe","/select,\""+preparedSuggestion.Archive+"\""){UseShellExecute=true});}catch(Exception ex){SuggestionMessage.Text=ex.Message;}}
 }
}
