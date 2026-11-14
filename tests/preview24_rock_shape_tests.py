"""Actual geometry, open-border protection, movement and non-periodic stone."""
import argparse,hashlib,json,math,os,re,subprocess
from pathlib import Path
from preview23_sky_life_tests import pixels,different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pipeline',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);images={};dumps={};logs={};rows={}
cases=[('low-surface',129,5955),('low-shape',161,5955),('moving',169,5955),('baked-shape',177,5955),
       ('open-surface',65,5955),('open-shape',97,5955),('character-surface',133,5955),('character-shape',165,5955),
       ('unknown-surface',129,0),('unknown-shape',161,0),('wall',257,5955),
       ('dense-surface',641,5955),('dense-shape',673,5955)]
for name,toggle,identity in cases:
    folder=out/name;folder.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(a.pipeline.resolve()),AOVK_FIXTURE_ROCK_ID=str(identity))
    with (folder/'run.log').open('w') as log:
        subprocess.run([str(build/'rvk_demo.exe'),'--visual-case','23','--visual-toggle',str(toggle),'--size','768','512','--fixed-clock','.000001','--frames','1250','--shot','result.bmp'],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
    text=(folder/'run.log').read_text(errors='replace');assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',text,re.I),name
    images[name]=pixels(folder/'result.bmp');dumps[name]=(folder/'visual-frame.txt').read_text();logs[name]=text
    print(name,'rendered',flush=True)
for name in ['low-shape','baked-shape','open-shape']:
    assert re.search(r'rock geometry: draws 1;',dumps[name]),(name,dumps[name][-500:])
for name in ['low-surface','character-shape','unknown-shape']:
    assert 'rock geometry: draws 0;' in dumps[name],name
rows['geometryChangedPixels']=different(images['low-surface'],images['low-shape']);assert rows['geometryChangedPixels']>1000,rows
for left,right in [('character-surface','character-shape'),('unknown-surface','unknown-shape')]:
    delta=different(images[left],images[right]);assert delta==0,(left,right,delta)
rows['turningMaximumChannelDifference']=max(abs(x-y) for row,other in zip(images['low-shape'],images['moving']) for x,y in zip(row,other))
assert rows['turningMaximumChannelDifference']<=1,rows
def mask(img):
    background=img[0][:3]
    return {(x,y) for y,row in enumerate(img) for x in range(768) if max(abs(row[x*3+c]-background[c]) for c in range(3))>8}
def bounds(m):return min(x for x,y in m),min(y for x,y in m),max(x for x,y in m),max(y for x,y in m)
rows['closedSilhouetteChangedPixels']=len(mask(images['low-shape'])^mask(images['low-surface']));assert rows['closedSilhouetteChangedPixels']>100,rows
rows['closedSurfaceBounds']=bounds(mask(images['low-surface']));rows['closedShapeBounds']=bounds(mask(images['low-shape']))
assert rows['closedSurfaceBounds']!=rows['closedShapeBounds'],'The actual silhouette must change'
rows['openSurfaceBounds']=bounds(mask(images['open-surface']));rows['openShapeBounds']=bounds(mask(images['open-shape']))
assert rows['openSurfaceBounds']==rows['openShapeBounds'],rows
assert 'rock geometry: draws 15;' in dumps['dense-shape']
assert 'topology entries 1' in dumps['dense-shape'],'Instances should share topology metadata'
# A 3.6 m tile projects to ~123 pixels on this plane. Reject a strong fixed
# repetition at that exact horizontal period, across several stone-only rows.
def corr(values,lag):
    left=values[:-lag];right=values[lag:];ma=sum(left)/len(left);mb=sum(right)/len(right)
    return sum((x-ma)*(y-mb) for x,y in zip(left,right))/max(math.sqrt(sum((x-ma)**2 for x in left)*sum((y-mb)**2 for y in right)),1e-6)
correlations=[]
for y in [220,260,300,340,380]:
    row=images['wall'][y];values=[sum(row[x*3:x*3+3])/3 for x in range(60,708)]
    correlations.append(corr(values,123))
rows['tilePeriodCorrelations']=correlations;assert sum(correlations)/len(correlations)<.85,rows
rows['gpuSamples']={name:[s.split('gpu ms:')[-1] for s in logs[name].splitlines() if 'gpu ms:' in s][-2:] for name in ['low-surface','low-shape','dense-surface','dense-shape']}
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),cases=rows,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
