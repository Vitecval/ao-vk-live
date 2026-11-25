"""Validate the private runtime pack and pin a concrete local test snapshot."""
import argparse,hashlib,json,wave
from pathlib import Path
from PIL import Image
import numpy as np
ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'build/environment-078-r65';PACK=ROOT/'material-packs/ao-penumbra-078'
parser=argparse.ArgumentParser();parser.add_argument('--revision',type=int,default=65);args=parser.parse_args()

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
manifest=read(PACK/'manifest.json');records=read(WORK/'individual-generation-records.json')
seen=set();retained=0;expected_maps=set()
for row in manifest['materials']:
    key=(row['type'],row['id']);assert key not in seen;seen.add(key)
    source=PACK/row['source'];assert source.is_relative_to(PACK) and sha(source)==row['source_sha256']
    if row.get('painting_atlas'):
        atlas=row['painting_atlas'];whole=PACK/atlas['source']
        assert whole.is_relative_to(PACK) and sha(whole)==atlas['sha256']
        with Image.open(whole) as painting,Image.open(source) as exported:
            assert max(painting.size)>=1024
            assert painting.convert('RGB').crop(tuple(atlas['box'])).tobytes()==exported.convert('RGB').tobytes()
    if row.get('palette_calibration'):
        palette=row['palette_calibration'];assert sha(Path(palette['source']))==palette['source_sha256']
        with Image.open(PACK/'maps'/f"{key[0]}_{key[1]}_d.png") as im:mean=np.asarray(im.convert('RGB'),dtype=float).mean(axis=(0,1))
        assert np.max(np.abs(mean-np.asarray(palette['target_mean_rgb'])))<2,('Paving palette no longer matches its terrain family',key)
    if row.get('original_alpha_source'):
        original=Path(row['original_alpha_source']);assert sha(original)==row['original_alpha_source_sha256']
        with Image.open(original) as im:alpha=im.convert('RGBA').getchannel('A').resize(tuple(row['size']),Image.Resampling.LANCZOS)
        with Image.open(PACK/'maps'/f"{key[0]}_{key[1]}_d.png") as im:assert im.convert('RGBA').getchannel('A').tobytes()==alpha.tobytes(),key
    if row.get('normal_policy')=='flat-functional-surface':
        with Image.open(PACK/'maps'/f"{key[0]}_{key[1]}_n.png") as im:
            assert np.all(np.asarray(im.convert('RGB'))==np.array([128,128,255])),('Functional graphic acquired surface relief',key)
    for suffix in row['maps']:
        name=f'{key[0]}_{key[1]}_{suffix}.png';file=PACK/'maps'/name
        expected_maps.add(name)
        assert sha(file)==manifest['files'][name],name
        with Image.open(file) as im:im.verify()
    for suffix,old in row.get('retained_companions',{}).items():
        assert sha(PACK/'maps'/f'{key[0]}_{key[1]}_{suffix}.png')==old['sha256'];retained+=1
for name,digest in manifest['files'].items():
    assert Path(name).name==name and sha(PACK/'maps'/name)==digest
assert expected_maps==set(manifest['files']),'Unreferenced runtime map in manifest'
assert {file.name for file in (PACK/'maps').glob('*.png')}==expected_maps,'Unreviewed/orphan runtime maps'
for row in records:
    source=Path(row['source']);assert source.is_relative_to(ROOT)
    with Image.open(source) as im:
        if max(im.size)<1024:
            atlas=row.get('material',{}).get('painting_atlas')
            assert atlas and sha(Path(atlas['source']))==atlas['sha256'],(row['identity'],'source not HD')
            with Image.open(atlas['source']) as full:
                assert max(full.size)>=1024 and full.convert('RGB').crop(tuple(atlas['box'])).tobytes()==im.convert('RGB').tobytes()
        assert all(a>=b for a,b in zip(im.size,row['runtime_size'])),(row['identity'],'upscaled')
required={'1010004:209665':[1024,1024],'1010004:211094':[1024,512],'1010004:209763':[1024,1024]}
by_id={row['identity']:row for row in records}
for identity,size in required.items():assert by_id[identity]['runtime_size']==size
checks={};historical_checks={};reference_evidence={};release_check_paths={};historical_input_snapshots={}
def saved_check(name):
    path=WORK/name/'RESULTS.json'
    if args.revision>=70:
        if path.exists():
            report=read(path)
            historical_checks[name]=dict(sha256=sha(path),recordedResult=report.get('result',report.get('status')),classification='historical-not-current-binary')
        return
    assert path.exists() and read(path)['result']=='passed',name+' check required before pinning'
    checks[name]=sha(path)
for name in ['authored-snow-check','verified-regional','terrain-followup-check','hd-ground-check','hd-ground-seams-check','hd-path-layout-check','billboard-alpha-check','shared-residency-check','grid-sign-check']:
    saved_check(name)
