using System;
namespace Horizon {
 // UI timekeeping is independent of worker output, including preflight and retry.
 public sealed class SetupActivity {
  public string Message {get;private set;}
  public int Percent {get;private set;}
  public bool Active {get;private set;}
  DateTime started,reported,finished;
  public void Start(string message,DateTime now){started=reported=now;Message=message;Percent=-1;Active=true;}
  public void Report(string message,int percent,DateTime now){Message=message;Percent=percent;reported=now;}
  public void Stop(DateTime now){finished=now;Active=false;}
  static string Duration(TimeSpan value){return ((int)Math.Max(0,value.TotalMinutes)).ToString()+":"+Math.Max(0,value.Seconds).ToString("00");}
  public string Timing(DateTime now){
   string result="Elapsed "+Duration((Active?now:finished)-started);
   if(Active&&(now-reported).TotalSeconds>=10)result+=" · No new progress report for "+Duration(now-reported);
   if(Active&&(now-reported).TotalSeconds>=30)result+=". Check for a Windows or security prompt; open the setup log for details.";
   return result;
  }
 }
}
