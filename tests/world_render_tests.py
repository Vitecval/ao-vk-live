"""Check GPU world-detail, login and horizon-haze comparisons from rvk_demo."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    args=parser.parse_args()
    def im(name):return np.asarray(Image.open(args.directory/(name+'.bmp')).convert('RGB')).astype(int)
    off,detail,login=im('final-off'),im('final-detail'),im('final-login')
    delta=np.abs(off-detail)
    assert delta[450:].mean()>.1, 'Global detail did not render'
    assert delta[:250].max()==0, 'Detail changed sky'
    assert np.array_equal(off,login), 'Login scene received global detail'
    authored_off,authored_on=im('authored-off'),im('authored-on')
    assert np.array_equal(authored_off,authored_on), 'Authored ground received another grain layer'
    haze_off,haze_on=im('haze-final-off'),im('haze-final-on')
    haze=np.abs(haze_off-haze_on)
    assert haze[:80].max()==0, 'Upper sky changed'
    assert haze[850:950].max()==0, 'Near ground changed'
    assert haze[180:300].mean()>1, 'Horizon sky unchanged'
    assert haze[300:500].mean()>1, 'Far land unchanged'
    assert haze_on[180:280].mean()>60, 'Haze became an unlit black band in daylight'
    result={'global_detail_ground_mean_delta':round(float(delta[450:].mean()),3),
            'detail_sky_unchanged':True,'login_unchanged':True,'authored_ground_unchanged':True,
            'haze_upper_sky_unchanged':True,'haze_foreground_unchanged':True,
            'haze_horizon_and_far_land_changed':True}
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
