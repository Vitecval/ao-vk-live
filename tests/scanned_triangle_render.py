"""Serial controlled captured-grid diagonal, normal, shadow and warmth checks.

The fixture keeps original grid/corner heights but uses a controlled orthographic
camera, optional AO-up normals and explicit illumination, not a retail replay.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,traceback
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build/environment-078-r71/triangle-render-final-check')
p.add_argument('--validate-only',action='store_true')
p.add_argument('--controls-only',action='store_true')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
proof_path=ROOT/'build/environment-078-r71/scanned-scene-build/BUILD-REPORT.json'
proof=json.loads(proof_path.read_text(encoding='utf-8-sig'));assert proof['result']=='passed'
builds={item['version']:item for item in proof['builds']}
exes={label:Path(item['executable']) for label,item in builds.items()}
fingerprints={label:sha(path) for label,path in exes.items()}
binary_folders={'before':ROOT/'build/environment-078-r71/before','after':ROOT/'build/preview23/x86'}
binary_hashes={label:{name:sha(folder/name) for name in ['rvk_demo.exe','ao-vk.dll']} for label,folder in binary_folders.items()}
assert all(fingerprints[label]==builds[label]['executableSha256'] for label in builds)
cache=out/'pipelines.cache'
seed=ROOT/'build/environment-078-r70/shared077-render-check/pipelines.cache'
if not cache.exists() and seed.exists():shutil.copyfile(seed,cache)
result=dict(result='running',executableSha256=fingerprints,binarySha256=binary_hashes['after'],beforeBinarySha256=binary_hashes['before'],fixtureSha256=proof['fixtureSha256'],
            buildProofSha256=sha(proof_path),runs={},diagonals={},shadows={},warmth={},
            controlledOriginalGrid=True,retailReplay=False,validationErrors=[])
images={};roi=np.s_[32:688,152:808]
def persist():
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
def metric(first,second):
    d=np.abs(first[roi]-second[roi])
    return dict(maximum=float(d.max()),mean=float(d.mean()),p99=float(np.quantile(d,.99)),
                changedPixels=int(np.count_nonzero(d.max(axis=2)>1)))
def stable():
    assert fingerprints=={label:sha(path) for label,path in exes.items()},'Standalone binaries changed during GPU run'
    assert binary_hashes=={label:{name:sha(folder/name) for name in binary_hashes[label]} for label,folder in binary_folders.items()},'Main renderer binaries changed during GPU run'
    for row in builds.values():assert sha(Path(row['library']))==row['librarySha256'],'Frozen renderer library changed'
    assert sha(ROOT/'tests/shared_plus_scanned_scene.cpp')==proof['fixtureSha256'],'Fixture source changed'
def run(scan,label,key,flags,validation=False):
    stable();directory=out/(str(scan)+'-'+key)/label;directory.mkdir(parents=True,exist_ok=True)
    command=[str(exes[label]),'--scan',str(ROOT/f'build/environment-078-r71/triangle-audit/scan-{scan}'),
             '--orthographic','--normals-up','--albedo-only','--hd-ground','1','--frames','100','--output','frame.bmp',*flags]
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache),VK_LOADER_LAYERS_DISABLE='~implicit~')
    if validation:env.update(VK_LAYER_PATH=str(ROOT/'build/deps/vulkan-validation-1.4.321/build-x86/layers'),
                             VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LOADER_DEBUG='layer',VK_LAYER_VALIDATE_SYNC='1')
    if not a.validate_only:
        with (directory/'run.log').open('w') as log:
            subprocess.run(command,cwd=directory,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
    log=(directory/'run.log').read_text(errors='replace')
    errors=[line for line in log.splitlines() if re.search(r'VUID-|Validation Error|SYNC-HAZARD|device lost|VK_ERROR',line,re.I)]
    result['validationErrors'].extend(errors)
    assert not errors,(scan,label,key,errors[:3])
    if validation:assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log,'Validation layer inactive'
    assert 'original AO grid/corner heights retained' in log
    image=np.asarray(Image.open(directory/'frame.bmp').convert('RGB'),dtype=float)
    dump=(directory/'frame.bmp.txt').read_text(errors='replace')
    # A mostly flat cliff-weight diagnostic can correctly be almost black.
    # Prove a real populated terrain target using its independent albedo output.
    albedo=np.asarray(Image.open(directory/'frame.bmp-albedo.bmp').convert('RGB'),dtype=float)
    assert image.shape==(720,960,3) and albedo[roi].mean()>2 and albedo[roi].std()>1,(scan,label,key,'Missing actual terrain surface')
    row=dict(command=command,frameSha256=sha(directory/'frame.bmp'),dumpSha256=sha(directory/'frame.bmp.txt'),
             roiMeanRgb=image[roi].mean(axis=(0,1)).tolist(),roiStd=float(image[roi].std()),roiSpatialStdRgb=image[roi].std(axis=(0,1)).tolist(),validation=validation)
    result['runs'][f'{scan}/{label}/{key}']=row;persist();print(scan,label,key,'rendered',flush=True)
    return image,dump,log
try:
    if not a.controls_only:
        for scan in ['215604','215630','215655','220250']:
            result['diagonals'][scan]={}
            for stage in [22,25,26]:
                by_version={}
                for label in ['before','after']:
                    pair=[]
                    for diagonal in [0,1]:
                        image,_,_=run(scan,label,f'debug{stage}-diagonal{diagonal}',
                                      ['--debug',str(stage),'--diagonal',str(diagonal)],
                                      validation=scan=='215604' and label=='after' and stage==22 and diagonal==0)
                        pair.append(image)
                    by_version[label]=metric(*pair)
                result['diagonals'][scan][str(stage)]=by_version;persist()
                print('DIAGONAL',scan,stage,json.dumps(by_version),flush=True)
        before_gap=sum(row['22']['before']['mean'] for row in result['diagonals'].values())
        after_gap=sum(row['22']['after']['mean'] for row in result['diagonals'].values())
        result['materialPositiveControl']=dict(beforeDiagonalMeanSum=before_gap,afterDiagonalMeanSum=after_gap)
        assert before_gap>.5,'Old diagonal artifact was not exercised'
        assert after_gap<before_gap*.2,'Material diagonal dependence did not materially improve'
        for scan,row in result['diagonals'].items():
            for stage in ['22','25','26']:
                assert row[stage]['after']['mean']<.2,(scan,stage,'Residual diagonal discontinuity',row[stage])
        result['normalFieldResponse']={scan:result['runs'][f'{scan}/after/debug25-diagonal0']['roiSpatialStdRgb'] for scan in result['diagonals']}
        assert all(max(spread)>1 for spread in result['normalFieldResponse'].values()),'Height-derived shading normals became flat'
        result['normalMapTestScope']='Native height-field normals and post-material diagnostic continuity; no extra normal map is attached by this fixture.'
    scan='215604'
    for label in ['before','after']:
        pairs={}
        for stage in [21,0]:
            off,offdump,_=run(scan,label,f'caster-debug{stage}-off',['--debug',str(stage),'--caster','--lighting','--ambient','.15'])
            on,ondump,_=run(scan,label,f'caster-debug{stage}-on',['--debug',str(stage),'--caster','--shadows','--ambient','.15'],validation=label=='after' and stage==21)
            pairs[str(stage)]=metric(off,on)
            assert re.search(r'# sun state: enabled 1; valid 1',ondump),(label,'Actual shadow state not valid')
            assert 'scanned-fixture-caster' in ondump,(label,'Cube caster draw absent')
            assert pairs[str(stage)]['changedPixels']>20,(label,stage,'No actual caster shadow pixels')
        result['shadows'][label]=pairs;persist()
    for label in ['before','after']:
        image,_,log=run('215604',label,'warmth-diffuse20',['--debug','20','--lighting','--ambient-only','--area-light','.6','--sun-intensity','1',
            '--ambient-rgb','.6352941176','.1960784314','.1960784314'])
        assert 'RS_AMBIENT=FFA23232' in log
        result['warmth'][label]=dict(meanRgb=image[roi].mean(axis=(0,1)).tolist())
    old,new=[np.asarray(result['warmth'][label]['meanRgb']) for label in ['before','after']]
    result['warmth']['redToGreenRatioBefore']=float(old[0]/old[1])
    result['warmth']['redToGreenRatioAfter']=float(new[0]/new[1])
    result['warmth']['outputMeanDelta']=float(np.abs(new-old).mean())
    assert new[0]/new[1]>old[0]/old[1],'Volcanic ambient warmth did not increase'
    assert np.abs(new-old).mean()>.05,'Captured warm ambient did not reach the real diffuse hook'
    stable();result['result']='passed'
except Exception as error:
    result['result']='failed';result['error']=str(error);result['traceback']=traceback.format_exc();raise
finally:
    persist();print(json.dumps({k:v for k,v in result.items() if k!='runs'},indent=2),flush=True)
