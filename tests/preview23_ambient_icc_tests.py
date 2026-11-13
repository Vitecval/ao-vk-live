"""Headless Vulkan checks for Automatic habitats, ICC HD retention and native cache residency."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess


def bmp(path):
    data=path.read_bytes();offset=struct.unpack_from('<I',data,10)[0]
    width,height=struct.unpack_from('<ii',data,18);bits=struct.unpack_from('<H',data,28)[0]
    assert bits in (24,32),bits
    stride=((width*bits+31)//32)*4;channels=bits//8
    return bytes(value for row in range(abs(height)) for x in range(width)
        for value in data[offset+row*stride+x*channels:offset+row*stride+x*channels+3])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True);parser.add_argument('--maps',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();build=args.build.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines'))
    rows=[]

    def run(name,options,frames=90,pressure=False):
        folder=out/name;folder.mkdir(exist_ok=True)
        active=dict(env)
        if pressure:active.update(RANDYVK_TEST_MODULES='1',RANDYVK_TEST_MATERIAL_CACHE_MB='1')
        command=[str(build/'rvk_demo.exe'),'--shared-terrain-test','--terrain-pebbles','--hdr','--terrain-lighting-fixture',
            '--terrain-blend','2','--size','640','480','--fixed-clock','16.666667',
            '--frames',str(frames),'--shot','result.bmp',*options]
        with (folder/'run.log').open('w') as log:
            subprocess.run(command,cwd=folder,env=active,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
        text=(folder/'run.log').read_text()
        assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',text,re.I),name
        dump=(folder/'terrain-frame.txt').read_text()
        if name!='login':assert 'geometry=native-patches' in dump,name
        rows.append(dict(case=name,placement=re.findall(r'# Ambient (?:life|kinds):[^\n]*',dump)))
        print(name,'passed',flush=True)
        return folder,dump

    for name,zone,fixture in [('rk-city',655,3),('rk-green',655,1),('rk-dry',565,2),
        ('sl-forest',4310,1),('jobe',4314,3),('snow',4320,3),('inferno',4605,2),('ash',4900,3),('cavern',386,5)]:
        folder,dump=run(name,['--ambient-life-fixture',str(fixture),'--area-zone',str(zone),'--ambient-life','1.5'])
        assert '# Ambient life:' in dump,name
        if name=='jobe':assert re.search(r'spirits [1-9]',dump),dump
        if name=='rk-green':assert re.search(r'pollen [1-9]',dump),dump
    _,dump=run('login',['--ambient-life-fixture','4','--ambient-life','1.5'])
    assert '# Ambient life:' not in dump
    _,dump=run('off',['--ambient-life-fixture','3','--ambient-life','0'])
    assert '# Ambient life:' not in dump
    maps=['--terrain-authored','--terrain-personal','--terrain-id','21','--shared-ground-maps',str(args.maps.resolve())]
    # HD Ground keeps upgraded pixels when ICC refinement is disabled.
    a,_=run('icc-off-hd-on',[*maps,'--improved-tiles','0','--hd-ground','1'],frames=30)
    b,_=run('icc-off-hd-off',[*maps,'--improved-tiles','0','--hd-ground','0'],frames=30)
    c,_=run('icc-refined',[*maps,'--improved-tiles','1','--hd-ground','1'],frames=30)
    aa,bb,cc=map(lambda f:bmp(f/'result.bmp'),(a,b,c))
    assert sum(x!=y for x,y in zip(aa,bb))>1000,'HD-on still uses native artwork'
    assert sum(x!=y for x,y in zip(aa,cc))>1000,'ICC refinement absent'
    d,_=run('native-residency',[*maps,'--native-residency-check','--hd-ground','1'],frames=240,pressure=True)
    assert 'native residency: active state 2' in (d/'run.log').read_text()
    settings=out/'settings';settings.mkdir(exist_ok=True)
    for name in ['ao-vk.dll','preview23_settings_check.exe']:shutil.copy2(build/name,settings/name)
    with (settings/'run.log').open('w') as log:
        subprocess.run([str(settings/'preview23_settings_check.exe')],cwd=settings,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
    result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),scope='Controlled headless Vulkan fixtures; retail appearance and camera movement require a playtest',cases=rows)
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Settings and all render checks passed',flush=True)


if __name__=='__main__':main()
