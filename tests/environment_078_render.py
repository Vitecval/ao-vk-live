"""GPU regressions: snowy path contrast, short-lived prints and legacy sky overlay."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--cache-file',type=Path)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
exe=(a.build/'rvk_demo.exe').resolve();cache=a.cache_file.resolve() if a.cache_file else out/'pipelines.cache'
def run(name,args):
    d=out/name;d.mkdir(exist_ok=True)
    with (d/'run.log').open('w') as f:
        subprocess.run([str(exe),*args,'--size','768','512','--shot','result.bmp'],cwd=d,
            env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache)),stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
    log=(d/'run.log').read_text(errors='replace')
    assert not re.search('VUID-|VK_ERROR|device lost|allocation failed',log,re.I),name
    print(name,'rendered',flush=True)
    return np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(float),d
snow=['--snow-ground-fixture','--terrain-blend','2','--ground-rocks','0','--ground-pebbles','0','--ground-tess','0','--cliff-depth','0','--grass-combined','--sky-zone','4320','--fixed-clock','16.666667']
images={}
for name,extra in [('original',['--terrain-blend','0','--frames','160']),('hd',['--hd-ground','1','--frames','160']),('powder-off',['--hd-ground','1','--snow-powder','0','--frames','160']),('prints',['--hd-ground','1','--snow-stamp','--frames','160']),('faded',['--hd-ground','1','--snow-stamp','--frames','700'])]:
    images[name],d=run(name,snow+extra)
    if name!='original':
        dump=(d/'native-frame.txt').read_text()
        assert 'detail-masters=6; hd-masters=6' in dump,name+' master upload not ready'
        assert 'tiles=1' in dump,name+' missing snow identity'
        if name in ['prints','faded']:assert 'stamped=6;' in dump,name+' missing stamps'
contrast={name:float(im[360:480,40:150].mean()-im[360:480,600:720].mean()) for name,im in images.items()}
print('path contrast',contrast,flush=True)
for name in ['original','hd','prints']:assert contrast[name]>20,(name,'path erased',contrast)
fresh=np.abs(images['prints']-images['hd']);faded=np.abs(images['faded']-images['hd'])
print('print delta',fresh.max(),fresh.sum(),faded.max(),faded.sum(),flush=True)
assert np.count_nonzero(fresh>1)>20,'prints invisible'
assert faded.sum()<fresh.sum()*.25,'snow impressions did not fade'
authored,d=run('authored-snow',snow+['--snow-authored-fixture','--hd-ground','1','--frames','160'])
bare,d=run('authored-snow-no-powder',snow+['--snow-authored-fixture','--hd-ground','1','--snow-powder','0','--frames','160'])
assert np.abs(authored-bare).sum()>1000,'powder bypassed on installed snow albedo'
assert float(authored[360:480,40:150].mean()-authored[360:480,600:720].mean())>20,'authored path erased'
separate,d=run('authored-snow-two-passes',[arg for arg in snow if arg!='--grass-combined']+['--snow-authored-fixture','--hd-ground','1','--frames','160'])
assert float(separate[360:480,40:150].mean()-separate[360:480,600:720].mean())>20,'two-pass snow path lost'
skies={}
for name,extra in [('soft',['--soft-sky']),('soft-gloom',['--soft-sky','--gloomy-sky-fixture']),('soft-no-stars',['--soft-sky','--soft-sky-no-stars']),('stock',[]),('stock-gloom',['--gloomy-sky-fixture']),('soft-night',['--soft-sky','--visual-case','8'])]:
    skies[name],d=run(name,['--visual-case','9','--area-zone','4320','--fixed-clock','.000001','--frames','100',*extra])
assert np.array_equal(skies['soft'],skies['soft-gloom']),'gloom overlay leaked into Soft skies'
assert np.array_equal(skies['soft'],skies['soft-no-stars']),'Soft skies depends on stars'
assert not np.array_equal(skies['stock'],skies['stock-gloom']),'gloom test did not exercise the overlay'
# Penumbra intentionally keeps an open, bright Soft sky even at night (owner's
# r65 art direction). Verify this separately from ordinary outdoor night skies.
ordinary,d=run('ordinary-soft-night',['--visual-case','8','--area-zone','655','--soft-sky','--fixed-clock','.000001','--frames','100'])
assert skies['soft-night'][:180].mean()>ordinary[:180].mean()*1.3,'regional bright sky not active'
report=dict(result='passed',pathContrast=contrast,printDelta=float(fresh.sum()),fadedPrintDelta=float(faded.sum()),
    skyGloomSuppressed=True,softSkyIndependentOfStars=True,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
