"""Repeatable real Vulkan fixtures. Never starts AO or accesses a game installation."""
from pathlib import Path
import os, subprocess, json, time, ctypes, re, statistics, hashlib, argparse
from PIL import Image, ImageDraw, ImageFont
import numpy as np
R=Path(__file__).resolve().parents[1]; E=R/'evidence';E.mkdir(exist_ok=True)
exe=R/'build/x86/living_fixture.exe';scene=R/'experiment/assets/fixture.lrs'
env=os.environ.copy();env['RANDYVK_LIVING_TIME']='2';env['RANDYVK_PIPELINE_CACHE']=str(E/'fixture-pipeline.cache')
class Memory(ctypes.Structure):
    _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ['peakWS','workingSet','quotaPeakPaged','quotaPaged','quotaPeakNonpaged','quotaNonpaged','pagefile','peakPagefile']]
def run(name,args,program=exe):
    log=E/(name+'.log');peak=0;trace=[];start=time.monotonic()
    with log.open('w') as f:
        p=subprocess.Popen([str(program),*map(str,args)],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT)
        while p.poll() is None:
            m=Memory();m.cb=ctypes.sizeof(m)
            if ctypes.windll.psapi.GetProcessMemoryInfo(ctypes.c_void_p(int(p._handle)),ctypes.byref(m),m.cb):peak=max(peak,m.workingSet);trace.append([round(time.monotonic()-start,2),m.workingSet])
            if time.monotonic()-start>240:p.kill();raise RuntimeError('timeout '+name)
            time.sleep(.1)
    text=log.read_text(errors='replace')
    if p.returncode:raise RuntimeError(f'{name} exit {p.returncode}: '+text[-1200:])
    gpu=re.findall(r'gpu ms:.*?\| total ([0-9.]+)',text)
    cpu=re.findall(r'living submit ([0-9.]+)',text)
    smoothed=re.findall(r'GPU smoothed=([0-9.]+)',text)
    out={'name':name,'exit':p.returncode,'peak_working_set_bytes':peak,'elapsed_seconds':round(time.monotonic()-start,3),'gpu_frame_ms':float(gpu[-1]) if gpu else None,'living_cpu_submit_ms':float(cpu[-1]) if cpu else 0,'gpu_smoothed_ms':float(smoothed[-1]) if smoothed else None,'log':log.name}
    if 'zone-stress' in name:out['memory_trace']=trace
    print(json.dumps({k:v for k,v in out.items() if k!='memory_trace'}),flush=True);return out
def fixture(name,*args,frames=120):
    return run(name,['--scene',scene,'--frames',frames,'--shot',E/(name+'.bmp'),*args])
def compare(a,b):
    x=np.array(Image.open(E/a).convert('RGB'),dtype=np.int16);y=np.array(Image.open(E/b).convert('RGB'),dtype=np.int16);d=np.abs(x-y)
    return {'a':a,'b':b,'identical':bool(np.array_equal(x,y)),'different_pixels':int(np.any(d,axis=2).sum()),'maximum_channel_error':int(d.max()),'mean_channel_error':float(d.mean())}
def label(img,title):
    img=img.copy().convert('RGB');d=ImageDraw.Draw(img);d.rectangle((0,0,img.width,44),fill=(13,21,25));d.text((16,12),title,fill=(233,231,216),font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18));return img