if args.revision>=66:
    for name in ['grid-label-family-check',('screen-native-r68-final' if args.revision>=68 else 'screen-native-check'),'sound-check']:
        if args.revision<70 or name!='sound-check':saved_check(name)
    screen=WORK/('screen-native-r68-final' if args.revision>=68 else 'screen-native-check')/'RESULTS.json'
    if args.revision<69:
        assert read(screen)['executableSha256']==sha(ROOT/'build/preview23/x86/rvk_demo.exe'),'Screen rendering fixture changed after validation'
if args.revision>=68:
    for name in ['shared-plus-classic-r68','shared-snow-lighting-check','hd-seams-r68','hd-path-r68']:
        saved_check(name)
    if args.revision<69:
        assert read(WORK/'shared-plus-classic-r68/RESULTS.json')['binarySha256']=={name:sha(ROOT/'build/preview23/x86'/name) for name in ['rvk_demo.exe','ao-vk.dll']}
    assert read(WORK/'sound-check/RESULTS.json')['settings']['rendererSha256']==sha(ROOT/'build/preview23/x86/ao-vk.dll'),'Sound settings tested against a different renderer'
if args.revision>=69:
    build=(ROOT/'build/preview23/x86').resolve()
    current={name:sha(build/name) for name in ['rvk_demo.exe','ao-vk.dll']}
    def latest(name,status='result'):
        path=WORK/name/'RESULTS.json'
        assert path.is_file(),name+' latest-binary check required before pinning'
        report=read(path)
        assert report.get(status)=='passed',name+' latest-binary check did not pass'
        checks[name]=sha(path)
        return report
    def demo_check(name,status='result'):
        report=latest(name,status)
        assert report['executableSha256']==current['rvk_demo.exe'],name+' tested a different executable'
        return report
if args.revision==69:
    demo_check('high-resolution-check','status')
    policy=latest('high-resolution-policy-check')
    policy_binaries=policy['binarySha256']
    policy_required={'rvk_demo.exe','ao-vk.dll','display_mode_policy_check.exe','display_dpi_policy_check.exe',
        'display_dpi_window_check.exe','framebuffer_transaction_check.exe','native_display_layout_check.exe',
        'native_resize_integration_check.exe','presentation_policy_check.exe'}
    assert policy_required<=set(policy_binaries),'Missing high-resolution native policy/window binary evidence'
    for name,digest in policy_binaries.items():
        assert Path(name).name==name and sha(build/name)==digest,('Policy binary changed after testing',name)
    failures=latest('target-resize-failure-r69')
    assert failures['executableSha256']==sha(build/'target_resize_failure_check.exe') and failures['rendererSha256']==current['ao-vk.dll']
    assert failures['actualGpuFailureCases']==15 and failures['validationErrors']==0 and failures['retainedOldPixels']
    wsi_path=WORK/'high-resolution-wsi-isolated-check/RESULTS.json'
    wsi=read(wsi_path)
    assert wsi['result']=='functional-passed-validation-warning' and wsi['validationPassed'] is False
    assert wsi['binarySha256']['ao-vk.dll']==current['ao-vk.dll'] and wsi['binarySha256']['rvk_demo.exe']==current['rvk_demo.exe']
    assert wsi['binarySha256']['fsr_lifecycle_check.exe']==sha(build/'fsr_lifecycle_check.exe')
    assert len(wsi['cases'])==3 and all(case['exitCode']==0 for case in wsi['cases'])
    checks['high-resolution-wsi-isolated-check']=sha(wsi_path)
    window=latest('window-repaint-r69')
    assert window['rendererSha256']==current['ao-vk.dll']
    assert window['executableSha256']==sha(build/'window_repaint_check.exe')
    feature=latest('hd-ground-feature-r69')
    assert feature['rendererBinaries'][str(build)]==current,'HD feature test used a different current build'
    caustic=demo_check('caustic-receiver-r69')
    assert caustic['hdrOffReceiverTreatment'] is False,'HDR-off fallback was not actually exercised'
    demo_check('underwater-bubble-r69')
    classic=latest('shared-plus-classic-r69')
    assert classic['binarySha256']==current,'Classic ground check used a different build'
    snow=latest('shared-snow-lighting-r69')
    assert snow['binarySha256'][str(build)]==current,'Snow lighting check used a different current build'
    for name in ['hd-seams-r69','hd-path-r69']:
        report=demo_check(name)
        assert report['binarySha256']==current,name+' renderer pair changed after testing'
    demo_check('screen-native-r69')
    sound=read(WORK/'sound-check/RESULTS.json')
    assert all(sound[key] is True for key in ['defaultOff','allPresetsOff','settingPersisted',
        'independentOfGraphics','actualUiTogglePassed','volumeControlPassed'])
    assert sound['settings']['choicePersisted'] is True and sound['settings']['volumePersisted'] is True
    assert sound['runtimeManifestSha256']==sha(WORK/'runtime-assets.json'),'Sound report has a stale runtime delivery manifest'
