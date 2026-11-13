"""Reproduce pinned local release archives on disposable GitHub Windows hosts.

No release is published here. Every upload must match the already verified local
archive SHA256, and all input paths and source artwork are independently pinned.
"""
import argparse,base64,ctypes,hashlib,json,os,re,shutil,subprocess,sys,time,urllib.request,urllib.error,zipfile,zlib
from pathlib import Path
REPO='Vitecval/ao-vk-live';RID=444758328;API='https://api.github.com/repos/'+REPO
TOKEN=os.environ['GH_TOKEN'];TEMP=Path(os.environ['RUNNER_TEMP'])/'preview24-package';TEMP.mkdir(exist_ok=True)
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):return None
strict=urllib.request.build_opener(NoRedirect())
def api(path):
 req=urllib.request.Request(API+path,headers={'Authorization':'Bearer '+TOKEN,'Accept':'application/vnd.github+json','User-Agent':'AO-VK-Preview24-Packaging'})
 with urllib.request.urlopen(req,timeout=120) as r:return json.load(r)
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def download(url,path,pin,private=False):
 if path.exists() and sha(path)==pin:return
 for attempt in range(3):
  try:
   headers={'User-Agent':'AO-VK-Preview24-Packaging'}
   if private:headers.update(Authorization='Bearer '+TOKEN,Accept='application/octet-stream')
   req=urllib.request.Request(url,headers=headers)
   try:r=strict.open(req,timeout=120)
   except urllib.error.HTTPError as e:
    if e.code not in (301,302,303,307,308):raise
    target=e.headers['Location']
    from urllib.parse import urlparse
    parsed=urlparse(target)
    assert parsed.scheme=='https' and (parsed.hostname=='github.com' or parsed.hostname.endswith('.githubusercontent.com')),parsed.hostname
    r=urllib.request.urlopen(urllib.request.Request(target,headers={'User-Agent':'AO-VK-Preview24-Packaging'}),timeout=120)
   with r,path.open('wb') as f:shutil.copyfileobj(r,f,1<<20)
   assert sha(path)==pin,('Download checksum',path.name)
   return
  except Exception:
   if attempt==2:raise
   time.sleep(2+attempt*3)
def upload(path,pin):
 state=api('/releases/'+str(RID));assert state['draft'],'Only the draft may receive files'
 existing=[x for x in state['assets'] if x['name']==path.name]
 if existing:
  assert len(existing)==1 and existing[0]['digest']=='sha256:'+pin and existing[0]['size']==path.stat().st_size
  print('Reused verified upload',path.name,flush=True);return
 size=path.stat().st_size;assert size<2**31 and sha(path)==pin
 with path.open('rb') as f:
  req=urllib.request.Request('https://uploads.github.com/repos/'+REPO+'/releases/'+str(RID)+'/assets?name='+path.name,data=f,headers={'Authorization':'Bearer '+TOKEN,'Content-Type':'application/zip' if path.suffix=='.zip' else 'text/plain','Content-Length':str(size),'User-Agent':'AO-VK-Preview24-Packaging'},method='POST')
  with strict.open(req,timeout=180) as r:assert r.status==201;item=json.load(r)
 assert item['size']==size and item['digest']=='sha256:'+pin,(path.name,item)
 print('Uploaded with matching GitHub digest',path.name,flush=True)
def extract_member(z,name,target,pin):
 assert sha_bytes(z,name)==pin,('Source member checksum',name)
 target.parent.mkdir(parents=True,exist_ok=True)
 with z.open(name) as src,target.open('wb') as dst:shutil.copyfileobj(src,dst,1<<20)
def sha_bytes(z,name):
 with z.open(name) as f:return hashlib.file_digest(f,'sha256').hexdigest()
def set_filetime(path,value):
 from ctypes import wintypes as w
 class FT(ctypes.Structure):_fields_=[('low',w.DWORD),('high',w.DWORD)]
 kernel=ctypes.WinDLL('kernel32',use_last_error=True)
 kernel.CreateFileW.argtypes=[w.LPCWSTR,w.DWORD,w.DWORD,w.LPVOID,w.DWORD,w.DWORD,w.HANDLE];kernel.CreateFileW.restype=w.HANDLE
 kernel.SetFileTime.argtypes=[w.HANDLE,ctypes.POINTER(FT),ctypes.POINTER(FT),ctypes.POINTER(FT)];kernel.SetFileTime.restype=w.BOOL
 kernel.CloseHandle.argtypes=[w.HANDLE]
 handle=kernel.CreateFileW(str(path),0x100,3,None,3,0,None)
 assert handle!=ctypes.c_void_p(-1).value,('Open source timestamp',path)
 stamp=FT(value&0xffffffff,value>>32)
 try:assert kernel.SetFileTime(handle,None,None,ctypes.byref(stamp)),('Set source timestamp',path)
 finally:kernel.CloseHandle(handle)
