"""Validate the shared GroundOn draw path, material response and mode switches."""
import argparse,json,re
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path);root=parser.parse_args().directory
    metrics={}
    names=['groundon','shared','shared-groundon','plus','plus-manual','plus-reference','plus-no-edge-blend','plus-original-textures','plus-no-pebbles','cliff-on','cliff-off','cycle']
    for name in names:
        log=(root/name/'run.txt').read_text(errors='replace')
        assert 'rendered; screenshot result.bmp' in log,name
        assert not re.search(r'VUID-|VK_ERROR|Validation Error|device lost',log,re.I),name
        dump=(root/name/'native-frame.txt').read_text(errors='replace')
        m=re.search(r'native terrain: shared-plus (\d+); enabled (\d+); patches (\d+); patches/frame ([\d.]+); rocks (\d+); pebbles (\d+); AO ([\d.]+); sun ([\d.]+)',dump)
        assert m,name
        metrics[name]=dict(zip(['plus','enabled','patches','patches_per_frame','rocks','pebbles','ao','sun'],map(float,m.groups())))
    def pixels(name):return np.asarray(Image.open(root/name/'result.bmp').convert('RGB')).astype(float)
    for name in ['groundon','shared-groundon','plus','plus-manual','plus-reference','cycle']:
        assert metrics[name]['patches']>0 and metrics[name]['patches_per_frame']>0,name
        assert metrics[name]['rocks']>0 and metrics[name]['pebbles']>0,(name,metrics[name])
    assert metrics['shared']['enabled']==0 and metrics['shared']['patches_per_frame']==0
    deltas={}
    for a,b in [('plus','plus-manual'),('plus','plus-reference'),('plus','cycle')]:
        d=float(abs(pixels(a)-pixels(b)).max());deltas[a+'/'+b]=d
        assert d<=1,(a,b,'Shared+ changed lighting or duplicated native details',d)
        assert metrics[a]['rocks']==metrics[b]['rocks'] and metrics[a]['pebbles']==metrics[b]['pebbles'],(a,b)
        assert metrics[a]['patches_per_frame']==metrics[b]['patches_per_frame'],(a,b,'Duplicate terrain pass')
    d=float(abs(pixels('plus')-pixels('plus-no-edge-blend')).max())
    assert d==0,('Plus retained extra tile-edge blending',d)
    deltas['plus/no-edge-blend']=d
    d=float(abs(pixels('plus')-pixels('plus-original-textures')).mean())
    assert d>.1,('Plus lost the enhanced floor textures',d)
    deltas['plus/original-textures']=d
    d=float(abs(pixels('edge-plus')-pixels('edge-no-blend')).max())
    assert d==0,('Plus tile edges differ from the no-blending control',d)
    deltas['edge-plus/no-blend']=d
    diff=abs(pixels('edge-with-blend')-pixels('edge-no-blend'))
    changed=int(np.count_nonzero(diff.max(axis=2)>1))
    assert changed>20 and diff.max()>5,('Tile-edge blend positive control inactive',changed)
    deltas['edge-blending changed_pixels']=changed
    for a,b in [('plus','plus-no-pebbles'),('cliff-on','cliff-off'),('shared','shared-groundon')]:
        diff=abs(pixels(a)-pixels(b))
        d=float(diff.mean());deltas[a+'/'+b]=d
        # Pebbles are deliberately tiny: count visible affected pixels instead
        # of requiring a whole-frame brightness change from their toggle.
        if b=='plus-no-pebbles':
            changed=int(np.count_nonzero(diff.max(axis=2)>1))
            assert changed>100 and diff.max()>5,(a,b,'Pebbles absent',changed)
            deltas[a+'/'+b+' changed_pixels']=changed
        else:
            assert d>.01,(a,b,'Detail or cliff materials absent',d)
    for name in ['plus','plus-manual','cycle']:
        assert metrics[name]['ao']==metrics[name]['sun']==0,name
    assert (root/'cycle/run.txt').read_text().count('128 x 128 cells of')==1,'Mode switching rebuilt the ground'
    for name,phrase in [('settings-final','terrain settings Ultra, immediate switching'),('restart-plus','restart 2 passed'),('restart-shared','restart 1 passed')]:
        assert phrase in (root/(name+'.txt')).read_text(errors='replace'),name
    for name in ['continuity-flat','road-rough','deck-palette']:
        actual=np.asarray(Image.open(root/('regression-'+name)/'result.bmp'))
        previous=np.asarray(Image.open(root.parent/'preview12-check'/name/'result.bmp'))
        d=float(abs(actual.astype(float)-previous).max())
        assert d==0,(name,'Ordinary Shared Ground changed',d)
        deltas['preview12/'+name]=d
    separate_a=np.asarray(Image.open(root/'separate-plus/result.bmp')).astype(float)
    separate_b=np.asarray(Image.open(root/'separate-reference/result.bmp')).astype(float)
    d=float(abs(separate_a-separate_b).max())
    assert d<=1,('Separate ground passes changed lighting or details',d)
    deltas['separate-plus/reference']=d
    print(json.dumps({'result':'passed','render_metrics':metrics,'pixel_deltas':deltas,
                     'live_zoning_and_model_fit':'requires user visual confirmation'},indent=2))

if __name__=='__main__':main()
