"""Exercise sky-only clouds and traffic, ambient visibility, and revision 36 defaults."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess


def pixels(path):
    data=path.read_bytes();offset=struct.unpack_from('<I',data,10)[0]
    width,height=struct.unpack_from('<ii',data,18);bits=struct.unpack_from('<H',data,28)[0]
    stride=((width*bits+31)//32)*4;channels=bits//8
    rows=[]
    for y in range(abs(height)):
        at=offset+(abs(height)-1-y if height>0 else y)*stride
        rows.append(bytes(c for x in range(width) for c in data[at+x*channels:at+x*channels+3]))
    return rows


def different(a,b):
    return sum(any(x!=y for x,y in zip(aa[i:i+3],bb[i:i+3])) for aa,bb in zip(a,b) for i in range(0,len(aa),3))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();build=args.build.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines'));rows=[]

    def run(name,options,sky=True):
        folder=out/name;folder.mkdir(exist_ok=True)
        common=['--sky-enhance-fixture','--soft-sky','--water-lake','--water-off','--sky-dome','8FAACA','--water-pitch','-.18','--sky-zone','655','--stars','0','--meteors','0','--aurora','0','--profile-window'] if sky else ['--shared-terrain-test','--terrain-pebbles','--hdr','--terrain-lighting-fixture','--ambient-life-fixture','3']
        cmd=[str(build/'rvk_demo.exe'),*common,'--size','768','512','--fixed-clock','250','--frames','40','--dump','frame.txt','--shot','result.bmp',*options]
        with (folder/'run.log').open('w') as f:subprocess.run(cmd,cwd=folder,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
        log=(folder/'run.log').read_text();assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
        dump=(folder/('sky-frame.txt' if sky else 'terrain-frame.txt')).read_text()
        rows.append(dict(case=name,diagnostics=[s for s in dump.splitlines() if s.startswith(('# Sky enhancements:','# Ambient life:'))]))
        print(name,'passed',flush=True)
        return pixels(folder/'result.bmp'),dump

    for time,sun in [('day','-.7'),('night','.7')]:
        base=['--sun-dir','.6',sun,'.3']
        control,_=run(time+'-off',base)
        clouds,dump=run(time+'-clouds',[*base,'--sky-clouds','1'])
        assert 'clouds 1; traffic 0' in dump
        traffic,dump=run(time+'-traffic',[*base,'--sky-traffic','1'])
        assert 'clouds 0; traffic 1' in dump
        both,_=run(time+'-both',[*base,'--sky-clouds','1','--sky-traffic','1'])
        counts=[different(control[:256],a[:256]) for a in [clouds,traffic,both]]
        assert counts[0]>1000 and counts[1]>30,(time,counts)
        # Bottom rows are opaque ground/flat legacy water; sky changes must not darken them.
        ground=[different(control[400:],a[400:]) for a in [clouds,traffic,both]]
        assert ground==[0,0,0],('Sky enhancement changed foreground',time,ground)
        rows.append(dict(time=time,sky_pixels_changed=counts,foreground_pixels_changed=ground))
    _,dump=run('legacy-clouds',['--sun-dir','.6','-.7','.3','--sky-clouds','1','--clouds','--cloud-qual','0'])
    assert 'clouds 0;' in dump,'Legacy cloud renderer must take priority'
    _,dump=run('login-sky',['--sky-zone','0','--sky-clouds','1','--sky-traffic','2'])
    assert 'clouds 1;' not in dump and 'traffic 2;' not in dump,'Login must exclude sky enhancements'
    later,_=run('traffic-later',['--sun-dir','.6','-.7','.3','--sky-traffic','1','--frames','80'])
    earlier=pixels(out/'day-traffic'/'result.bmp')
    assert different(earlier[:256],later[:256])>30,'Traffic must move over time'
    run('night-stars',['--sun-dir','.6','.7','.3','--sky-clouds','1','--sky-traffic','1','--stars','1','--star-bright','1.45'])
    off,_=run('ambient-off',['--ambient-life','0'],False)
    for name,style in [('dust',2),('snow',5),('ash',7),('fireflies',1)]:
        img,dump=run(name,['--ambient-style',str(style),'--ambient-life','2','--ambient-brightness','1.5'],False)
        count=different(off,img);assert count>10,(name,count)
        rows.append(dict(style=name,particle_pixels_changed=count))
    settings=out/'settings';settings.mkdir(exist_ok=True)
    for name in ['ao-vk.dll','preview23_settings_check.exe']:shutil.copy2(build/name,settings/name)
    with (settings/'run.log').open('w') as f:
        subprocess.run([str(settings/'preview23_settings_check.exe')],cwd=settings,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=60)
    result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),scope='Headless Vulkan fixtures; retail appearance and performance require a playtest',cases=rows)
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print('Sky and ambient checks passed',flush=True)


if __name__=='__main__':main()
