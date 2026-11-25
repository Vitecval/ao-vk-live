"""Verify resolved HD photo features, rather than added noise or old AO grain."""
import argparse, hashlib, json, os, re, shutil, subprocess
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter


def photographic_reference(source, feature_scale, width=768, height=512, base_colour=(128,128,128)):
    """Reference the actual HD painting on the existing flat AO fixture.

    Reconstruct its camera rays, mirrored UVs, ordinary box mips and bilinear
    samples. This independently verifies *which photographic structures* remain
    visible; random grain cannot satisfy the correlation assertion.
    """
    image=np.asarray(Image.open(source).convert('RGB'),dtype=float)/255.
    means=np.floor(image.mean(axis=(0,1))*255.+.5)/255.
    mips=[]; current=image.copy()
    while True:
        mips.append(np.floor(current*255.+.5)/255.)
        if current.shape[0]==1:break
        current=current.reshape(current.shape[0]//2,2,current.shape[1]//2,2,3).mean(axis=(1,3))
    yy,xx=np.mgrid[:height,:width]
    nx=(xx+.5)/width*2.-1.; ny=1.-(yy+.5)/height*2.
    f=np.array([0.,-15.,.01]);f/=np.linalg.norm(f)
    right=np.array([1.,0.,0.]);up=np.cross(f,right)
    rays=f+nx[:,:,None]*np.tan(np.pi/6)*(width/height)*right+ny[:,:,None]*np.tan(np.pi/6)*up
    pos=np.array([8.,15.,7.99])+rays*(-15./rays[:,:,1,None])
    world=pos[:,:,[0,2]]*.25*feature_scale
    dx=np.gradient(world,axis=1);dy=np.gradient(world,axis=0)
    def read(st,ddx,ddy):
        footprint=np.maximum(np.linalg.norm(ddx,axis=2),np.linalg.norm(ddy,axis=2))*image.shape[0]
        lod=np.clip(np.log2(np.maximum(footprint,1e-8)),0,len(mips)-1)
        low=np.floor(lod).astype(int); high=np.minimum(low+1,len(mips)-1);part=lod-low
        def at(level):
            result=np.zeros((height,width,3))
            for k,im in enumerate(mips):
                use=level==k
                if not use.any():continue
                uv=st[use]*im.shape[0]-.5;lo=np.floor(uv).astype(int);t=uv-lo
                a=im[lo[:,1]%len(im),lo[:,0]%len(im)]
                b=im[lo[:,1]%len(im),(lo[:,0]+1)%len(im)]
                c=im[(lo[:,1]+1)%len(im),lo[:,0]%len(im)]
                d=im[(lo[:,1]+1)%len(im),(lo[:,0]+1)%len(im)]
                result[use]=a*(1-t[:,0,None])*(1-t[:,1,None])+b*t[:,0,None]*(1-t[:,1,None])+c*(1-t[:,0,None])*t[:,1,None]+d*t[:,0,None]*t[:,1,None]
            return result
        return at(low)*(1-part[:,:,None])+at(high)*part[:,:,None]
    mirrored=1.-np.abs((world*.5%1.)*2.-1.)
    broad=read(mirrored,dx,dy)
    fine_world=world*3.+np.array([.173,.417])
    fine=read(1.-np.abs((fine_world*.5%1.)*2.-1.),dx*3.,dy*3.)
    broad*=1.+.5*(np.clip(fine/np.maximum(means,.04),.65,1.5)-1.)
    broad=np.clip(broad,0,1)
    return np.clip(broad*(np.asarray(base_colour)/255.)/means,0,1)*255.


def band(array):
    image=Image.fromarray(np.clip(array,0,255).astype('uint8'))
    # Resolved structures several pixels wide; one-pixel noise is excluded.
    return np.asarray(image.filter(ImageFilter.GaussianBlur(1.5)),dtype=float).mean(axis=2)-np.asarray(image.filter(ImageFilter.GaussianBlur(6)),dtype=float).mean(axis=2)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--before',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--cache',type=Path)
    p.add_argument('--validate-only',action='store_true');p.add_argument('--reference-only',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
    binaries={} if a.reference_only else {str(d.resolve()):{n:sha(d/n) for n in ['rvk_demo.exe','ao-vk.dll']} for d in [a.build,a.before]}
    cache=out/'pipelines.cache'
    if a.cache and not cache.exists():shutil.copyfile(a.cache,cache)
    report=dict(result='pending',rendererBinaries=binaries,cases={},failures=[],lowResolutionGrainInput=False,retailPlaytest=False)
    for material,scale,colour in [('grass',.5,(47,100,35)),('dirt',.5,(72,46,24)),('gravel',.25,(128,128,128))]:
        source=root/f'rvk/assets/terrain/shared-hd-{material}.png'
        expected=photographic_reference(source,scale,base_colour=colour)
        previous_expected=photographic_reference(source,1.,base_colour=colour)
        Image.fromarray(expected.astype('uint8')).save(out/f'{material}-photo-reference.png')
        result=render_case(a,out,cache,binaries,material,colour,source,expected,previous_expected)
        report['cases'][material]=result
        report['failures'].extend(material+': '+x for x in result.get('failures',[]))
        report['result']='reference-only' if a.reference_only else 'failed' if report['failures'] else 'passed'
        # Keep all measurements even when a later assertion fails.
        (out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2),flush=True)
    assert not report['failures'],report['failures']