if args.revision==70:
    for name in ['target-resize-failure-r69','high-resolution-wsi-isolated-check','high-resolution-check',
                 'high-resolution-policy-check','window-repaint-r69','hd-ground-feature-r69',
                 'caustic-receiver-r69','underwater-bubble-r69','shared-plus-classic-r69',
                 'shared-snow-lighting-r69','hd-seams-r69','hd-path-r69','screen-native-r69']:
        saved_check(name)
    evidence_root=ROOT/'build/environment-078-r70'
    def evidence(path):
        assert path.is_file(),str(path)+' reference-build evidence is required'
        reference_evidence[str(path.relative_to(ROOT)).replace('\\','/')]=sha(path)
        return read(path)
    independent=evidence(evidence_root/'independent-077-build/BUILD-REPORT.json')
    assert independent['result']=='passed' and independent['sourceEdits'] is False and independent['gpuRun'] is False
    input_manifest=ROOT/'build/horizon-0.7.7/ENGINE-BUILD-INPUTS.json'
    inputs=evidence(input_manifest)
    assert sha(input_manifest)=='4b5fb6b3b007b063dc38bf1d298da85cd27583dcfe2ef9439cf2459f909ad394','Reference is not the verified published077 build-input manifest'
    assert inputs['sourceCommit']=='69a00600acfb2fb7cbf327d424f91c535e9049e4'
    assert independent['manifestSha256']==sha(input_manifest) and len(inputs['files'])==790
    reference_source=Path(independent['source'])
    assert reference_source.resolve()==(ROOT/'build/horizon-0.7.7/engine-source').resolve()
    for item in inputs['files']:
        path=reference_source/item['path']
        assert path.stat().st_size==item['bytes'] and sha(path)==item['sha256'],('Immutable077 build input changed',item['path'])
    for phase,key in [('BEFORE','beforeReportSha256'),('AFTER','afterReportSha256')]:
        path=evidence_root/f'independent-077-build/{phase}-SOURCE.json';report=evidence(path)
        assert sha(path)==independent[key] and report['result']=='passed' and report['fileCount']==790 and report['mismatches']==[]
        assert report['manifestSha256']==independent['manifestSha256']
    commands=evidence_root/'independent-077-build/COMMANDS.json';evidence(commands)
    assert sha(commands)==independent['commandsSha256']
    reference_demo=Path(independent['executable'])
    assert sha(reference_demo)==independent['executableSha256']
    shared=latest('shared077-render-r70')
    assert shared['positiveControlsPassed'] is True and shared['actualGpuSurfaceColourReadback'] is True and len(shared['cases'])==12
    assert shared['executableSha256']=={'reference':sha(reference_demo),'restored':current['rvk_demo.exe']}
    assert all(case['maximum']<=1 and case['mean']<.05 for case in shared['cases'])
    scanned_build=evidence(evidence_root/'scanned-scene-build/BUILD-REPORT.json')
    assert scanned_build['result']=='passed' and scanned_build['frozenInputsChanged']==[]
    assert scanned_build['rendererLibraryRebuilt'] is False and scanned_build['rendererDllModified'] is False
    assert scanned_build['fixtureSha256']==sha(ROOT/'tests/shared_plus_scanned_scene.cpp')
    before=evidence(evidence_root/'scanned-scene-build/INPUTS-BEFORE.json')
    after=evidence(evidence_root/'scanned-scene-build/INPUTS-AFTER.json')
    assert before==after,'Scanned fixture was linked while frozen inputs changed'
    for path,digest in after.items():assert sha(Path(path))==digest,('Scanned build input changed',path)
    evidence(evidence_root/'scanned-scene-build/COMMANDS.json')
    builds={row['version']:row for row in scanned_build['builds']}
    assert set(builds)=={'current','reference'}
    assert Path(builds['current']['library']).resolve()==(build/'rvk.lib').resolve()
    assert Path(builds['reference']['library']).resolve()==(Path(independent['build'])/'rvk.lib').resolve()
    assert Path(builds['current']['headerRoot']).resolve()==ROOT.resolve()
    assert Path(builds['reference']['headerRoot']).resolve()==reference_source.resolve()
    fixture_hashes={}
    for label,row in builds.items():
        assert sha(Path(row['library']))==row['librarySha256']
        assert after[row['library']]==row['librarySha256']
        exe=Path(row['executable']);assert sha(exe)==row['executableSha256'];fixture_hashes[label]=row['executableSha256']
        commands=evidence_root/f'scanned-scene-build/{label}-commands.log'
        assert sha(commands)==row['linkCommandsSha256']
        reference_evidence[str(commands.relative_to(ROOT)).replace('\\','/')]=sha(commands)
        assert sha(exe.parent/'compile_commands.json')==row['compileCommandsSha256']
    assert after[str(build/'ao-vk.dll')]==current['ao-vk.dll'] and after[str(build/'rvk_demo.exe')]==current['rvk_demo.exe']
    scanned=latest('scanned-shared077-r70')
    assert len(scanned['cases'])==8 and scanned['executableSha256']==fixture_hashes
    assert scanned['hdOffAndOnCompared'] is True and scanned['sideLoadedPaintingsRejected'] is True
    assert all(case['maximum']<=1 and case['mean']<.05 for case in scanned['cases'])
    snow=latest('snow-preservation-r70')
    assert snow['binarySha256']['after']==current
    assert snow['binarySha256']['before']=={name:sha(evidence_root/'before'/name) for name in current}
    caustic=demo_check('caustic-receiver-r70');assert caustic['hdrOffReceiverTreatment'] is False
    demo_check('currenthigh-resolution-r70','status')
    sound=latest('sound-check')
    assert sound['settings']['rendererSha256']==current['ao-vk.dll']
    assert all(sound[key] is True for key in ['defaultOff','allPresetsOff','settingPersisted',
        'independentOfGraphics','actualUiTogglePassed','volumeControlPassed'])
    assert sound['settings']['choicePersisted'] is True and sound['settings']['volumePersisted'] is True
    assert sound['runtimeManifestSha256']==sha(WORK/'runtime-assets.json')