def finish_images(names):
    tiles=[]
    for name,title in names:
        img=Image.open(E/(name+'.bmp')).convert('RGB');img.save(E/(name+'.png'));tiles.append(label(img.resize((960,540)),title+' | renderer fixture'))
    sheet=Image.new('RGB',(1920,540*((len(tiles)+1)//2)),(13,21,25))
    for i,t in enumerate(tiles):sheet.paste(t,((i%2)*960,(i//2)*540))
    sheet.save(E/'comparison-sheet.png')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--stage',choices=['images','bench','regressions','film'],required=True);a=parser.parse_args();result=[]
    if a.stage=='images':
        for name,args in [
            ('all-off',['--level',0]),('people-unavailable',['--level',1,'--group',0]),
            ('drone-close',['--level',1,'--group',1,'--camera','drone']),('drone-patrol',['--level',2,'--group',1]),
            ('air-traffic',['--level',1,'--group',2,'--camera','traffic']),('wildlife-unavailable',['--level',1,'--group',3]),
            ('machinery',['--level',1,'--group',4,'--camera','machine']),('vent-steam',['--level',1,'--group',5,'--camera','machine']),
            ('available-combined',['--level',2]),('night-combined',['--level',2,'--night']),
            ('toggle-off',['--level',2,'--toggle']),('wrong-playfield',['--level',2,'--zone',566]),
            ('hdr-off',['--level',2,'--hdr-off']),('hdr-off-disabled',['--level',0,'--hdr-off']),
            ('night-off',['--level',0,'--night'])]:result.append(fixture(name,*args))
        comparisons=[compare('all-off.bmp',n+'.bmp') for n in ['people-unavailable','wildlife-unavailable','wrong-playfield']]
        comparisons.append(compare('baseline-off.bmp','all-off.bmp'))
        for receipt in comparisons:assert receipt['identical'],receipt
        # TAA can retain a small history after disabling: measure, rather than assume, exact final-frame parity.
        comparisons.append(compare('all-off.bmp','toggle-off.bmp'))
        hdr=compare('hdr-off-disabled.bmp','hdr-off.bmp');assert not hdr['identical'],'non-HDR actors failed to render';comparisons.append(hdr)
        (E/'parity.json').write_text(json.dumps(comparisons,indent=2))
        finish_images([('all-off','All ambient systems OFF'),('available-combined','Available systems combined'),('drone-close','Original service drone / articulated scanner'),('machinery','Independent rotating extractor'),('air-traffic','Distant cargo traffic'),('night-combined','Available systems at night')])
    elif a.stage=='bench':
        for repeat in range(3):
            for name,args in [('off',['--level',0]),('drones',['--level',1,'--group',1]),('traffic',['--level',1,'--group',2]),('machinery',['--level',1,'--group',4]),('atmosphere',['--level',1,'--group',5]),('low',['--level',1]),('medium',['--level',2]),('high',['--level',3])]:
                result.append(fixture(f'bench-{name}-{repeat}',*args,frames=720))
        summary={}
        for name in ('off','drones','traffic','machinery','atmosphere','low','medium','high'):
            samples=[x for x in result if x['name'].startswith('bench-'+name+'-')]
            summary[name]={'gpu_frame_ms_median':statistics.median(x['gpu_frame_ms'] for x in samples),'gpu_frame_ms_range':[min(x['gpu_frame_ms'] for x in samples),max(x['gpu_frame_ms'] for x in samples)],'living_cpu_ms_median':statistics.median(x['living_cpu_submit_ms'] for x in samples),'peak_working_set_bytes':max(x['peak_working_set_bytes'] for x in samples)}
        for x in summary.values():x['gpu_delta_from_off_ms']=round(x['gpu_frame_ms_median']-summary['off']['gpu_frame_ms_median'],3)
        (E/'benchmark-summary.json').write_text(json.dumps({'scope':'1920x1080 controlled renderer fixture; no citizens or wildlife; GPU timestamps after 120-frame warmup; 3 repeats; not live gameplay or target-hardware guarantee','results':summary,'raw':result},indent=2))
    elif a.stage=='film':
        for camera in ('drone','machine'):
            folder=E/('film-'+camera);folder.mkdir(exist_ok=True)
            result.append(fixture('animation-'+camera,'--level',1,'--group',1 if camera=='drone' else 4,'--camera',camera,'--film',folder,frames=180))
            frames=[]
            for p in sorted(folder.glob('frame-*.bmp')):
                im=Image.open(p).convert('RGB').resize((960,540));frames.append(label(im,'Living Rubi-Ka — animated renderer fixture / '+camera))
            frames[0].save(E/('animation-'+camera+'.gif'),save_all=True,append_images=frames[1:],duration=67,loop=0,optimize=False)
            # Raw frames retained for independent continuity inspection.
            (E/('animation-'+camera+'-evidence.json')).write_text(json.dumps({'type':'real Vulkan renderer fixture sequence','frames':len(frames),'simulation_seconds':3,'capture_interval_seconds':4/60,'all_frame_hashes':[hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.glob('frame-*.bmp'))]},indent=2))
    else:
        for name in ('weather_check','object_cache_check','shadow_cache_check','smoke_ring_check','threaded_worker_check','material_catalog_check','grass_coverage_check','diagnostic_walk_check'):
            result.append(run('regression-'+name,[],R/'build/x86'/(name+'.exe')))
        result.append(run('living-check',[scene],R/'build/x86/living_check.exe'))
        result.append(fixture('zone-stress','--level',2,'--cycles',12,frames=960))
        env['RANDYVK_GPU_SKIN']='0'
        result.append(fixture('cpu-skin-fallback','--level',1,'--group',1,'--camera','drone',frames=120))
    (E/('validation-'+a.stage+'.json')).write_text(json.dumps(result,indent=2))
