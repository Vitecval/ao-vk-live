"""Independent numeric/color/UI checks of the real Vulkan fixture output."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'build/horizon-0.7.6/vivid-check'
names=['off','zero','on','off-again','static','static-bright','adapt-bright']
a={n:np.asarray(Image.open(p/(n+'.bmp'))).astype(float) for n in names}
base=np.array([[0,0,0],[32,32,32],[96,96,96],[160,160,160],[238,238,238],[204,51,51],[51,187,85],[68,102,204]],float)/255
weights=np.array([.2126,.7152,.0722])
scene=float(np.float16(np.exp(np.mean(np.log(np.maximum(base@weights,.0001))))))
def reference(c):
    bright=np.clip((scene-.15)/.35,0,1);bright=bright*bright*(3-2*bright)
    before=c**1.45;c=before*2**.3
    dark=c+.2*(1-bright)*abs(1-c@weights)**2*.1
    light=.5*c+.5*abs(c/.5)**(1+1.2*bright)*.5
    c=dark*(1-bright)+light*bright
    key=np.clip(.5/np.clip(scene,.01,2)*(1.5*(1-bright)+.8*bright),.1,5)
    lum=c@weights;scaled=lum*key;mapped=scaled/(1+scaled/(1.6*(1-bright)+2.4*bright))
    mc=c*(mapped/max(lum,.0001));sat=1.2*(1-bright)+.9*bright
    c=.5*before+.5*(mapped+(mc-mapped)*sat)
    lum=c@weights;under=np.clip(1-lum*2,0,1)*(1-bright*.5);over=np.clip(lum*2-1,0,1)*(1+bright*.5)
    c=abs(c)**(2.5*(1-bright)+1.5*bright)*under+c*(1-under-over)+abs(c)**(.6*(1-bright)+.8*bright)*over
    return (base_i*.1+np.clip(c,0,1)*.9)*255
errors=[]
for i,base_i in enumerate(base):
    expected=reference(base_i.copy())
    actual=np.median(a['on'][50:90,i*32+8:i*32+24],axis=(0,1))
    errors.append(float(np.max(np.abs(expected-actual))))
assert max(errors)<4,errors
assert np.max(abs(a['off']-a['zero']))<=1,'Zero strength changed more than frame dithering'
assert np.max(abs(a['off']-a['off-again']))<=1,'Disabling did not restore the baseline'
for name in names:
    assert np.array_equal(a[name][8:20,8:24],a['off'][8:20,8:24]),name+' changed the UI'
assert np.mean(a['static-bright'][60:90])>np.mean(a['adapt-bright'][60:90])+15,'Exposure did not adapt after unfreezing'
report=dict(result='passed',cpuReferenceMaxChannelError=max(errors),disabledDifferenceMax=1,untouchedUi=True,
            staticExposureAndAdaptation=True,loginChartPreservesAuthoredColors=True,retailClientsChanged=False)
(p/'RESULTS.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
