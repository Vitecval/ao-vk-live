"""Compare actual GPU captures for shared terrain and optional daylight grading.

Input folders in DIRECTORY: continuity-flat/turn/off, mask-on/off, login-on/off,
on/off, day0/day08, each with result.bmp and run.txt. Terrain fixtures use
rvk_demo --shared-terrain-test with --terrain-blend 1, --terrain-turns,
--terrain-mask, or --no-world-playfield; on/off use --global-terrain-test.
The daylight pair uses --mineral-materials-test --hdr --sky-zone 655
--sun-dir .5 .6 .6 --soft-highlights .75 and --day-look 0/.8.
"""
import argparse,json,re
from pathlib import Path
import numpy as np
from PIL import Image

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);r=p.parse_args().directory
 def image(n):return np.asarray(Image.open(r/n/'result.bmp').convert('RGB')).astype(float)
 flat,turn,old=map(image,('continuity-flat','continuity-turn','continuity-off'))
 assert abs(flat-turn).max()<=1,'World detail changed with cell rotation'
 assert abs(flat-old).mean()>2,'Shared detail was not rendered'
 assert flat[30:480,160:600].std()>5,'Blank/shared material fixture'
 assert abs(image('login-on')-image('login-off')).max()==0,'Login art changed'
 a,b=image('mask-off'),image('mask-on')
 def classes(c):
  red,green,blue=c.transpose(2,0,1);lum=.3*red+.59*green+.11*blue
  grass=(green-np.maximum(red,blue))/np.maximum(lum,1)>.09
  warm=(red-blue)/np.maximum(lum,1)>.18
  return np.where(grass,1,np.where(warm,2,3))
 roi=np.s_[30:480,160:600]
 stable=(classes(a)[roi]==classes(b)[roi]).mean()
 assert stable>.97,('Original path/biome shapes changed',stable)
 assert abs(a-b).mean()>2,'No material replacement in transition fixture'
 assert abs(image('on')-image('off')).mean()>2,'Live combined ground omitted shared detail'
 day0,day1=image('day0'),image('day08');delta=day1-day0
 dark=day0.max(2)<110
 assert delta[dark].mean()>12,'Daylight control did not lift dark regions'
 assert abs(delta[20:40,20:150]).max()==0,'Daylight grading changed interface'
 bright0,bright1=image('bright0'),image('bright08')
 assert (bright1.max(2)>250).sum()<=(bright0.max(2)>250).sum(),'Daylight increased clipped highlights'
 assert abs(image('mask-on')-image('separate-mask')).max()<=2,'Separate and combined ground differ'
 loader=(r/'terrain-loader.log').read_text(errors='replace')
 first=loader.split('shared terrain materials: off;',1)[0]
 assert 'RDB texture 1010006:18 gets' not in first,'Legacy terrain pack loaded in shared mode'
 assert 'RDB texture 1010004:295610 gets' in first,'Prop pack lost its artwork'
 assert 'RDB texture 1010006:18 gets' not in loader,'Partial road repaint returned with shared detail off'
 for identity in (17,21,22):
  assert re.search(rf'RDB texture (1010006|1010021|1010022):{identity} gets',first),('Authored city ground missing',identity)
 assert 'RDB texture 1010006:17 shares uploaded material maps' in loader,'Deck quality copies did not share maps'
 assert 'presentation released' in loader,'Loader did not shut down cleanly'
 assert 'preset checks passed' in (r/'background-loader.txt').read_text(),'Preset regression'
 assert not re.search(r'VK_ERROR|VUID-|allocation failed|decode failed',loader,re.I),'Loader failure' 
 for name in ('continuity-flat','continuity-turn','mask-on','on','native','day08'):
  log=(r/name/'run.txt').read_text(errors='replace')
  assert not re.search(r'VUID-|VK_ERROR_|Validation Error|device lost',log,re.I),name
  if name!='day08':assert 'five continuous master materials uploaded' in log,name
 print(json.dumps({'UV_rotation_max_delta':float(abs(flat-turn).max()),'shared_detail_mean_delta':round(float(abs(flat-old).mean()),3),'original_mask_class_agreement':round(float(stable),4),'login_unchanged':True,'daylight_dark_pixel_mean_lift':round(float(delta[dark].mean()),2),'daylight_interface_unchanged':True,'draw_paths':['live combined','separate base/light','native ground'],'loader_toggle_and_quality_aliases':'passed','Ultra_preset_and_shutdown':'passed'},indent=2))
if __name__=='__main__':main()