if args.revision>=71:
    # Natural material-field repair deliberately differs from077 on slopes.
    # Require actual final-source/current-library evidence; never apply r70's
    # whole scanned-scene pixel-parity assertion to this release.
    evidence_root=ROOT/'build/environment-078-r71'
    for name in ['target-resize-failure-r69','high-resolution-wsi-isolated-check','high-resolution-check',
                 'high-resolution-policy-check','window-repaint-r69','hd-ground-feature-r69',
                 'caustic-receiver-r69','underwater-bubble-r69','shared-plus-classic-r69',
                 'shared-snow-lighting-r69','hd-seams-r69','hd-path-r69','screen-native-r69',
                 'shared077-render-r70','scanned-shared077-r70','snow-preservation-r70',
                 'caustic-receiver-r70','currenthigh-resolution-r70']:
        saved_check(name)
    def evidence(path):
        assert path.is_file(),str(path)+' release evidence is required'
        reference_evidence[str(path.relative_to(ROOT)).replace('\\','/')]=sha(path)
        return read(path)
    def r71_check(name,relative,status='result'):
        path=evidence_root/relative;report=evidence(path)
        assert report.get(status)=='passed',name+' has not passed'
        checks[name]=sha(path);release_check_paths[name]=str(path.relative_to(ROOT)).replace('\\','/')
        return report
    prior=evidence(WORK/'PIN-r70.json');prior_sources=WORK/'SOURCE-FILES-r70.json';evidence(prior_sources)
    assert prior['revision']==70 and sha(prior_sources)==prior['sourceManifestSha256'],'Preserved r70 PIN/source proof mismatch'
    assert prior['materialManifestSha256']==sha(PACK/'manifest.json'),'Unreviewed material change since r70'
    assert prior['runtimeAssetsManifestSha256']==sha(WORK/'runtime-assets.json'),'Unreviewed audio change since r70'
    for name,digest in prior['referenceBuildEvidence'].items():
        assert sha(ROOT/name)==digest,('Preserved independent reference evidence changed',name)
        reference_evidence[name]=digest
    shader_hash=sha(ROOT/'rvk/shaders/terrain.glsl')
    cpu=r71_check('r71-natural-field-cpu','natural-field-cpu-check/RESULTS.json')
    # Historical field name: this CPU runner records its SHADER hash here.
    assert cpu['rendererSha256']==shader_hash and cpu['gpuExecuted'] is False
    assert cpu['samplesChecked']>=176144 and len(cpu['cases'])==4
    assert set(cpu['scans'])=={'215604','215630','215655','220250'}
    assert all(c['previousMaximumGap']>.7 and c['repairedMaximumGap']==0 for c in cpu['cases'].values())
    for scan,row in cpu['scans'].items():
        assert row['maximumMaterialGap']==0 and row['maximumSideUvOrJacobianDelta']==0
        for name,digest in row['inputsSha256'].items():
            path=evidence_root/'triangle-audit'/f'scan-{scan}'/name
            assert Path(name).name==name and sha(path)==digest
            reference_evidence[str(path.relative_to(ROOT)).replace('\\','/')]=digest
    independent=r71_check('r71-independent-checks','independent-checks/RESULTS.json')
    assert independent['shaderSha256']==shader_hash and independent['gpuExecuted'] is False
    assert independent['sourceReferenceExit']==0 and independent['naturalFieldCpuResult']=='passed'
    assert independent['samplesChecked']==cpu['samplesChecked'] and independent['warmthExit']==0
    assert independent['warmthExeSha256']==sha(build/'inferno_warmth_check.exe')
    reference=evidence(evidence_root/'independent-checks/source-reference.json')
    assert reference['result']=='passed' and reference['gpu_executed'] is False
    assert reference['snapshot_sha256']=='9551f90bd3bd25ca8e66e776ca4ee74a14b85efef1616bd5fda6af12f0a1262d'
    assert sha(ROOT/'build/horizon-0.7.7/source-final/rvk/shaders/terrain.glsl')==reference['snapshot_sha256']
    proof_path=evidence_root/'scanned-scene-build/BUILD-REPORT.json';proof=evidence(proof_path)
    assert proof['result']=='passed' and proof['frozenInputsChanged']==[] and proof['gpuRun'] is False
    assert proof['rendererLibraryRebuilt'] is False and proof['rendererDllModified'] is False
    assert proof['r70PublicHeaderMatched'] is True and proof['r70MaterialHeaderMatched'] is True
    assert proof['singleStbImplementationPerExecutable'] is True
    assert proof['fixtureSha256']==sha(ROOT/'tests/shared_plus_scanned_scene.cpp')
    before=evidence(evidence_root/'scanned-scene-build/INPUTS-BEFORE.json')
    after=evidence(evidence_root/'scanned-scene-build/INPUTS-AFTER.json');assert before==after
    for path,digest in after.items():
        source=Path(path)
        if source.resolve()==(WORK/'SOURCE-FILES.json').resolve():
            assert digest==prior['sourceManifestSha256']
            source=prior_sources
            historical_input_snapshots[str(Path(path).relative_to(ROOT)).replace('\\','/')]=dict(
                snapshot=str(source.relative_to(ROOT)).replace('\\','/'),sha256=digest)
        assert sha(source)==digest,('Scanned build input changed',path)
    evidence(evidence_root/'scanned-scene-build/COMMANDS.json')
    builds={row['version']:row for row in proof['builds']};assert set(builds)=={'before','after'}
    assert proof['r70PinSha256']==sha(WORK/'PIN-r70.json')
    assert sha(evidence_root/'before/PIN-r70.json')==proof['r70PinSha256']
    assert sha(evidence_root/'before/ao-vk.dll')==prior['rendererSha256']
    assert Path(builds['before']['library']).resolve()==(evidence_root/'before/rvk.lib').resolve()
    assert Path(builds['after']['library']).resolve()==(build/'rvk.lib').resolve()
    fixture_hashes={}
    for label,row in builds.items():
        assert Path(row['headerRoot']).resolve()==ROOT.resolve()
        assert sha(Path(row['library']))==row['librarySha256']==after[row['library']]
        exe=Path(row['executable']);assert sha(exe)==row['executableSha256'];fixture_hashes[label]=row['executableSha256']
        commands=evidence_root/f'scanned-scene-build/{label}-commands.log'
        assert sha(commands)==row['linkCommandsSha256'];reference_evidence[str(commands.relative_to(ROOT)).replace('\\','/')]=sha(commands)
        assert sha(exe.parent/'compile_commands.json')==row['compileCommandsSha256']
    assert after[str(build/'ao-vk.dll')]==current['ao-vk.dll'] and after[str(build/'rvk_demo.exe')]==current['rvk_demo.exe']
    gpu=r71_check('r71-triangle-render','triangle-render-final-check/RESULTS.json')
    assert gpu['buildProofSha256']==sha(proof_path) and gpu['fixtureSha256']==proof['fixtureSha256']
    assert gpu['executableSha256']==fixture_hashes and gpu['controlledOriginalGrid'] is True and gpu['retailReplay'] is False
    assert gpu['binarySha256']==current,'Natural-field GPU report used a different DLL/demo build'
    assert gpu['validationErrors']==[] and set(gpu['diagonals'])==set(cpu['scans'])
    for scan,row in gpu['diagonals'].items():
        assert set(row)=={'22','25','26'}
        for stage,pair in row.items():
            assert set(pair)=={'before','after'} and pair['after']['mean']<.2,(scan,stage,'Residual diagonal dependence')
    positive=gpu['materialPositiveControl']
    assert positive['beforeDiagonalMeanSum']>.5 and positive['afterDiagonalMeanSum']<positive['beforeDiagonalMeanSum']*.2
    for label in ['before','after']:
        for stage in ['21','0']:assert gpu['shadows'][label][stage]['changedPixels']>20
    assert gpu['warmth']['redToGreenRatioAfter']>gpu['warmth']['redToGreenRatioBefore']
    assert gpu['warmth']['outputMeanDelta']>.05
    assert len(gpu['runs'])>=58 and sum(bool(row['validation']) for row in gpu['runs'].values())>=2
    for key,row in gpu['runs'].items():
        scan,label,name=key.split('/');assert label in builds and scan in cpu['scans'] and Path(name).name==name
        directory=evidence_root/'triangle-render-final-check'/(scan+'-'+name)/label
        assert row['command'][0]==builds[label]['executable']
        for filename,field in [('frame.bmp','frameSha256'),('frame.bmp.txt','dumpSha256')]:
            assert sha(directory/filename)==row[field],('GPU evidence changed',key,filename)
    # These independent077 controls do not load a native height grid. They
    # verify the unchanged colour path, not077 pixel parity on repaired slopes.
    shared=r71_check('r71-shared077-render','shared077-render-check/RESULTS.json')
    original_build=read(ROOT/'build/environment-078-r70/independent-077-build/BUILD-REPORT.json')
    reference_demo=Path(original_build['executable'])
    assert sha(reference_demo)==original_build['executableSha256']
    assert shared['executableSha256']=={'reference':sha(reference_demo),'restored':current['rvk_demo.exe']}
    assert shared['positiveControlsPassed'] is True and shared['actualGpuSurfaceColourReadback'] is True
    assert len(shared['cases'])==12 and all(c['maximum']<=1 and c['mean']<.05 for c in shared['cases'])
    snow=r71_check('r71-snow-preservation','snow-preservation-check/RESULTS.json')
    assert snow['binarySha256']['after']==current
    assert snow['binarySha256']['before']=={name:sha(evidence_root/'before'/name) for name in current}
    assert set(snow['cases'])=={'near','far'}
    for distance,row in snow['cases'].items():
        for label in ['before','after']:
            controls=row[label+'PositiveControls']
            assert controls['normalMapPerturbedPixels']>100 and controls['hdMaterialChangedPixels']>100 and controls['powderChangedPixels']>50
    resolution=r71_check('r71-high-resolution','high-resolution-check/RESULTS.json','status')
    assert resolution['executableSha256']==current['rvk_demo.exe'] and len(resolution['cases'])==11
    assert resolution['physicalPresentationTested'] is False and resolution['independentInterfaceScale'] is False
    sound=latest('sound-check')
    assert sound['settings']['rendererSha256']==current['ao-vk.dll']
    assert all(sound[key] is True for key in ['defaultOff','allPresetsOff','settingPersisted',
        'independentOfGraphics','actualUiTogglePassed','volumeControlPassed'])
    assert sound['settings']['choicePersisted'] is True and sound['settings']['volumePersisted'] is True
    assert sound['runtimeManifestSha256']==sha(WORK/'runtime-assets.json')
    settings_path=Path(sound['currentSettingsEvidence']['path'])
    settings_report=evidence(settings_path)
    assert sha(settings_path)==sound['currentSettingsEvidence']['sha256']
    assert settings_report['result']=='passed' and settings_report['gpuExecuted'] is False
    assert settings_report['audioDeviceOpened'] is False and settings_report['liveClientSettingsChanged'] is False
    assert settings_report['binarySha256']=={'ao-vk.dll':current['ao-vk.dll'],
        'preview23_settings_check.exe':sha(build/'preview23_settings_check.exe')}
    assert settings_report['settingsFixtureSourceSha256']==sha(ROOT/'tests/preview23_settings_check.cpp')
    fixture=Path(settings_report['fixture'])
    for phase,row in settings_report['phases'].items():
        assert phase in ['save','check'] and row['exitCode']==0
        assert row['savedSound']==(1 if phase=='save' else 0) and row['savedVolume']==.25
        assert sha(fixture/(phase+'.log'))==row['logSha256']==sound['currentSettingsPhases'][phase]
        assert sha(fixture/(phase+'-settings.ini'))==row['settingsSha256']
    assert set(settings_report['phases'])=={'save','check'}
    ui=sound['packagedUiEvidence'];assert sha(Path(ui['path']))==ui['sha256']
    assert read(Path(ui['path']))['result']=='passed'
