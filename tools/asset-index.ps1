# An acceleration hint for unchanged artwork, never proof of ownership for writes.
# The receipt hash invalidates the whole index after an interrupted transaction.
function Initialize-AoAssetIndex {
    if ('AoVk.AssetIndexV1' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
namespace AoVk {
 public sealed class AssetStampV1 {
  public long Length, Written, Created;
  public bool Same(AssetStampV1 other) { return other!=null && Length==other.Length && Written==other.Written && Created==other.Created; }
 }
 public sealed class AssetEntryV1 {
  public string Hash;
  public AssetStampV1 Stamp;
  public long VerifiedUtc;
  public AssetEntryV1(string hash,AssetStampV1 stamp) { Hash=hash; Stamp=stamp; VerifiedUtc=DateTime.UtcNow.Ticks; }
  public bool Matches(string hash,AssetStampV1 stamp) { return Hash==hash && Stamp.Same(stamp) && VerifiedUtc<=DateTime.UtcNow.Ticks && DateTime.UtcNow.Ticks-VerifiedUtc<=TimeSpan.FromDays(30).Ticks; }
 }
 public static class AssetIndexV1 {
  public const string FileName=".ao-vk-asset-index-v1.bin";
  static void SafeDirectory(string path) {
   for(var d=new DirectoryInfo(path); d!=null; d=d.Parent)
    if(d.Exists && (d.Attributes&FileAttributes.ReparsePoint)!=0) throw new IOException("Linked asset directory.");
  }
  public static AssetStampV1 Stamp(string path) {
   var f=new FileInfo(path); f.Refresh();
   if(!f.Exists || (f.Attributes&(FileAttributes.ReparsePoint|FileAttributes.Directory))!=0) return null;
   return new AssetStampV1 { Length=f.Length, Written=f.LastWriteTimeUtc.Ticks, Created=f.CreationTimeUtc.Ticks };
  }
  public static Dictionary<string,AssetStampV1> Snapshot(string client,string[] relativePaths) {
   var result=new Dictionary<string,AssetStampV1>(StringComparer.Ordinal);
   var folders=new Dictionary<string,Dictionary<string,string>>(StringComparer.OrdinalIgnoreCase);
   foreach(string rel in relativePaths) {
    string path=Path.Combine(client,rel.Replace('/',Path.DirectorySeparatorChar)), dir=Path.GetDirectoryName(path);
    Dictionary<string,string> wanted;
    if(!folders.TryGetValue(dir,out wanted)) folders[dir]=wanted=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
    wanted.Add(Path.GetFileName(path),rel);
   }
   foreach(var folder in folders) {
    SafeDirectory(folder.Key);
    if(!Directory.Exists(folder.Key)) continue;
    foreach(var f in new DirectoryInfo(folder.Key).EnumerateFiles()) {
     string rel;
     if(folder.Value.TryGetValue(f.Name,out rel) && (f.Attributes&FileAttributes.ReparsePoint)==0)
      result.Add(rel,new AssetStampV1 { Length=f.Length, Written=f.LastWriteTimeUtc.Ticks, Created=f.CreationTimeUtc.Ticks });
    }
   }
   return result;
  }
  public static Dictionary<string,AssetEntryV1> Empty() { return new Dictionary<string,AssetEntryV1>(StringComparer.Ordinal); }
  public static Dictionary<string,AssetEntryV1> Load(string client,string receiptHash) {
   try {
    SafeDirectory(client); string path=Path.Combine(client,FileName); var info=new FileInfo(path);
    if(!info.Exists || info.Length<48 || info.Length>64*1024*1024 || (info.Attributes&FileAttributes.ReparsePoint)!=0) return null;
    byte[] data=File.ReadAllBytes(path); int payload=data.Length-32;
    using(var hash=SHA256.Create()) { byte[] sum=hash.ComputeHash(data,0,payload); for(int i=0;i<32;i++)if(sum[i]!=data[payload+i])return null; }
    using(var stream=new MemoryStream(data,0,payload))using(var r=new BinaryReader(stream,Encoding.UTF8)) {
     if(r.ReadString()!="AO-VK-ASSET-INDEX-1" || r.ReadString()!=receiptHash) return null;
     int count=r.ReadInt32(); if(count<0 || count>500000) return null;
     var result=Empty();
     for(int i=0;i<count;i++) {
      string rel=r.ReadString(), hash=r.ReadString();
      if(rel.Length>1024 || hash.Length!=64) return null;
      var stamp=new AssetStampV1 { Length=r.ReadInt64(), Written=r.ReadInt64(), Created=r.ReadInt64() };
      if(stamp.Length<0 || stamp.Written<=0 || stamp.Created<=0) return null;
      var entry=new AssetEntryV1(hash,stamp); entry.VerifiedUtc=r.ReadInt64();
      result.Add(rel,entry);
     }
     return stream.Position==payload ? result : null;
    }
   } catch { return null; }
  }
  public static void Save(string client,string receiptHash,Dictionary<string,AssetEntryV1> verified) {
   SafeDirectory(client); string path=Path.Combine(client,FileName);
   if((File.Exists(path)||Directory.Exists(path)) && (File.GetAttributes(path)&(FileAttributes.ReparsePoint|FileAttributes.Directory))!=0) throw new IOException("Linked or invalid asset index preserved.");
   string stage=Path.Combine(client,FileName+".writing-"+Guid.NewGuid().ToString("N"));
   try {
    byte[] data;
    using(var memory=new MemoryStream()) {
     using(var w=new BinaryWriter(memory,Encoding.UTF8,true)) {
      w.Write("AO-VK-ASSET-INDEX-1"); w.Write(receiptHash); w.Write(verified.Count);
      foreach(var p in verified) { w.Write(p.Key); w.Write(p.Value.Hash); w.Write(p.Value.Stamp.Length); w.Write(p.Value.Stamp.Written); w.Write(p.Value.Stamp.Created); w.Write(p.Value.VerifiedUtc); }
     }
     data=memory.ToArray();
    }
    using(var f=new FileStream(stage,FileMode.CreateNew,FileAccess.Write,FileShare.None))using(var hash=SHA256.Create()) {
     f.Write(data,0,data.Length); byte[] sum=hash.ComputeHash(data); f.Write(sum,0,sum.Length); f.Flush(true);
    }
    if(File.Exists(path))File.Replace(stage,path,null); else File.Move(stage,path);
   } finally { if(File.Exists(stage))File.Delete(stage); }
  }
 }
}
'@
}

function Get-AoIndexedAssetHash([string]$Target, [string]$Relative, $Verified) {
    $before = [AoVk.AssetIndexV1]::Stamp($Target)
    $hash = Get-AoSetupHash $Target
    $after = [AoVk.AssetIndexV1]::Stamp($Target)
    if ($hash -and $before -and $before.Same($after)) {
        $Verified[$Relative] = New-Object AoVk.AssetEntryV1($hash, $after)
    }
    return $hash
}

function Save-AoAssetIndex([string]$Client, [string]$ReceiptHash, $Assets, $Verified) {
    # Only entries verified before/after hashing (or trusted unchanged entries)
    # can be carried forward. Renaming a verified stage preserves its stamp.
    $paths = @($Assets.PSObject.Properties | ForEach-Object { $_.Name })
    if ($Assets -is [Collections.IDictionary]) { $paths = @($Assets.Keys) }
    $current = [AoVk.AssetIndexV1]::Snapshot($Client, [string[]]$paths)
    $result = [AoVk.AssetIndexV1]::Empty()
    foreach ($relative in $paths) {
        $hash = if ($Assets -is [Collections.IDictionary]) { [string]$Assets[$relative] } else { [string]$Assets.$relative }
        $entry = $Verified[$relative]
        if ($entry -and $entry.Matches($hash, $current[$relative])) { $result.Add($relative, $entry) }
    }
    [AoVk.AssetIndexV1]::Save($Client, $ReceiptHash, $result)
}