def archive(record,resolve):
 path=TEMP/record['name']
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=record['level']) as z:
  for item in record['entries']:
   source=resolve(item);assert source.stat().st_size==item['size']
   zi=zipfile.ZipInfo(item['name'],tuple(item['date']));zi.file_size=item['size'];zi.compress_type=item['compressType'];zi._compresslevel=record['level']
   zi.create_system=item['createSystem'];zi.create_version=item['createVersion'];zi.extract_version=item['extractVersion'];zi.internal_attr=item['internalAttr'];zi.external_attr=item['externalAttr'];zi.extra=base64.b64decode(item['extra']);zi.comment=base64.b64decode(item['comment'])
   with source.open('rb') as src,z.open(zi,'w') as dst:shutil.copyfileobj(src,dst,1024*8)
 assert path.stat().st_size==record['bytes'] and sha(path)==record['sha256'],('Archive differs from tested local build',record['name'],sha(path),record['sha256'])
 with zipfile.ZipFile(path) as z:assert z.testzip() is None
 return path
def main():
 p=argparse.ArgumentParser();p.add_argument('--part',required=True);args=p.parse_args()
 inputzip=TEMP/'input.zip';chunks=json.loads(os.environ['INPUT_CHUNKS'])
 with inputzip.open('wb') as joined:
  for chunk in sorted(chunks,key=lambda x:x['index']):
   piece=TEMP/('input-'+str(chunk['index'])+'.bin');download(API+'/releases/assets/'+str(chunk['assetId']),piece,chunk['sha256'],True)
   with piece.open('rb') as src:shutil.copyfileobj(src,joined,1<<20)
   piece.unlink()
 assert sha(inputzip)==os.environ['INPUT_SHA256'],'Combined build input checksum'
 with zipfile.ZipFile(inputzip) as z:
  plan=json.loads(z.read('input.json'))
  assert plan['releaseId']==RID and plan['version']=='0.6.0-preview.24'
  assert sys.version.split()[0]==plan['python'] and zlib.ZLIB_RUNTIME_VERSION==plan['zlib'],(sys.version,zlib.ZLIB_RUNTIME_VERSION)
  for name in z.namelist():
   assert name in {'input.json','prepare.exe','checksums.txt'} or re.fullmatch(r'(blobs/[a-f0-9]{64}|extras/[a-f0-9]{16}\.rvkm|deltas/[a-f0-9]{64}\.bin)',name),name
   target=TEMP/'input'/name;target.parent.mkdir(parents=True,exist_ok=True)
   with z.open(name) as src,target.open('wb') as dst:shutil.copyfileobj(src,dst,1<<20)
 inputzip.unlink();bundle=TEMP/'input';assert sha(bundle/'prepare.exe')==plan['preparerSha256']
 if args.part=='setup':records=[r for r in plan['archives'] if r['kind']=='setup']
 else:records=[r for r in plan['archives'] if r['name'].endswith('-materials-'+args.part+'.zip')]
 assert records and len(records)==(2 if args.part=='setup' else 1)
 remote=api('/releases/'+str(RID));assert remote['draft']
 if all(any(x['name']==r['name'] and x['digest']=='sha256:'+r['sha256'] and x['size']==r['bytes'] for x in remote['assets']) for r in records):print('All requested archives already verified');return
 source=TEMP/'source';cache=TEMP/'cache';source.mkdir(exist_ok=True);cache.mkdir(exist_ok=True)
 groups={}
 for rel in plan['sourceStamps']:
  match=re.match(r'^(randy-vk/packs/[^/]+/[0-9]+_[0-9]+)_',rel);assert match,rel
  groups.setdefault(match[1],[]).append(rel)
 def source_group(paths):
  first=next(x for x in paths if x);prefix=re.match(r'^(randy-vk/packs/[^/]+/[0-9]+_[0-9]+)_',first)[1];return groups[prefix]
 needed=set();cache_names=set()
 for record in records:
  if record['kind']=='material':
   for item in record['entries']:
    name=item['name']
    if name.endswith('.png'):needed.add(name)
    elif name.endswith('.rvkm'):
     key=Path(name).name;cache_names.add(key)
     if key in plan['recipes']:needed.update(source_group(plan['recipes'][key]))
 oldbase='https://github.com/'+REPO+'/releases/download/v0.6.0-preview.23/'
 # Extract only the images needed by this job. One old archive is on disk at a time.
 for part in plan['oldParts']:
  selected=needed.intersection(part['files'])
  if not selected:continue
  print('Fetching verified artwork',part['name'],len(selected),'images',flush=True)
  path=TEMP/part['name'];download(oldbase+part['name'],path,part['sha256'])
  with zipfile.ZipFile(path) as z:
   for rel in sorted(selected):extract_member(z,rel,source/rel,plan['assetPins'][rel])
  path.unlink();needed.difference_update(selected)
 setup=plan['oldSetup'];setupfile=TEMP/setup['name'];download(oldbase+setup['name'],setupfile,setup['sha256'])
 with zipfile.ZipFile(setupfile) as z:
  for rel in sorted(needed):extract_member(z,setup['folder']+'/'+rel,source/rel,plan['assetPins'][rel])
  extract_member(z,setup['folder']+'/LICENSE-TEXTURES.md',source/'LICENSE-TEXTURES.md',sha_bytes(z,setup['folder']+'/LICENSE-TEXTURES.md'))
  if args.part=='setup':
   for delta in plan['deltaFiles'].values():extract_member(z,delta['base'],source/'seed'/delta['baseSha256'],delta['baseSha256'])
   for record in records:
    for item in record['entries']:
     h=item['sha256']
     if h in plan['seedFiles']:extract_member(z,plan['seedFiles'][h],source/'seed'/h,h)
 setupfile.unlink()
 if cache_names:
  assert not Path('F:/').exists(),'The temporary runner already has an F: drive'
  drive=TEMP/'drive';drive.mkdir(exist_ok=True);subprocess.run(['subst','F:',str(drive)],check=True)
  try:
   client=Path(r'F:\AO_SInstall\Anarchy Online');requested=[]
   for key in sorted(cache_names):
    if key in plan['extraFiles']:
     shutil.copy2(bundle/plan['extraFiles'][key],cache/key);continue
    paths=plan['recipes'][key];first=next(x for x in paths if x);match=re.search(r'/([0-9]+)_([0-9]+)_',first);assert match
    requested.append('rdb:'+match[2]+' type:'+match[1])
    for rel in source_group(paths):
     target=client/rel
     if target.exists():continue
     target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/rel,target)
     set_filetime(target,plan['sourceStamps'][rel]['filetime'])
   capture=TEMP/'selected.txt';capture.write_text('\n'.join(requested))
   if requested:subprocess.run([str(bundle/'prepare.exe'),'--client',str(client),'--output',str(cache),'--capture',str(capture)],check=True)
  finally:subprocess.run(['subst','F:','/D'],check=True)
  for key in cache_names:assert sha(cache/key)==plan['assetPins']['randy-vk/cache/materials/'+key],('Prepared pixels/payload differs from verified local cache',key)
 def resolve(item):
  name=item['name']
  if args.part=='setup':
   h=item['sha256']
   if h in plan['deltaFiles']:
    delta=plan['deltaFiles'][h];path=source/'constructed'/h
    if not path.exists():
     path.parent.mkdir(exist_ok=True);old=(source/'seed'/delta['baseSha256']).read_bytes();data=(bundle/delta['data']).read_bytes()
     with path.open('wb') as f:
      for op,offset,n in delta['ops']:f.write((old if op=='copy' else data)[offset:offset+n])
   else:path=source/'seed'/h if h in plan['seedFiles'] else bundle/plan['blobFiles'][h]
   assert sha(path)==h;return path
  if name.endswith('.rvkm'):return cache/Path(name).name
  path=source/name
  if name!='LICENSE-TEXTURES.md':assert sha(path)==plan['assetPins'][name]
  return path
 for record in records:
  print('Building exact archive',record['name'],flush=True);path=archive(record,resolve);upload(path,record['sha256']);path.unlink()
 if args.part=='setup':
  sums=bundle/'checksums.txt';assert sha(sums)==plan['checksumsSha256'];upload(sums,plan['checksumsSha256'])
 print('PASS: every uploaded byte matches the tested local release',flush=True)
if __name__=='__main__':main()
