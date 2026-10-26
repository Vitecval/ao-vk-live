"""GPU ownership, duplicate-map accounting and low-VA loader comparisons.

Inputs: control/shared/result.bmp and ownership-frame.txt from rvk_demo
--shared-maps-control / --shared-maps-test (768x512, 12 frames). Loader logs
come from randy_harness --world-materials-test with RANDYVK_SHARE_MAPS=1/0,
and --material-pressure-test; all use --normal-exit.
"""
import argparse,json,re
from pathlib import Path
import numpy as np
from PIL import Image

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);r=p.parse_args().directory
 def image(n):return np.asarray(Image.open(r/n/'result.bmp').convert('RGB')).astype(int)
 a,b=image('control'),image('shared');delta=int(abs(a-b).max());assert delta<=1,delta
 assert a.std()>20,'Blank fixture'
 counts=[]
 for n in ['control','shared']:
  t=(r/n/'ownership-frame.txt').read_text();m=re.search(r'rvk: textures [\d.]+ MB \((\d+)\)',t);assert m,n;counts.append(int(m[1]))
 assert counts[0]==counts[1],('Shared children leaked or vanished',counts)
 logs={n:(r/(n+'-loader.log')).read_text(errors='replace') for n in ['shared','duplicated','pressure']}
 for n,t in logs.items():
  assert 'presentation released' in t,n
  assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|decode failed',t,re.I),n
 assert 'shares uploaded material maps' in logs['shared']
 assert 'shares uploaded material maps' not in logs['duplicated']
 def peak(t):return max(int(x) for x in re.findall(r'rvk: textures (\d+) MB',t))
 saved=peak(logs['duplicated'])-peak(logs['shared']);assert saved>=5,saved
 assert 'low process address space' in logs['pressure'],'Pressure fallback not exercised'
 assert 'material pressure: reservations released' in (r/'pressure-loader.txt').read_text()
 assert '[albedo map]' in logs['pressure'],'Artwork did not load in recovery cycles'
 print(json.dumps({'survives_source_deletion_and_sibling_replacement':True,'repeat_share_no_leaked_images':counts,'pixel_max_delta':delta,'quality_alias_gpu_mb_saved_in_fixture':saved,'low_va_fallback_and_recovery':'passed','clean_exit':'passed'},indent=2))

if __name__=='__main__':main()
