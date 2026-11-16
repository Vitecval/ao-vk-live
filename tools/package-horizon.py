"""Build the single-EXE Horizon preview; no network or publication performed."""
import argparse,hashlib,json,struct,shutil,re,zipfile,gzip
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def verify_full_manifest(core,resource):
 expected=json.loads(gzip.decompress(resource.read_bytes()))
 with zipfile.ZipFile(core) as archive:
  names=[n for n in archive.namelist() if n.endswith('/manifest.json') and len(n.split('/'))==2]
  if len(names)!=1:raise RuntimeError('The graphics bootstrap must contain one top-level manifest.')
  actual=json.loads(archive.read(names[0]).decode('utf-8-sig'))
 if actual!=expected:raise RuntimeError('The bundled Full manifest differs from the app texture catalogue. Rebuild the matching graphics bootstrap and cache before packaging.')
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/horizon-preview/minimal/Rubi-Ka Horizon Setup.exe');a=p.parse_args()
 source=ROOT/'build/horizon-preview/app/Rubi-Ka Horizon.exe'
 metadata=json.loads((ROOT/'apps/horizon/Assets/engine.json').read_text(encoding='utf8'))
 core=ROOT/('build/preview24/release-r'+str(metadata['revision']))/'ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip'
 pin={'sha256':metadata['fullSetupSha256']}
 assert sha(core)==pin['sha256']
 verify_full_manifest(core,ROOT/'apps/horizon/Assets/full-manifest.json.gz')
 assert source.read_bytes()[:2]==b'MZ'
 a.output.parent.mkdir(parents=True,exist_ok=True)
 with a.output.open('wb') as out,source.open('rb') as app,core.open('rb') as payload:
  shutil.copyfileobj(app,out);offset=out.tell();shutil.copyfileobj(payload,out)
  out.write(b'RKHCORE1'+struct.pack('<qq',offset,core.stat().st_size)+bytes.fromhex(pin['sha256']))
 version=re.search(r'public const string VersionTag="v([^"]+)"',(ROOT/'apps/horizon/UpdateService.cs').read_text(encoding='utf8')).group(1)
 report=dict(product='Rubi-Ka Horizon',version=version,state='release-candidate',published=False,
  executable=str(a.output.resolve()),sha256=sha(a.output),bytes=a.output.stat().st_size,
  engineRevision=metadata['revision'],engineSha256=pin['sha256'],liteTextureDownloads=False,fullPreparedCacheFiles=14345,
  requires='Windows 10/11 x64 with .NET Framework 4.8')
 (a.output.parent/'BUILD.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
 # Carry every permanent website/admin download in each stable release.
 plugin=a.output.parent/'rubika-horizon-reports-wordpress.zip'
 with zipfile.ZipFile(plugin,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
  for file in sorted((ROOT/'apps/wordpress/horizon-reports').glob('*')):
   if file.is_file():
    entry=zipfile.ZipInfo('rubika-horizon-reports/'+file.name,date_time=(2026,1,1,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED;entry.external_attr=0o644<<16;archive.writestr(entry,file.read_bytes())
 setup=a.output.parent/'WORDPRESS-SETUP.txt';shutil.copyfile(ROOT/'apps/wordpress/horizon-reports/README.txt',setup)
 (a.output.parent/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in [a.output,plugin,setup]),encoding='ascii')
if __name__=='__main__':main()