sources=['CMakeLists.txt','rvk/area_profiles.h','rvk/environment_materials.h','rvk/terrain_materials.h','rvk/native_ground.cpp','rvk/sky.cpp','rvk/draw.cpp','rvk/water.cpp','rvk/pointshadow.cpp','rvk/lava_lighting.h','rvk/rvk.h','rvk/shaders/ffp.vert','rvk/shaders/ffp.tesc','rvk/shaders/ffp.tese','rvk/shaders/ffp_main.glsl','rvk/shaders/frame_lights.glsl','rvk/shaders/lighting.glsl','rvk/shaders/parallax.glsl','rvk/shaders/terrain.glsl','rvk/shaders/snow_surface.glsl','rvk/shaders/sky_glow.frag','rvk/shaders/sky_enhance.glsl','rvk/shaders/water.frag','proxy/ddraw/rvk_materials.cpp','rvk/assets/terrain/lava-surface.png']
sources+=['rvk/shadow.cpp','rvk/diag.cpp','rvk/resources.cpp','rvk/threaded.h','rvk/threaded.cpp','proxy/ddraw/rvk_settings.cpp','tests/environment_followup_render.py','tests/hd_ground_material_render.py']
sources+=['rvk/shared_terrain.cpp','tests/rvk_demo.cpp','tests/hd_ground_seams_render.py','tests/lava_lighting_check.cpp']
sources+=['tests/hd_ground_path_render.py','tools/environment-repaint.py','tools/integrate-environment-repaints.py','tools/validate-environment-repaints.py']
sources+=['rvk/foliage_alpha.h','rvk/leaves.cpp','tests/foliage_alpha_check.cpp']
sources+=['tests/billboard_alpha_render.py']
sources+=['proxy/native/scene.cpp','proxy/native/attachment_index.h','tests/attachment_index_check.cpp','tests/native_material_residency_render.py']
sources+=['tests/grid_sign_render.py','rvk/shaders/constants.glsl']
sources+=['rvk/internal.h']
if args.revision>=66:
    sources+=['tests/screen_role_render.py','rvk/sound_policy.h','proxy/ddraw/rvk_sound.h','proxy/ddraw/rvk_sound.cpp',
              'proxy/ddraw/rvk_statels.cpp','proxy/ddraw/rvk_ddraw.cpp','tests/sound_policy_check.cpp',
              'tests/preview23_settings_check.cpp','apps/horizon/Models.cs','apps/horizon/PresetTests.cs',
              'tools/prepare-private-ambient-pack.py','tools/setup-safety.ps1']
