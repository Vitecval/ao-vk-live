"""Exact Grid label contrast under strong emission, with an unrelated lamp control."""
import argparse,json,os,re,subprocess,time,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--family-evidence',type=Path);p.add_argument('--label-ids',type=int,nargs='+');a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
pack=Path('F:/AO_SInstall/Anarchy Online/randy-vk/packs/ao-cyber-world')
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache'))
rows={}
binary_hashes={name:hashlib.sha256((build/name).read_bytes()).hexdigest() for name in ['rvk_demo.exe','ao-vk.dll']}
def run(identity,source,glow):
 name=f'{identity}-glow-{glow}';d=out/name;d.mkdir(exist_ok=True)
 args=[str(build/'rvk_demo.exe'),'--grid-sign-test',str(identity),'--grid-sign-source',str(source),'--grid-sign-glow',str(glow),'--size','768','512','--frames','12','--fixed-clock','.000001','--proc-sky','0','--sky-zone','152','--sky-time','1000','--shot','result.bmp']
 with (d/'run.log').open('w') as log:subprocess.run(args,cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
 log=(d/'run.log').read_text(errors='replace');assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
 im=np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(int)
 rows[name]=dict(range=[int(im.min()),int(im.max())],mean=float(im.mean()))
 print(name,rows[name],flush=True);return im
family=json.loads(a.family_evidence.read_text()) if a.family_evidence else [dict(id=i,current=str(pack/f'1010004_{i}_d.png')) for i in [205337,125777]]
if a.label_ids:
 assert set(a.label_ids).issubset({r['id'] for r in family}), 'Requested labels lack reviewed family evidence'
 family=[r for r in family if r['id'] in a.label_ids]
for entry in family:
 identity=entry['id'];source=Path(entry['current']);assert source.is_file(),('Missing reviewed label',identity)
 if entry.get('current_sha256'):assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['current_sha256'],('Reviewed label changed',identity)
 off=run(identity,source,0);on=run(identity,source,4)
 face=(slice(240,330),slice(210,590));lamp=(slice(135,178),slice(160,205))
 delta=np.abs(on-off);face_delta=delta[face]
 # Small destination text can occupy less than a tenth of the source panel;
 # use tails that still sample the actual glyph strokes on those labels.
 contrast=float(np.percentile(on[face],98)-np.percentile(on[face],2))
 lamp_changed=int(np.count_nonzero(np.any(delta[lamp]>5,axis=2)))
 metrics=dict(face_mean_delta=float(face_delta.mean()),face_p99_delta=float(np.percentile(face_delta,99)),glyph_channel_contrast=contrast,unrelated_lamp_changed_pixels=lamp_changed)
 rows[f'{identity}-metrics']=metrics;print(identity,metrics,flush=True)
 assert contrast>35,('Missing glyph/background contrast',identity,metrics)
 assert metrics['face_mean_delta']<5,('Label still responds strongly to glow',identity,metrics)
 assert lamp_changed>100,('Ordinary emitter did not exercise actual glow',identity,metrics)
source=pack/'1010004_205337_d.png';off=run(0,source,0);on=run(0,source,4)
control=float(np.abs(on-off)[240:330,210:590].mean());rows['ordinary_face_glow_mean_delta']=control
assert control>10,('Generic identity failed to exercise same-image glow path',control)
assert all(hashlib.sha256((build/name).read_bytes()).hexdigest()==value for name,value in binary_hashes.items()),'Shared build outputs changed during validation'
report=dict(result='passed',checked_label_count=len(family),familyEvidenceSha256=hashlib.sha256(a.family_evidence.read_bytes()).hexdigest() if a.family_evidence else None,checks=rows,rendererSha256=binary_hashes['ao-vk.dll'],demoSha256=binary_hashes['rvk_demo.exe'],buildOutputsStable=True,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
