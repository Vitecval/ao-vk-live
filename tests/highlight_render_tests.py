"""GPU glare comparisons across sun directions, with UI/sky/light controls.

Accept screenshots from rvk_demo at 768x512: --mineral-materials-test,
then the same with --sun-dir 0.5 0.6 0.6; and --pbr-test --hdr
--pbr-selflit --sky-zone 655 --bloom 0.5. Each pair uses
--soft-highlights 0 / 0.75. Names: glare, night-glare, lights-glare,
each suffixed -off/-on/result.bmp.
"""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);args=p.parse_args();report={}
    for name in ['glare','night-glare','lights-glare']:
        def read(state):return np.asarray(Image.open(args.directory/(name+'-'+state)/'result.bmp').convert('RGB')).astype(int)
        a,b=read('off'),read('on');delta=a-b;peak=a.max(2);bright=peak>220;dark=peak<100
        assert bright.sum()>100,'Fixture lacks bright highlights'
        assert delta[bright].mean()>8 and delta.max()>=25,(name,'No effective glare reduction')
        assert abs(delta[dark]).max()<=1,(name,'Shadows changed')
        assert abs(delta[50:70,20:150]).max()==0,(name,'Sky changed')
        if name!='lights-glare':
            assert abs(delta[20:40,20:150]).max()==0,(name,'Interface changed')
            assert abs(delta[90:112,615:635]).max()==0,(name,'Additive lamp changed')
        log=(args.directory/(name+'-on')/'run.txt').read_text(errors='replace')
        assert not any(x in log for x in ['VUID-','VK_ERROR_','Validation Error']),name
        report[name]={'mean_bright_pixel_reduction':round(float(delta[bright].mean()),2),'max_reduction':int(delta.max()),'dark_pixel_max_delta':int(abs(delta[dark]).max())}
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