if args.revision>=68:
    sources+=['tests/shared_plus_classic_surface_render.py','tests/shared_snow_lighting_render.py',
              'SoundRework/tools/prepare_runtime_field_ambience.py']
if args.revision>=69:
    sources+=['proxy/display_window_extent_policy.h','proxy/window_callback_policy.h','tools/gen_com_trace.py','proxy/ddraw/com_stub.gen.h','tests/fsr_lifecycle_check.cpp','proxy/display_mode_policy.h','proxy/display_mode_win32.h','proxy/display_dpi_policy.h',
              'proxy/display_dpi_win32.h','proxy/framebuffer_transaction.h',
              'proxy/native/display_layout_policy.h','proxy/native/main_surface_policy.h',
              'proxy/native/resize_queue_policy.h','proxy/native/randy_init.cpp','proxy/native/randy_init.h',
              'proxy/native/devices.cpp','proxy/native/devices.h','proxy/native/viewport.cpp','proxy/native/viewport.h',
              'proxy/native/camera.cpp','proxy/native/camera.h','proxy/native/gui.cpp',
              'proxy/ddraw/rvk_backend.h','proxy/ddraw/rvk_device.cpp','proxy/ddraw/rvk_surface.cpp',
              'proxy/ddraw/rvk_input.cpp','rvk/device.cpp','rvk/startup.cpp','rvk/presentation_policy.h',
              'rvk/underwater_policy.h','rvk/caustic_policy.h','rvk/rain.cpp',
              'rvk/shaders/ambient.vert','rvk/shaders/ambient.frag',
              'tests/display_mode_policy_check.cpp','tests/display_dpi_policy_check.cpp',
              'tests/display_dpi_window_check.cpp','tests/display-mode-options-cases.tsv',
              'tests/framebuffer_transaction_check.cpp','tests/native_display_layout_check.cpp',
              'tests/native_resize_integration_check.cpp','tests/presentation_policy_check.cpp',
              'tests/target_resize_failure_check.cpp','docs/fsr-stability-audit.md','RELEASE-NOTES.md','docs/settings.md','docs/rvk.md','README.md','tests/window_repaint_check.cpp','tests/resolution_render.py','tests/hd_ground_feature_render.py',
              'tests/caustic_policy_check.cpp','tests/caustic_receiver_render.py',
              'tests/underwater_policy_check.cpp','tests/underwater_bubble_scene.h','tests/underwater_bubble_render.py',
              'tools/install-live.ps1','tools/prepare-horizon-assets.py','tools/build-horizon.ps1','tools/package-horizon.py',
              'docs/display-mode-discovery.md','docs/native-high-resolution-display.md',
              'build/environment-078-r65/verify-private-bundle-r69.py',
              'build/environment-078-r65/verify-horizon-r69.ps1',
              'build/environment-078-r69/prepare-bubble-audio.py','build/environment-078-r69/prepare-runtime.py']
    sources += [str(path.relative_to(ROOT)).replace('\\','/') for path in (ROOT/'apps/horizon').glob('*.cs')]
    sources += ['apps/horizon/MainWindow.xaml','apps/horizon/App.xaml','apps/horizon/Horizon.csproj',
                'apps/horizon/app.manifest','apps/horizon/Assets/settings.json','apps/horizon/Assets/patch-notes.txt',
                'apps/horizon/README.md']
