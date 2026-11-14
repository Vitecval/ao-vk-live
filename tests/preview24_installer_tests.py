"""Actual setup, portable payload and edition-switch checks in disposable clients."""
import hashlib,json,os,shutil,subprocess,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 out=ROOT/'build/preview24'/('installer-'+uuid.uuid4().hex[:8]);out.mkdir()
 host=Path(os.environ['WINDIR'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
 catalog=(ROOT/'build/preview24/recipes.tsv').read_text().splitlines()
 recipe=next(x for x in catalog[1:] if '/1010004_200348_n.png' in x)
 fields=recipe.split('\t');cache='randy-vk/cache/materials/'+fields[0]+'.rvkm'
 package=out/'Full setup';client=out/'AO [personal]';package.mkdir();client.mkdir()
 def put(p,data):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 put(client/'randy31.dll',b'original');put(client/'AnarchyOnline.exe',b'dummy never run');put(client/'randy-vk.ini',b'[Renderer]\r\nPersonal=17\r\n')
 for n in ['ao-vk.dll','ddraw.dll']:put(package/n,('fixture '+n).encode())
 assets={}
 for relative in set(filter(None,fields[1:])):
  src=Path(r'F:\AO_SInstall\Anarchy Online')/relative;dst=package/relative;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
  os.utime(dst,(1700000000,1700000000)) # Deliberately unlike the prepared source stamps.
  assets[relative]=sha(dst)
 p=package/cache;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'build/preview23/atmosphere-r44/prepared-materials-final'/Path(cache).name,p);assets[cache]=sha(p)
 rel='randy-vk/cache/materials/recipes.tsv';put(package/rel,('RVK-RECIPES-1\n'+recipe+'\n').encode());assets[rel]=sha(package/rel)
 loading='randy-vk/loading/test.png';put(package/loading,b'loading');assets[loading]=sha(package/loading)
 for name in ['rock_face_03_diff_2k.jpg','rock_face_03_nor_gl_2k.jpg','rock_face_03_arm_1k.jpg','rock-face-03-source.json']:
  relative='randy-vk/packs/EnhancedRocks/'+name
  put(package/relative,('owned surface fixture '+name).encode());assets[relative]=sha(package/relative)
 m=dict(product='AO-VK Live',version='0.6.0-preview.24',build='fixture24',edition='full',client={'randy31.dll':sha(client/'randy31.dll')},addon={n:sha(package/n) for n in ['ao-vk.dll','ddraw.dll']},assets=assets,preparedMaterials=dict(format=1,catalog=rel))
 def save():(package/'manifest.json').write_text(json.dumps(m))
 def run(tag,script,good=True):
  p=out/(tag+'.ps1');p.write_text("$ErrorActionPreference='Stop'\nfunction global:Get-Process {param($Name,$ErrorAction)}\n"+script,encoding='utf-8-sig')
  r=subprocess.run([str(host),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(p)],capture_output=True,text=True,timeout=60)
  (out/(tag+'.log')).write_text(r.stdout+r.stderr)
  assert (r.returncode==0)==good,(tag,r.stdout,r.stderr)
 def invoke(options=''):
  return f"& '{ROOT}/tools/install-live.ps1' -ClientDir '{client}' -PackageDir '{package}' {options}\n"
 def snapshot():return {p.relative_to(client).as_posix():sha(p) for p in client.rglob('*') if p.is_file()}
 save();run('install',invoke())
 index=client/'randy-vk/cache/materials/bindings.tsv';assert index.is_file()
 receipt=json.loads((client/'ao-vk-live.install.json').read_text());assert receipt['assets']['randy-vk/cache/materials/bindings.tsv']==sha(index)
 checker=ROOT/'build/preview23/x86/material_cache_check.exe';prefix=client/'randy-vk/packs/ao-cyber-world/1010004_200348'
 def probe(good=True):
  r=subprocess.run([str(checker),'--probe',str(prefix),str(index.parent)],capture_output=True,text=True)
  (out/('probe-good.log' if good else 'probe-stale.log')).write_text(r.stdout+r.stderr)
  assert (r.returncode==0)==good,(r.stdout,r.stderr)
 probe();run('update',invoke('-Upgrade'));probe()
 # A broken catalogue fails before the first committed game file changes.
 before=snapshot();put(package/rel,b'RVK-RECIPES-1\nbroken\n');m['assets'][rel]=sha(package/rel);save()
 run('bad-catalog',invoke('-Upgrade'),False);assert snapshot()==before
 put(package/rel,('RVK-RECIPES-1\n'+recipe+'\n').encode());m['assets'][rel]=sha(package/rel);save()
 albedo=client/'randy-vk/packs/ao-cyber-world/1010004_200348_d.png';stamp=albedo.stat();os.utime(albedo,(stamp.st_atime,stamp.st_mtime+7));probe(False)
 run('rebind-existing',invoke('-Upgrade'));probe()
 # Lite rejects accidental download metadata before a request can be made.
 full=m.copy();m={**m,'edition':'lite','assets':{loading:assets[loading]},'optionalTexturePack':False};m.pop('preparedMaterials');m['materialDownload']={'baseUrl':'https://invalid.invalid/'};save();before=snapshot()
 run('lite-download-refused',invoke('-Upgrade'),False);assert snapshot()==before
 m.pop('materialDownload');save();put(client/'randy-vk/packs/personal/keep.png',b'personal')
 run('full-to-lite',invoke('-Upgrade'))
 assert not list((client/'randy-vk/cache/materials').glob('*.rvkm')) and not index.exists()
 assert not list((client/'randy-vk/packs/ao-cyber-world').glob('*.png'))
 assert not any((client/'randy-vk/packs/EnhancedRocks').glob('*'))
 assert not (package/'.ao-vk-downloads').exists() and (client/loading).is_file()
 assert (client/'randy-vk.ini').read_bytes()==b'[Renderer]\r\nPersonal=17\r\n'
 m=full;save();run('lite-to-full',invoke('-Upgrade'));probe()
 run('uninstall',invoke('-Uninstall'));assert not index.exists() and not (client/'ao-vk.dll').exists()
 assert not any((client/'randy-vk/packs/EnhancedRocks').glob('*'))
 assert (client/'randy-vk.ini').is_file() and (client/'randy-vk/packs/personal/keep.png').read_bytes()==b'personal'
 run('reset',f"& '{ROOT}/tools/reset-settings.ps1' -ClientDir '{client}'\n");run('reset-again',f"& '{ROOT}/tools/reset-settings.ps1' -ClientDir '{client}'\n")
 assert not (client/'randy-vk.ini').exists()
 report=dict(result='passed',portablePathsAndTimestamps=True,pixelsMatchFreshDecode=True,staleSourcesRejected=True,fullLiteSwitches=True,liteDownloads=False,failedBindingAtomic=True,settingsPreserved=True,resetIdempotent=True,fixture=str(out))
 (ROOT/'build/preview24/INSTALLER-CHECK.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
