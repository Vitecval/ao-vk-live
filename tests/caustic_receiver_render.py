"""Native caustic receiver precision, occlusion, animation and role controls.

Requires local native exports 226088-native.png and 226099-native.png; no game
pixels are bundled. The precision fixture is not a captured-geometry replay.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--native-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--validation-layers',type=Path,required=True)
    args=parser.parse_args()
    exe,native,out=(path.resolve() for path in (args.exe,args.native_dir,args.output))
    out.mkdir(parents=True,exist_ok=True)
    (out/'RESULTS.json').unlink(missing_ok=True)
    fingerprint=hashlib.sha256(exe.read_bytes()).hexdigest()
    frames={}
    def render(name,mode,offset=.0003,identity=226099,validate=False,
               intensity=1,water=True,hdr=True,next_intensity=-1):
        directory=out/name;directory.mkdir(exist_ok=True)
        command=[str(exe),'--caustic-receiver-test',str(mode),
                 '--caustic-receiver-offset',str(offset),
                 '--caustic-receiver-source',str(native/f'{identity}-native.png'),
                 '--caustic-receiver-intensity',str(intensity),
                 '--caustic-receiver-intensity-next',str(next_intensity),
                 '--caustic-receiver-water',str(int(water)),
                 '--night-glow','0','--fixed-clock','16.6666667',
                 '--size','640','480','--frames','120','--shot',str(directory/'frame.bmp')]
        # rvk_demo's --bloom option enables HDR even when its strength is zero.
        # Keep it out of the native/HDR-off controls entirely; otherwise this
        # pair accidentally compares the enhanced named receiver with HDR-on
        # ordinary geometry and cannot establish the native fallback contract.
        if hdr:command.extend(['--hdr','--bloom','0'])
        environment=os.environ.copy()
        # Each case shares only this runner's private writable cache. Root may
        # seed it from a completed compatible r68 cache before this run.
        environment['RANDYVK_PIPELINE_CACHE']=str(out/'pipelines.cache')
        if validate:
            environment.update(VK_LAYER_PATH=str(args.validation_layers.resolve()),
                               VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
                               VK_LOADER_DEBUG='layer',VK_LAYER_VALIDATE_SYNC='1')
        with (directory/'run.log').open('w') as log:
            subprocess.run(command,cwd=directory,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True)
        frames[name]=np.array(Image.open(directory/'frame.bmp').convert('RGB'),dtype=np.float64)
        print(name,'rendered',flush=True)
    for name,mode,offset,identity,validate in [
        ('baseline',3,.0003,226099,False),
        ('receiver',1,.0003,226099,False),
        ('old-precision',2,.0003,226099,False),
        ('receiver-companions',4,.0003,226099,True),
        ('old-full-light',2,0,226099,False),
        ('old-companions',5,0,226099,False),
        ('receiver-phase',1,.0003,226088,False),
    ]:render(name,mode,offset,identity,validate)
    render('caustics-off',1,intensity=0)
    render('caustics-half',1,intensity=.5)
    render('caustics-double',1,intensity=2)
    render('live-off',1,next_intensity=0)
    render('live-double',1,next_intensity=2)
    render('water-master-off',1,water=False)
    render('water-master-off-zero',1,water=False,intensity=0)
    render('hdr-off-native',1,offset=0,hdr=False,water=False)
    render('hdr-off-unlisted',2,offset=0,hdr=False,water=False)
    # Flat grey receiver is the brightest common baseline material. The blue
    # foreground blocker is the brightest common blue material; dark clear
    # background is excluded. Erode both masks to avoid legitimate silhouette motion.
    colours,counts=np.unique(frames['baseline'].reshape(-1,3),axis=0,return_counts=True)
    common=counts>1000
    ground_colour=colours[np.where(common)[0][np.argmax(colours[common].mean(axis=1))]]
    blue=common & (colours[:,2]>colours[:,1]*1.15) & (colours[:,1]>colours[:,0]*1.15)
    blocker_colour=colours[np.where(blue)[0][np.argmax(colours[blue].mean(axis=1))]]
    def mask(colour):
        # Tone-map dither varies a flat receiver by one or two channel values.
        # Eroding an exact single RGB value would erase its checkerboard cells.
        selected=np.max(np.abs(frames['baseline']-colour),axis=2)<=2
        for axis in (0,1):
            selected=selected & np.roll(selected,1,axis) & np.roll(selected,-1,axis)
        return selected
    ground,blocker=mask(ground_colour),mask(blocker_colour)
    def difference(first,second,region=None):
        delta=np.abs(frames[first]-frames[second])
        if region is not None:delta=delta[region]
        return dict(max=float(delta.max()),mean=float(delta.mean()),
                    changedShare=float(np.mean(np.max(delta,axis=-1)>1)))
    log=(out/'receiver-companions/run.log').read_text(errors='replace')
    dump=(out/'receiver-companions/caustic-receiver-frame.txt').read_text(errors='replace')
    hdr_off_dump=(out/'hdr-off-native/caustic-receiver-frame.txt').read_text(errors='replace')
    result=dict(result='passed',executableSha256=fingerprint,
                pipelineCachePath=str(out/'pipelines.cache'),caseCount=len(frames),
                nativeInputSha256={str(identity):hashlib.sha256((native/f'{identity}-native.png').read_bytes()).hexdigest()
                                   for identity in (226088,226099)},
                groundPixels=int(ground.sum()),blockerPixels=int(blocker.sum()),
                correctedCoverage=difference('receiver','baseline',ground),
                oldPrecisionCoverage=difference('old-precision','baseline',ground),
                companions=difference('receiver','receiver-companions'),
                oldFullLight=difference('old-full-light','baseline',ground),
                oldCompanionControl=difference('old-full-light','old-companions',ground),
                foregroundOcclusion=difference('receiver','baseline',blocker),
                nativeFrameAnimation=difference('receiver','receiver-phase',ground),
                causticsOff=difference('caustics-off','baseline'),
                halfIntensity=difference('caustics-half','baseline',ground),
                doubleIntensity=difference('caustics-double','baseline',ground),
                liveOff=difference('live-off','caustics-off'),
                liveDouble=difference('live-double','caustics-double'),
                waterMasterOff=difference('water-master-off','receiver',ground),
                waterMasterOffIgnoresChild=difference('water-master-off','water-master-off-zero'),
                hdrOffNativeFallback=difference('hdr-off-native','hdr-off-unlisted'),
                hdrOffReceiverTreatment='caustic-light gain' in hdr_off_dump,
                validationLayerActive='Insert instance layer "VK_LAYER_KHRONOS_validation"' in log,
                synchronizationValidationRequested=True,
                validationErrors=[line for line in log.splitlines() if any(s in line for s in ('VUID-','Validation Error','SYNC-HAZARD'))],
                receiverDiagnostic='caustic-light gain 0.16 effective-depth LEQUAL/decal' in dump,
                retailPostInstallVerified=False)
    try:
        assert hashlib.sha256(exe.read_bytes()).hexdigest()==fingerprint,'Executable changed during run'
        assert result['groundPixels']>10000 and result['blockerPixels']>1000
        assert result['correctedCoverage']['changedShare']>.6
        assert result['oldPrecisionCoverage']['changedShare']<.4
        assert result['companions']['max']<=1
        assert result['oldCompanionControl']['max']>5
        assert result['foregroundOcclusion']['max']<=1
        assert result['correctedCoverage']['max']<result['oldFullLight']['max']*.5
        assert result['nativeFrameAnimation']['mean']>.3
        assert result['causticsOff']['max']<=1
        assert 0<result['halfIntensity']['mean']<result['correctedCoverage']['mean']*.85
        assert result['doubleIntensity']['mean']>result['correctedCoverage']['mean']*1.1
        assert result['liveOff']['max']<=1 and result['liveDouble']['max']<=1
        assert result['waterMasterOff']['mean']>1
        assert result['waterMasterOffIgnoresChild']['max']<=1
        assert result['hdrOffNativeFallback']['max']<=1
        assert not result['hdrOffReceiverTreatment'],'HDR-off fixture unexpectedly used enhanced caustic receiver treatment'
        assert result['validationLayerActive'] and not result['validationErrors'] and result['receiverDiagnostic']
    except AssertionError:
        result['result']='failed'
        raise
    finally:
        (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))


if __name__=='__main__':main()