def render_case(a,out,cache,binaries,material,colour,source,expected,previous_expected):
    roi=np.s_[100:420,200:568]
    e0,e1=[band(x)[roi] for x in [previous_expected,expected]]
    predicted=dict(beforeResolvedBandRms=float(np.sqrt((e0*e0).mean())),afterResolvedBandRms=float(np.sqrt((e1*e1).mean())))
    if a.reference_only:return dict(predicted=predicted)
    sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
    flat=out/f'constant-{material}-mask.png';Image.new('RGB',(128,128),colour).save(flat)
    images={}
    for name,folder in [('before',a.before),('after',a.build)]:
        folder=folder.resolve();d=out/f'{material}-{name}';d.mkdir(exist_ok=True)
        assert binaries[str(folder)]=={n:sha(folder/n) for n in binaries[str(folder)]}
        if not a.validate_only:
            # --terrain-pebbles forces density1 inside the preserved fixture,
            # overriding --ground-pebbles0. This photo-only plane must omit it.
            flags=['--shared-terrain-test','--terrain-lighting-fixture','--terrain-id','28',
                '--terrain-source',str(flat),'--terrain-blend','2','--hd-ground','1','--sky-zone','566','--area-zone','566',
                '--ground-rocks','0','--ground-pebbles','0','--ground-tess','0','--cliff-depth','0','--pbr-debug','23',
                '--size','768','512','--frames','200','--fixed-clock','.000001','--shot','result.bmp']
            with (d/'run.log').open('w') as log:
                subprocess.run([str(folder/'rvk_demo.exe'),*flags],cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache)),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
        log=(d/'run.log').read_text(errors='replace');dump=(d/'terrain-frame.txt').read_text(errors='replace')
        assert 'rendered; screenshot result.bmp' in log and not re.search(r'VUID-|VK_ERROR|Validation Error|device lost',log,re.I),name
        assert 'hd-masters=6' in dump and 'original-terrain-consumed=0' in dump,name
        assert re.search(r'patches/frame 0\.00; rocks 0; pebbles 0;',dump),'Photo plane contaminated by independent geometry'
        images[name]=np.asarray(Image.open(d/'result.bmp').convert('RGB'),dtype=float)
        assert binaries[str(folder)]=={n:sha(folder/n) for n in binaries[str(folder)]}
        print(material,name,'rendered',flush=True)
    bands={n:band(im)[roi] for n,im in images.items()}
    correlation=float(np.corrcoef(bands['after'].ravel(),e1.ravel())[0,1])
    old_match=float(np.corrcoef(bands['before'].ravel(),e0.ravel())[0,1])
    wrong_match=float(np.corrcoef(bands['after'].ravel(),e0.ravel())[0,1])
    rms={n:float(np.sqrt((im*im).mean())) for n,im in bands.items()}
    # Recovered features should not come with a new broad exposure change.
    means={n:float(im[roi].mean()) for n,im in images.items()}
    checks=[(old_match>.7,'Reference camera/filter model does not describe baseline'),
        (correlation>.7 and correlation>wrong_match+.3,'Features do not follow the actual HD photograph'),
        (rms['after']>rms['before']*1.3,'Resolved photographic structure still lost'),
        (.85<means['after']/means['before']<1.15,'Broad palette/exposure changed')]
    return dict(hdSourceSha256=sha(source),resolvedBandRms=rms,
                actualPhotoStructureCorrelation=correlation,baselineReferenceCorrelation=old_match,
                wrongScaleCorrelation=wrong_match,predicted=predicted,meanColour=means,
                failures=[description for passed,description in checks if not passed])


if __name__=='__main__':main()
