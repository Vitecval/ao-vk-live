from pathlib import Path
import json,hashlib,subprocess,os,re,sys
O=Path(__file__).resolve().parent;B=O.parent;sys.path.insert(0,str(B/'python-deps'))
from PIL import Image
import numpy as np
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=json.loads((O/'GROUND-BAKE.json').read_text())['materials']+json.loads((O/'ART-BAKE.json').read_text())['materials'];maps=0
for r in rows:
 for v in r['maps'].values():
  p=Path(v['path']);assert sha(p)==v['sha256'];im=Image.open(p);im.load();assert list(im.size)==v['size'];maps+=1
listing=O/'new-materials.txt';listing.write_text('\n'.join(str(O/'runtime/randy-vk/packs/ao-cyber-world'/r['identity'].replace(':','_')) for r in rows)+'\n')
d=O/'resident-check';d.mkdir(exist_ok=True)
with (d/'run.log').open('w') as f, (d/'stderr.log').open('w') as e:subprocess.run([str(B/'x86/rvk_demo.exe'),'--check-material-list',str(listing),'--size','640','480'],cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(B/'overhaul-final-pass/frontend/randy-vk.cache')),stdout=f,stderr=e,check=True,timeout=600)
log=(d/'run.log').read_text(errors='replace');assert f'MATERIAL CHECK {len(rows)} failed 0' in log
sizes=[tuple(map(int,m)) for m in re.findall(r'RESIDENT .*? (\d+)x(\d+) \d+ bytes',log)];assert sizes==[tuple(r['maps']['d']['size']) for r in rows],('Resident map dimensions differ',len(sizes))
assert not re.search(r'VK_ERROR|VUID-|Validation Error|device lost',log+(d/'stderr.log').read_text(errors='replace'),re.I)
report={'result':'passed','materialSetsChecked':len(rows),'decodedMapsVerified':maps,'residentDiffuseSizesVerified':len(sizes),'allNewDiffuseMapsResidentAtExportSize':True,'totalNewDiskBytes':sum(v['bytes'] for r in rows for v in r['maps'].values()),'nativeLookupRendererSha256':sha(B/'x86/ao-vk.dll'),'residentHarnessSha256':sha(B/'x86/rvk_demo.exe'),'perceivedSharpnessOrRetailMeshFitCertified':False}
(O/'EXPORT-CHECK.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
