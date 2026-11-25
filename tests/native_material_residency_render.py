"""Visible bindless native tiles survive pressure; an unused HD map is retired."""
import argparse,json,os,re,subprocess
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
args=['--shared-terrain-test','--terrain-pebbles','--terrain-blend','2',
      '--hd-ground','1','--terrain-mixed-hd-fixture','1','--native-residency-check',
      '--size','768','512','--frames','420','--fixed-clock','1000','--shot','result.bmp']
env=dict(os.environ,RANDYVK_TEST_MODULES='1',RANDYVK_TEST_MATERIAL_CACHE_MB='1',
         RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache'))
with (out/'run.log').open('w') as f:
    subprocess.run([str((a.build/'rvk_demo.exe').resolve()),*args],cwd=out,env=env,
                   stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
log=(out/'run.log').read_text(errors='replace')
assert not re.search(r'VUID-|VK_ERROR|device lost|allocation failed',log,re.I)
assert 'retired 1 cold HD sets' in log,'Pressure fixture did not retire the unused map'
assert re.search(r'native-only residency: state 2, last sampled (42[01])',log),log[-1000:]
report=dict(result='passed',visibleNativeOnlyMapReady=True,unusedMapEvicted=True,
            frames=420,materialCacheLimitMB=1,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
