"""Build Preview 24 Lite and Full, including portable prepared materials."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];VERSION='0.6.0-preview.24'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def module(name,file):
 spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/file);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def stage(a):
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 assert not (out/'PACKAGE-AUDIT.json').exists(),'A completed release is immutable'
 source_manifest=out/'SOURCE-FILES.json';assert source_manifest.is_file(),'Freeze reviewed source before packaging'
 seed=a.seed.resolve();library=a.client.resolve();cache=a.cache.resolve();build=a.build.resolve()
 previous=read(seed/f'ao-vk-live-0.6.0-preview.23-windows-x86-full/manifest.json')
 cache_receipt=read(cache.parent/'MATERIAL-CACHE.json');pins={x['name']:x for x in cache_receipt['files']}
 recipes=a.recipes.read_bytes();assert recipes.startswith(b'RVK-RECIPES-1\n')
 recipe_keys={row.split(b'\t',1)[0].decode()+'.rvkm' for row in recipes.splitlines()[1:]}
 assert recipe_keys<=pins.keys()
 launchers=module('launchers','package-release.py')
 for edition in ['lite','full']:
  folder=out/f'ao-vk-live-{VERSION}-windows-x86-{edition}';folder.mkdir(exist_ok=True)
  seed_folder=seed/f'ao-vk-live-0.6.0-preview.23-windows-x86-{edition}'
  m=read(seed_folder/'manifest.json');assets={}
  def put(src,rel,expected=None):
   src=Path(src);dst=folder/rel;dst.parent.mkdir(parents=True,exist_ok=True)
   h=sha(src)
   if expected:assert h==expected,('Source integrity',str(src))
   if dst.exists():assert sha(dst)==h,('Interrupted stage differs; preserve and inspect',str(dst));return h
   if src.suffix.lower() in {'.png','.rvkm'} and src.drive.lower()==dst.drive.lower():os.link(src,dst)
   else:shutil.copy2(src,dst)
   return h
  # Seed notices, documentation and non-texture renderer assets. Never copy a
  # replacement PNG into Lite, even if a previous Lite used to contain it.
  refreshed={'manifest.json','README.md','RELEASE-NOTES.md','PLAYTEST.md','Install.cmd','Update.cmd','Uninstall.cmd','Reset settings.cmd'}
  scripts=['setup-safety.ps1','install-live.ps1','setup-live.ps1','reset-settings.ps1','setup-progress.ps1','material-download.ps1','material-parts.ps1','material-cache.ps1','large-address-aware.ps1','mesh-index.ps1','startup-report.ps1']
  refreshed.update(scripts);refreshed.update(['ao-vk.dll','ddraw.dll','ao-vk-cache-warm.exe'])
  for p in sorted(seed_folder.rglob('*')):
   if not p.is_file():continue
   rel=p.relative_to(seed_folder).as_posix()
   if rel in refreshed or rel.startswith(('randy-vk/packs/','randy-vk/cache/','.ao-vk-downloads/')):continue
   put(p,rel)
   if rel in m.get('assets',{}):assets[rel]=m['assets'][rel]
  for name in ['ao-vk.dll','ddraw.dll','ao-vk-cache-warm.exe']:put(build/name,name)
  for name in scripts:put(ROOT/'tools'/name,name)
  for action in ['Install','Update','Uninstall','Reset settings']:
   dst=folder/(action+'.cmd');data=launchers.launcher(action)
   if dst.exists():assert dst.read_bytes()==data
   else:dst.write_bytes(data)
  for name in ['README.md','RELEASE-NOTES.md','PLAYTEST.md']:put(ROOT/'docs/preview24.md',name)
  put(ROOT/'docs/preview24.md','docs/preview24.md')
  for p in sorted((ROOT/'artwork/loading').glob('*.png')):
   rel='randy-vk/loading/'+p.name
   # Loading art in the seed may be older; avoid writing through hardlinks.
   dst=folder/rel
   if dst.exists() and sha(dst)!=sha(p):dst.unlink()
   assets[rel]=put(p,rel)
  for key in ['materialParts','materialDownload','materialSourceVersion','preparedMaterials','iccArtwork','candidateRevision','artworkRefinements']:
   m.pop(key,None)
  if edition=='full':
   # Keep pinned license/provenance metadata, then use complete current pack
   # catalogues. Prefer an identical existing C: copy to duplicate disk use.
   for rel,h in previous['assets'].items():
    if rel.startswith('randy-vk/packs/') and not rel.endswith(('.png','/manifest.json')):
     assets[rel]=put(seed/f'ao-vk-live-0.6.0-preview.23-windows-x86-full'/rel,rel,h)
   for catalog in sorted((library/'randy-vk/packs').glob('*/manifest.json')):
    rel=catalog.relative_to(library).as_posix();assets[rel]=put(catalog,rel)
    for name,h in read(catalog)['files'].items():
     assert Path(name).name==name and name.endswith('.png'),name
     rel=(catalog.parent/name).relative_to(library).as_posix();old=seed/f'ao-vk-live-0.6.0-preview.23-windows-x86-full'/rel
     src=old if old.is_file() and sha(old)==h else catalog.parent/name
     assets[rel]=put(src,rel,h)
    print('STAGED',catalog.parent.name,len(read(catalog)['files']),flush=True)
   for i,(name,record) in enumerate(sorted(pins.items()),1):
    assert name==Path(name).name and len(name)==21 and name.endswith('.rvkm')
    assets['randy-vk/cache/materials/'+name]=put(cache/name,'randy-vk/cache/materials/'+name,record['sha256'])
    if i%2000==0:print('CACHE',i,'/',len(pins),flush=True)
   assets['randy-vk/cache/materials/recipes.tsv']=put(a.recipes,'randy-vk/cache/materials/recipes.tsv')
   m['preparedMaterials']=dict(format=1,catalog='randy-vk/cache/materials/recipes.tsv',payloadFormat=4,files=len(pins),recipes=len(recipe_keys),portable=True)
   m['materialSourceVersion']=VERSION
  else:
   assert not list((folder/'randy-vk').glob('packs/**/*'))
  m.update(version=VERSION,build='ao-vk-live-'+VERSION,edition=edition,optionalTexturePack=edition=='full',
   setupRevision=8,livingIncluded=False,candidateStatus='release',publicDistributionApproved=True,
   sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
   sourceManifestSha256=sha(source_manifest),publicAssetScope='Generated replacement artwork, portable material data and embedded renderer assets; no original client DLLs or private Living exports.')
  prior=[previous['addon'],*previous['previousAddons']]
  for p in sorted((ROOT/'build/preview23').glob('atmosphere-r*/candidate/ao-vk.dll')):
   item={'ao-vk.dll':sha(p),'ddraw.dll':sha(build/'ddraw.dll')}
   if item not in prior:prior.insert(0,item)
  m['previousAddons']=prior;m['addon']={n:sha(folder/n) for n in ['ao-vk.dll','ddraw.dll']}
  m['cacheWarmup']=dict(helper='ao-vk-cache-warm.exe',sha256=sha(folder/'ao-vk-cache-warm.exe'),relativeCache='auto')
  m.pop('setupSourceCommit',None);m['assets']=assets
  m['payload']={p.relative_to(folder).as_posix():sha(p) for p in sorted(folder.rglob('*')) if p.is_file() and p.relative_to(folder).as_posix() not in assets and p!=folder/'manifest.json'}
  m['payload'].update(assets);m['installBytes']=sum((folder/n).stat().st_size for n in set(assets)|set(m['addon']))
  write(folder/'manifest.json',m);print('READY',edition,len(assets),'assets',flush=True)
 write(out/'STAGED-INSTALLERS.json',dict(version=VERSION,result='passed',preparedFiles=len(pins),recipes=len(recipe_keys),sourceManifestSha256=sha(source_manifest)))
def archives(a):
 out=a.output.resolve();records=[]
 lite=out/f'ao-vk-live-{VERSION}-windows-x86-lite';full=out/f'ao-vk-live-{VERSION}-windows-x86-full'
 m=read(lite/'manifest.json');assert not m.get('materialParts') and not m.get('materialDownload') and not m.get('preparedMaterials')
 assert not any(n.startswith(('randy-vk/packs/','randy-vk/cache/','randy-vk/materials/')) for n in m['payload'])
 target=out/(lite.name+'.zip')
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for rel in ['manifest.json',*sorted(m['payload'])]:z.write(lite/rel,lite.name+'/'+rel)
 with zipfile.ZipFile(target) as z:assert z.testzip() is None
 print('ARCHIVE Lite verified',flush=True)
 products=module('parts24','material-archives.py').build_full_setup(full,VERSION,essential=m['assets'])
 for p in [target,*products]:records.append(dict(name=p.name,bytes=p.stat().st_size,sha256=sha(p)))
 write(out/'PACKAGE-AUDIT.json',dict(version=VERSION,result='passed',products=records,liteTextureAssets=0,liteMaterialDownloads=False,fullPreparedMaterials=read(full/'manifest.json')['preparedMaterials']))
 (out/'SHA256SUMS.txt').write_text(''.join(f"{x['sha256']}  {x['name']}\n" for x in records))
 print('PACKAGED',len(records),'archives;',sum(x['bytes'] for x in records),'bytes',flush=True)
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--archives',action='store_true')
 for name in ['seed','client','cache','recipes','build']:p.add_argument('--'+name,type=Path)
 a=p.parse_args()
 if a.archives:archives(a)
 else:
  assert all(getattr(a,k) for k in ['seed','client','cache','recipes','build']);stage(a)
if __name__=='__main__':main()
