"""Exercise every new ID through the renderer's native texture lookup harness."""
from pathlib import Path
import json,hashlib,os,re,shutil,subprocess
O=Path(__file__).resolve().parent;B=O.parent;F=O/'frontend';F.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for src in (B/'pedestal-200347-v1/frontend').glob('*.dll'):shutil.copy2(src,F/src.name)
for name in ('ao-vk.dll','randy_harness.exe'):shutil.copy2(B/'x86'/name,F/name)
if not (F/'randy-vk.cache').exists():shutil.copy2(B/'artwork-r15/frontend/randy-vk.cache',F/'randy-vk.cache')
(F/'randy-vk.ini').write_text('[Renderer]\nRVK_Preset=0\nRVK_RenderBg=1\nRVK_FrameGen=0\nRVK_Upscaler=0\nRVK_RenderScale=100\nRVK_ModernTex=2\nRVK_CharacterPack=1\nRVK_Grass=0\nRVK_SharedGroundDetail=0\nRVK_CacheFile=randy-vk.cache\n')
ground=json.loads((O/'GROUND-BAKE.json').read_text())['materials'];art=json.loads((O/'ART-BAKE.json').read_text())['materials'];dest=F/'randy-vk/packs/ao-cyber-world';dest.mkdir(parents=True,exist_ok=True)
for r in ground+art:
 for role,m in r['maps'].items():
  path=dest/Path(m['path']).name
  if path.exists() and sha(path)==m['sha256']:continue
  stage=path.with_suffix('.check-stage');os.link(m['path'],stage);os.replace(stage,path)
unit=subprocess.check_output([str(B/'x86/preview23_artwork_check.exe')],text=True);runs=[]
def run(part,kind):
 ids=[r['id'] for r in part];label='final-'+str(kind)+'-'+str(ids[0]);log=O/(label+'.renderer.log')
 env=dict(os.environ,RANDYVK_DDRAW='rvk',RANDYVK_TEST_MODULES='1',RANDYVK_THREADED='1',RANDYVK_LOG=str(log),RANDYVK_PIPELINE_CACHE=str(F/'randy-vk.cache'));env.pop('RANDYVK_CONSOLE_TEST',None)
 cmd=[str(F/'randy_harness.exe'),'--art-ids',','.join(map(str,ids)),'--art-type',str(kind),'--frames','120','--normal-exit']
 with (O/(label+'.out.log')).open('w') as f:subprocess.run(cmd,cwd=F,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120,check=True)
 text=log.read_text(errors='replace');text=text[text.rfind('renderer entry pid='):]
 assert not re.search(r'VK_ERROR|VUID-|Validation Error|device lost',text,re.I),label
 for id in ids:assert re.search(rf'RDB texture {kind}:{id} gets[^\r\n]*\[PBR material [^\r\n]*\[albedo map\]',text),(kind,id,'not selected by native lookup')
 runs.append({'type':kind,'ids':ids,'rendererLogSha256':sha(log)});print('PASS',kind,ids,flush=True)
for records,kind in [(art,1010004),(ground,1010006)]:
 for start in range(0,len(records),8):run(records[start:start+8],kind)
for kind in [1010016,1010017]:run(art[:8],kind)
for kind in [1010021,1010022]:run([next(r for r in ground if r['id']==id) for id in [100,174,752]],kind)
report={'result':'passed','rendererSha256':sha(F/'ao-vk.dll'),'worldMaterialsTested':len(art),'groundMaterialsTested':len(ground),'typedAliasesTested':[1010016,1010017,1010021,1010022],'guardCheck':unit.strip(),'cases':runs,'retailMeshFitCertified':False}
(O/'NATIVE-LOOKUP-CHECK.json').write_text(json.dumps(report,indent=2)+'\n');print('DONE all new sets',flush=True)
