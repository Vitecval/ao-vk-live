"""Regression for shared-VB index identity and restoration of r45 detail-only rocks."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
from preview23_sky_life_tests import pixels,different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pipeline',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);images={};rows={};dumps={}
cases=[('mesh-direct',19,49152,5955),('mesh-retained',19,16384,5955),('mesh-shared',19,81920,5955),
 ('hidden-direct',19,49154,5955),('hidden-retained',19,16386,5955),('hidden-shared',19,81922,5955),('turning-retained',19,16402,5955),
 ('detail-off',23,128,5955),('detail-on',23,129,5955),('legacy-shape-on',23,161,5955),('tile-rock',23,129,9863),('detail-moving',23,137,5955),
 ('character-off',23,132,5955),('character-on',23,133,5955),('unknown-off',23,128,0),('unknown-on',23,129,0),
 ('open-detail',23,65,5955),('open-legacy-shape',23,97,5955),('dense-detail',23,641,5955),('dense-legacy-shape',23,673,5955)]
for name,scene,toggle,identity in cases:
    folder=out/name;folder.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(a.pipeline.resolve()),AOVK_FIXTURE_ROCK_ID=str(identity),AOVK_ROCK_PACK=str(out/'no-external-pack'))
    with (folder/'run.log').open('w') as f:
        subprocess.run([str(build/'rvk_demo.exe'),'--visual-case',str(scene),'--visual-toggle',str(toggle),'--size','768','512',
          '--fixed-clock','16' if scene==19 else '.000001','--frames','140' if scene==19 else '1250','--shot','result.bmp'],cwd=folder,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
    log=(folder/'run.log').read_text(errors='replace');dump=(folder/('shadow-stability.txt' if scene==19 else 'visual-frame.txt')).read_text()
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|persistence failed',log,re.I),name
    assert 'rigid rock clusters: draws 0; rocks 0;' in dump and 'rock geometry: draws 0;' in dump,name
    if scene==19:assert re.search(r'\b2 remembered\b',dump),(name,'Lost indexed submesh')
    if scene==23 and toggle&1:assert "terrain: the cliffs' rock is in" in log,name
    assert 'shared Full material ready' not in log,name
    images[name]=pixels(folder/'result.bmp');dumps[name]=dump;rows[name]=dict(geometryDraws=0,rememberedCasters=2 if scene==19 else None)
    print(name,'passed',flush=True)
for ref,name in [('mesh-direct','mesh-retained'),('mesh-direct','mesh-shared'),('hidden-direct','hidden-retained'),('hidden-direct','hidden-shared'),('hidden-direct','turning-retained'),
 ('detail-on','legacy-shape-on'),('detail-on','tile-rock'),('character-off','character-on'),('unknown-off','unknown-on'),('open-detail','open-legacy-shape'),('dense-detail','dense-legacy-shape')]:
    delta=different(images[ref],images[name]);assert delta==0,(ref,name,delta);rows[name]['referenceChangedPixels']=delta
changed=different(images['detail-off'],images['detail-on']);assert changed>10000,changed
moving=max(abs(x-y) for row,other in zip(images['detail-on'],images['detail-moving']) for x,y in zip(row,other));assert moving<=1,moving
first=(Path(__file__).resolve().parents[1]/'build/preview24/rocks-r45/SOURCE-FILES.json');pins=json.loads(first.read_text());original=next(x['sha256'] for x in pins if x['path']=='rvk/shaders/rock_objects.glsl')
actual=hashlib.sha256((Path(__file__).resolve().parents[1]/'rvk/shaders/rock_objects.glsl').read_text().encode()).hexdigest();assert actual==original
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),cases=rows,detailChangedPixels=changed,turningMaxChannelDifference=moving,firstRockShaderSha256=actual,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print('All stability and original-detail cases passed',flush=True)
