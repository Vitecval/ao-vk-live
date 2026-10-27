"""Shared Ground+ must retain the ordinary ground draw and add only stones."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,default=Path('build/windows-live'))
    p.add_argument('--output',type=Path,default=Path('build/shared-pebbles'))
    args=p.parse_args();root=Path(__file__).resolve().parents[1]
    exe=(args.build/'rvk_demo.exe').resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    common=['--native-ground-test','--ground-detail-fixture','--terrain-maps','--pixel-lighting','--sky-zone','655',
            '--fixed-clock','1000','--size','768','512','--frames','480']
    cases={
        'shared':['--terrain-blend','1','--terrain-off'],
        'plus':['--terrain-blend','2','--ground-pebbles','1'],
        'plus-zero':['--terrain-blend','2','--ground-pebbles','0'],
        'plus-no-effects':['--terrain-blend','2','--ground-pebbles','1','--sand-ripple','0','--dune-light','0','--ground-tess','0','--cliff-depth','0','--ground-rocks','0','--ground-blend','0','--terrain-off'],
        'native':['--native-on','--terrain-blend','0','--ground-pebbles','1'],
        'native-overrides':['--native-on','--terrain-blend','2','--ground-pebbles','1'],
    }
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(root/'build/shared-terrain-check/pipeline.cache'))
    metrics={};images={}
    for separate in [False,True]:
        for name,extra in cases.items():
            if separate and name.startswith('native'):continue
            key=('separate-' if separate else '')+name;dest=out/key;dest.mkdir(exist_ok=True)
            with (dest/'run.txt').open('w') as log:
                subprocess.run([str(exe),*common,*([] if separate else ['--grass-combined']),*extra,'--shot','result.bmp'],
                               cwd=dest,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(dest/'run.txt').read_text(errors='replace');dump=(dest/'native-frame.txt').read_text(errors='replace')
            assert not re.search('VUID-|VK_ERROR|device lost|AddressSanitizer',log,re.I),key
            m=re.search(r'native terrain: shared-plus (\d+); enabled (\d+); patches (\d+); patches/frame ([\d.]+); rocks (\d+); pebbles (\d+)',dump)
            assert m,key
            metrics[key]=dict(zip(['plus','enabled','patches','patches_per_frame','rocks','pebbles'],map(float,m.groups())))
            images[key]=np.array(Image.open(dest/'result.bmp').convert('RGB')).astype(int)
    deltas={}
    for prefix in ['','separate-']:
        a,b=prefix+'shared',prefix+'plus-zero'
        delta=int(abs(images[a]-images[b]).max());deltas[a+'/plus-zero']=delta
        assert delta<=1,(a,b,'base terrain lighting or geometry changed',delta)
        a,b=prefix+'plus',prefix+'plus-no-effects'
        delta=int(abs(images[a]-images[b]).max());deltas[a+'/no-effects']=delta
        assert delta<=1,(a,'Plus inherited ground effects',delta)
        assert metrics[a]['patches_per_frame']==0 and metrics[a]['rocks']==0,metrics[a]
        assert metrics[a]['pebbles']>0,('no pebble geometry',metrics[a])
        changed=int(np.any(abs(images[a]-images[prefix+'plus-zero'])>2,axis=2).sum())
        assert changed>100,('pebbles not visible',prefix,changed)
        deltas[a+'/visible_pebble_pixels']=changed
    delta=int(abs(images['native']-images['native-overrides']).max());deltas['GroundOn_override']=delta
    assert delta<=1,('GroundOn did not override Shared',delta)
    report={'result':'passed','metrics':metrics,'pixel_checks':deltas,'in_game_visual_verification':False}
    (out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
