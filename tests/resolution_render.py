"""Offscreen native framebuffer/scene resize and pixel-accurate UI regression.

No desktop mode is changed. These tests prove actual GPU image/readback sizes,
not physical 1440p/4K monitor presentation or independent interface scaling.
Frame generation cannot interpolate without a window/swapchain: that requested
headless case explicitly records inactive status rather than claiming success.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--validation-layers',type=Path,required=True)
    args=parser.parse_args()
    exe,out=args.exe.resolve(),args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    (out/'RESULTS.json').unlink(missing_ok=True)
    digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    executable_hash=digest(exe)
    results=[]
    # name, output dimensions, internal scale, HDR, TAA, upscaler, FG request, validation
    cases=[
        ('1080p',1920,1080,100,True,True,0,False,False),
        ('1440p',2560,1440,100,True,True,0,False,False),
        ('4k',3840,2160,100,True,True,0,False,False),
        ('ultrawide',3440,1440,100,True,True,0,False,False),
        ('4k-75-taa',3840,2160,75,True,True,0,False,True),
        ('ultrawide-50-taa',3440,1440,50,True,True,0,False,False),
        ('1440p-67-fsr2',2560,1440,67,True,True,1,False,True),
        ('1440p-67-fsr3',2560,1440,67,True,True,2,False,True),
        ('original-1080p',1920,1080,50,False,False,0,False,False),
        ('taa-off-1440p',2560,1440,50,True,False,0,False,False),
        ('4k-fg-requested-headless',3840,2160,75,True,True,0,True,False),
    ]
    for name,w,h,scale,hdr,taa,upscaler,fg,validate in cases:
        directory=out/name
        directory.mkdir(exist_ok=True)
        command=[str(exe),'--resolution-test','--size',str(w),str(h),'--render-scale',str(scale),
                 '--upscaler',str(upscaler),'--frames','16','--fixed-clock','16.6666667',
                 '--shot',str(directory/'frame.bmp'),'--sharpen','0']
        if hdr:command.append('--hdr')
        if taa:command.append('--taa')
        if fg:command.append('--frame-gen')
        environment=os.environ.copy()
        environment['RANDYVK_PIPELINE_CACHE']=str(out/'pipelines.cache')
        if validate:
            environment.update(VK_LAYER_PATH=str(args.validation_layers.resolve()),
                VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LOADER_DEBUG='layer',VK_LAYER_VALIDATE_SYNC='1')
        with (directory/'run.log').open('w',encoding='utf-8') as log:
            subprocess.run(command,cwd=directory,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True)
        log=(directory/'run.log').read_text(errors='replace')
        assert not re.search(r'Validation Error|VUID-|SYNC-HAZARD|device lost',log,re.I),(name,'GPU/validation error')
        if validate:
            assert 'VK_LAYER_KHRONOS_validation' in log,(name,'validation layer did not load')
        report=json.loads((directory/'resolution-targets.json').read_text())
        actual_scale=scale if hdr and taa else 100
        sw,sh=(w*actual_scale+50)//100,(h*actual_scale+50)//100
        assert report['output']==[w,h] and report['scene']==[sw,sh],(name,report)
        assert report['actualScale']==actual_scale and report['resizeFailureRetained'] and report['inFrameResizeRejected']
        image=Image.open(directory/'frame.bmp').convert('RGB')
        assert image.size==(w,h),(name,'readback is not native output dimensions',image.size)
        pixels=np.asarray(image)
        markers=[]
        # Every full-colour UI pixel is composed AFTER HDR/TAA/FSR. Compare
        # bounding boxes and exact area, not an average that can hide scaling.
        for colour,box,area in [
            ((32,255,64),(16,16,32,32),256),
            ((255,255,32),(w//2-6,h//2-6,w//2+7,h//2+7),69),
            ((32,192,255),(w-48,h-28,w-16,h-16),384),
        ]:
            ys,xs=np.where(np.all(pixels==colour,axis=2))
            assert len(xs)==area,(name,colour,'UI area changed',len(xs),area)
            actual=(int(xs.min()),int(ys.min()),int(xs.max())+1,int(ys.max())+1)
            assert all(abs(a-b)<=1 for a,b in zip(actual,box)),(name,colour,actual,box)
            markers.append({'rgb':colour,'bounds':actual,'area':int(len(xs))})
        # The central world square uses a perspective aspect derived from the
        # current output, independently from scene scale. Wrong aspect stretch
        # changes its GPU silhouette's width/height.
        red=(pixels[:,:,0]>pixels[:,:,1].astype(float)*1.35)&(pixels[:,:,0]>pixels[:,:,2].astype(float)*1.35)
        ys,xs=np.where(red)
        assert len(xs)>10000,(name,'world square absent')
        rw,rh=int(xs.max()-xs.min()+1),int(ys.max()-ys.min()+1)
        assert abs(rw-rh)<=4,(name,'world aspect stretched',rw,rh)
        scene_path=directory/'resolution-frame-scene.bmp'
        if hdr:
            assert scene_path.is_file(),(name,'missing actual GPU scene readback')
            with Image.open(scene_path) as scene:
                assert scene.size==(sw,sh),(name,'scene readback dimensions',scene.size,[sw,sh])
        # Record actual backend messages. Unsupported FSR is a documented TAA
        # fallback; requesting an SDK is never counted as proof that it ran.
        sdk_messages=[line for line in log.splitlines() if re.search(r'FSR [23]|upscal|frame generation',line,re.I)]
        assert not report['frameGenerationActive'],(name,'headless cannot have an interpolation swapchain')
        results.append({'name':name,'outputReadback':[w,h],'sceneReadback':[sw,sh] if hdr else None,
            'targets':report,'uiMarkers':markers,'worldSquare':[rw,rh],
            'requestedUpscaler':upscaler,'sdkMessages':sdk_messages,
            'requestedFrameGeneration':fg,'validation':validate,'frameSha256':digest(directory/'frame.bmp')})
        print(name,'passed',flush=True)
    assert digest(exe)==executable_hash,'executable changed during test'
    result={'status':'passed','executable':str(exe),'executableSha256':executable_hash,
        'physicalPresentationTested':False,'independentInterfaceScale':False,
        'frameGenerationLimitation':'Headless request/fallback only; physical swapchain interpolation needs separate window test.',
        'cases':results}
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
    print('All native output, scene allocation/readback, UI pixel, aspect and resize checks passed',flush=True)


if __name__=='__main__':
    main()