if args.revision>=70:
    sources+=['build/environment-078-r65/stage-private-horizon-r70.py','tests/shared_plus_077_reference.py','tests/shared_plus_077_render.py',
              'tests/shared_plus_scanned_scene.cpp','tests/shared_plus_scanned_render.py',
              'tests/shared_plus_snow_preservation.py','tests/scanned_cliff_scene.h',
              'docs/shared-plus-077-restoration-r70.md',
              'build/environment-078-r70/independent-077-build/build-reference.ps1',
              'build/environment-078-r70/independent-077-build/COMPARISON-NOTES.md',
              'build/environment-078-r70/scanned-scene-build/CMakeLists.txt',
              'build/environment-078-r70/scanned-scene-build/build-standalone.ps1',
              'build/environment-078-r65/verify-private-bundle-r70.py',
              'build/environment-078-r65/verify-horizon-r70.ps1']
if args.revision>=71:
    sources+=['tests/shared_plus_field_mapping_check.py','tests/scanned_triangle_render.py','tests/inferno_warmth_check.cpp',
              'build/environment-078-r71/scanned-scene-build/CMakeLists.txt',
              'build/environment-078-r71/scanned-scene-build/build-standalone.ps1',
              'build/environment-078-r65/stage-private-horizon-r71.py',
              'build/environment-078-r65/verify-private-bundle-r71.py',
              'build/environment-078-r65/verify-horizon-r71.ps1',
              'build/environment-078-r71/test-packaged-horizon.ps1','build/environment-078-r71/refresh-sound-settings.py',
              'build/environment-078-r71/install-horizon-private.ps1']
