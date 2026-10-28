"""Render distant terrain composition, cliff coverage and water contact fixtures."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,default=Path('build/windows-live'))
    p.add_argument('--output',type=Path,default=Path('build/distant-terrain'))
    a=p.parse_args();root=Path(__file__).resolve().parents[1]
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);exe=(a.build/'rvk_demo.exe').resolve()
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(root/'build/shared-terrain-check/pipeline.cache'))
    images={};timings={};logs={}
    def run(name,flags):
        d=out/name;d.mkdir(exist_ok=True)
        with (d/'run.txt').open('w') as f:
            subprocess.run([str(exe),'--size','768','512','--fixed-clock','1000','--frames','480',
                '--shot','result.bmp',*flags],cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
        text=(d/'run.txt').read_text(errors='replace');logs[name]=text
        assert not re.search('VUID-|VK_ERROR|AddressSanitizer|device lost',text,re.I),name
        images[name]=np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(int)
        assert images[name].std()>5,('blank scene',name)
        m=re.search(r'terrain window: gpu ms:([^\n]+)',text)
        if m:timings[name]=float(re.search(r'\| total ([\d.]+)',m[1])[1])
    terrain=['--native-ground-test','--ground-detail-fixture','--terrain-maps','--pixel-lighting','--hdr',
             '--sky-zone','655','--grass-combined','--terrain-blend','1','--ground-pebbles','0',
             '--ground-rocks','0','--ground-far-view','--profile-window']
    weather={'day':['--sun-dir','.55','-.7','.45'],
             'sunrise':['--sun-dir','-.6','-.12','-.5'],
             'sunset':['--sun-dir','.6','-.12','.5'],
             'overcast':['--sun-dir','.55','-.7','.45','--clouds','--cloud-cover','1','--weather-mode','2']}
    metrics={}
    for name,flags in weather.items():
        for value in [0,1]:run(f'{name}-{value}',[*terrain,*flags,'--terrain-far',str(value)])
        delta=abs(images[name+'-1']-images[name+'-0']);changed=np.any(delta>1,axis=2)
        assert changed.sum()>150,(name,'macro composition not visible',int(changed.sum()))
        assert delta.mean()<10,(name,'excessive scene change',float(delta.mean()))
        metrics[name]={'changed_pixels':int(changed.sum()),'mean_absolute_delta':float(delta.mean()),
                       'mean_brightness_ratio':float(images[name+'-1'].mean()/images[name+'-0'].mean())}
        assert .88<metrics[name]['mean_brightness_ratio']<1.12,(name,'lighting balance changed')
    run('day-repeat',terrain+weather['day']+['--terrain-far','1'])
    assert np.array_equal(images['day-1'],images['day-repeat']),'unstable distant material'
    for mode in [1,2]:
        run(f'untiled-{mode}',terrain+['--ground-untiled','--terrain-blend',str(mode)])
    assert abs(images['untiled-1']-images['untiled-2']).max()<=1,'Shared/Plus cliffs differ'
    run('untiled-no-cliffs',terrain+['--ground-untiled','--terrain-off'])
    cliff_pixels=int(np.any(abs(images['untiled-1']-images['untiled-no-cliffs'])>2,axis=2).sum())
    assert cliff_pixels>100,'runtime composite terrain cliff fallback not exercised'
    for name,flags in [('haze',['--far-haze','.6','--haze-start','350','--haze-end','1000']),
                       ('coast',['--water-coast','--sky-zone','655','--water-style','1','--sky-dome','8fb0cc','--profile-window']),
                       ('coast-sunset',['--water-coast','--sky-zone','655','--sun-dir','.6','-.12','.5']),
                       ('coast-overcast',['--water-coast','--sky-zone','655','--clouds','--cloud-cover','1','--weather-mode','2'])]:
        run(name,terrain+flags if name=='haze' else flags)
    assert not np.array_equal(images['haze'],images['day-1']),'haze not exercised'
    assert not np.array_equal(images['overcast-1'],images['day-1']),'overcast weather not exercised'
    coast=['--water-coast','--sky-zone','655','--water-style','1','--sky-dome','8fb0cc']
    for distance,view in [('near',[]),('far',['--water-mesh-at','0','0','0','--water-eye','20','160','-280'])]:
        for value in [0,1]:run(f'shore-{distance}-{value}',coast+view+['--terrain-far',str(value)])
        delta=abs(images[f'shore-{distance}-1']-images[f'shore-{distance}-0'])
        changed=int(np.any(delta>0,axis=2).sum())
        assert changed>25,('shore contact not exercised',distance,changed)
        metrics['shore-'+distance]={'changed_pixels':changed,'mean_absolute_delta':float(delta.mean())}
    assert timings['day-1']<timings['day-0']*1.5+.4,('excessive GPU cost',timings)
    report={'result':'passed','weather':metrics,'runtime_composite_cliff_pixels':cliff_pixels,
            'repeat_identical':True,'shared_plus_identical':True,'gpu_total_ms':timings,
            'in_game_visual_verification':False,'silhouette_geometry_changed':False}
    (out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
