"""Validate native live ground, fallback, and mineral material GPU fixtures."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);a=p.parse_args()
    def im(name):return np.asarray(Image.open(a.directory/name/'result.bmp').convert('RGB')).astype(int)
    for name in ['separate-off','separate-on','combined-off','combined-on','unknown-off','unknown-on']:
        pixels=im(name)
        assert pixels[180:].std()>20, f'{name}: blank/flat ground'
        # Unsupported fixture deliberately selects unlit, flat-colour tiles.
        assert len(np.unique(pixels[200:].reshape(-1,3),axis=0))>=(4 if name.startswith('unknown') else 50),name
        log=(a.directory/name/'run.txt').read_text(errors='replace')
        assert not any(x in log for x in ['VUID-','VK_ERROR_','Validation Error']),name
    assert np.array_equal(im('unknown-off'),im('unknown-on')), 'Unsupported pass lost or changed original ground'
    rough,metal=im('mineral-rough'),im('mineral-metal')
    # The bottom row deliberately supplies glossy metal ORM maps to known
    # rocks/white stone, with real metal/glass controls in the last two columns.
    samples=[]
    for x in [222,304,385]:
        r=float(np.median(rough[360:372,x-4:x+5]));m=float(np.median(metal[360:372,x-4:x+5]))
        assert r>210 and m<5,(x,r,m)
        samples.append({'x':x,'roughness_debug':r,'metallic_debug':m})
    assert np.median(metal[360:372,460:473])>150, 'Real metal control became nonmetallic'
    assert np.median(rough[360:372,460:473])<100, 'Real metal control lost polish'
    assert np.abs(im('terrain-middle')-im('haze-layered'))[150:400].mean()>.2, 'Distant haze did not render'
    print(json.dumps({'native_combined_ground_visible':True,'separate_pass_ground_visible':True,
                      'unsupported_pass_retains_original_pixels':True,'known_minerals_override_glossy_metal_maps':samples,
                      'real_metal_control_preserved':True,'haze_rendered':True},indent=2))

if __name__=='__main__':main()