source_hashes={name:sha(ROOT/name) for name in sources}
(WORK/'SOURCE-FILES.json').write_text(json.dumps(source_hashes,indent=2)+'\n')
painted_ids={row['identity'] for row in records}
unique_paintings=len({row.get('painting_atlas',{}).get('sha256',row['source_sha256']) for row in manifest['materials'] if f"{row['type']}:{row['id']}" in painted_ids})
alias_count=sum(bool(row.get('material',{}).get('alias_of')) for row in records)
runtime_manifest=WORK/'runtime-assets.json'
runtime_hash=None
if runtime_manifest.exists():
    runtime=read(runtime_manifest);targets=set()
    for item in runtime['files']:
        target=item['target'];source=Path(item['source'])
        assert target.startswith('randy-vk/audio/ambience/') and '\\' not in target and '..' not in Path(target).parts and target not in targets
        assert source.is_relative_to(ROOT) and sha(source)==item['sha256']
        targets.add(target)
    assert targets,'Sound runtime manifest contains no assets'
    if args.revision>=69:
        prefix='randy-vk/audio/ambience/'
        wav_names={'grid.wav','city.wav','desert.wav','forest.wav','cold.wav','inferno.wav','underwater.wav'}
        assert targets=={prefix+name for name in wav_names|{'manifest.json','CREDITS.txt'}},'Expected exactly seven WAVs and two provenance files'
        delivery={Path(item['target']).name:item for item in runtime['files']}
        metadata=read(Path(delivery['manifest.json']['source']))
        assert metadata['revision']==69 and len(metadata['files'])==6
        for item in metadata['files']:
            assert delivery[item['filename']]['sha256']==item['sha256'],'Regional recording manifest mismatch'
        assert metadata['underwater']['sha256']==delivery['underwater.wav']['sha256']
        for name in wav_names:
            with wave.open(str(delivery[name]['source']),'rb') as wav:
                assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())==(2,2,48000) and wav.getnframes()>0,name+' is not the reviewed stereo PCM16 delivery'
    runtime_hash=sha(runtime_manifest)
elif args.revision>=69:
    raise AssertionError('The reviewed seven-delivery audio runtime manifest is required')
pin=dict(revision=args.revision,private=True,rendererSha256=sha(ROOT/'build/preview23/x86/ao-vk.dll'),cacheWarmSha256=sha(ROOT/'build/preview23/x86/ao-vk-cache-warm.exe'),materialManifestSha256=sha(PACK/'manifest.json'),individualNewPaintings=unique_paintings+1,individualIdentityRepaints=len(records),verifiedSourceAliases=alias_count,sharedAtlasAdditionalIdentities=len(records)-unique_paintings-alias_count,embeddedLavaPainting=1,sourceManifestSha256=sha(WORK/'SOURCE-FILES.json'),materialIdentities=len(seen),materialMaps=len(manifest['files']),retainedMaterialMasks=retained,checks=checks,retailPlaytest=False)
if args.revision>=70:
    pin['historicalChecks']=historical_checks
    pin['referenceBuildEvidence']=reference_evidence
if args.revision>=71:
    pin['releaseCheckPaths']=release_check_paths
    pin['historicalInputSnapshots']=historical_input_snapshots
    pin['terrainValidationScope']=dict(naturalSlopes='controlled captured-grid field/normal/UV and real-shadow GPU controls',
        unchangedColour='independent077 controls without native height grid',snow='current before/after preservation',
        manufacturedAndNonVolcanic='CPU/source controls only',retailScreenshotPixelParity=False)
if runtime_hash:pin['runtimeAssetsManifestSha256']=runtime_hash
if args.revision>=69:
    pin['displayLimitations']=dict(independentInterfaceScale=False,physical4kUltrawideVerified=False,retailInputVerified=False,frameGenerationSyncValidationPassed=False,frameGenerationHazardAlsoInPreviousBuild=True)
(WORK/'PIN.json').write_text(json.dumps(pin,indent=2)+'\n')
(WORK/'MATERIAL-INTEGRITY.json').write_text(json.dumps(dict(result='passed',**pin),indent=2)+'\n')
print(json.dumps(pin,indent=2))
