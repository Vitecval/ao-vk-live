"""Stage a local Preview 22 candidate with the exact frozen Preview 21 artwork.

Only code, authored metadata, redistributable helper dependencies and previously
accepted public art enter these archives. Installation-derived caches are never
inputs. Does not publish, update a release, install or launch a game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))


def package(baseline,build,helper,out):
    out.mkdir(parents=True,exist_ok=True)
    frozen=read(baseline/'ao-vk-live-0.6.0-preview.21-windows-x86-full/manifest.json')
    companions=[]
    for part in frozen.get('materialParts',[]):
        archive=baseline/part['name']
        if archive.stat().st_size!=part['bytes'] or sha(archive)!=part['sha256']:
            raise ValueError('Frozen material companion changed: '+part['name'])
        destination=out/part['name']
        if destination.exists() and sha(destination)!=part['sha256']:raise ValueError('Unrecognized existing material companion')
        if not destination.exists():shutil.copy2(archive,destination)
        if sha(destination)!=part['sha256']:raise ValueError('Companion copy integrity failure')
        companions.append(dict(path=str(destination),bytes=part['bytes'],sha256=part['sha256']))
    results=[]
    for edition in ('lite','full'):
        prior=baseline/f'ao-vk-live-0.6.0-preview.21-windows-x86-{edition}'
        old=read(prior/'manifest.json')
        if old['version']!='0.6.0-preview.21':raise ValueError('Exact Preview 21 materials required')
        target=out/f'ao-vk-live-0.6.0-preview.22-windows-x86-{edition}'
        if target.exists():raise ValueError('Use a new candidate output directory')
        target.mkdir()
        omitted={name for part in old.get('materialParts',[]) for name in part['files']} if edition=='full' else set()
        for name,expected in old['payload'].items():
            if name in omitted:continue
            path=prior/name
            if sha(path)!=expected:raise ValueError('Frozen payload changed: '+name)
            destination=target/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,destination)
        for name in ('ao-vk.dll','ddraw.dll','ao-vk-cache-warm.exe'):shutil.copy2(build/name,target/name)
        for name in ('install-live.ps1','setup-live.ps1','material-parts.ps1','material-download.ps1','prepare-living.ps1'):
            shutil.copy2(ROOT/'tools'/name,target/name)
        shutil.copy2(ROOT/'THIRD-PARTY-NOTICES.md',target/'THIRD-PARTY-NOTICES.md')
        shutil.copy2(ROOT/'RELEASE-NOTES.md',target/'RELEASE-NOTES.md')
        for name in ('CPython-local-helper.txt','Pillow-local-helper.txt'):
            shutil.copy2(ROOT/'licenses'/name,target/'licenses'/name)
        shutil.copy2(ROOT/'docs/preview22-living-local.md',target/'docs/preview22-living-local.md')
        shutil.copytree(helper,target/'living-preparer')
        metadata=target/'living-metadata';metadata.mkdir()
        for name in ('original-catalog.json','CITY-WARDROBES.json','routes-v4.json','playfield-sources-preview22.json'):
            shutil.copy2(ROOT/'experiment'/name,metadata/name)
        shutil.copy2(ROOT/'tools/ground-data.py',metadata/'ground-data.py')
        shutil.copytree(ROOT/'experiment/assets/city-wardrobes-v5',metadata/'city-wardrobes-v5')
        (target/'Prepare Newland Test.cmd').write_bytes(b'@echo off\r\npowershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0prepare-living.ps1" -NewlandTest\r\nif errorlevel 1 pause\r\n')
        (target/'README.md').write_text('''# Preview 22 local integration candidate

Unpublished test build; combined gameplay validation is pending.

Extract and run Install.cmd. Full HD uses the pinned Preview 21 material
companions automatically. The accepted artwork is frozen; texture workers are
not inputs. Setup prepares original Living resources from the selected AO
installation and keeps all derived data in its randy-vk/living-local directory.
It never modifies original game archives. A failed optional preparation leaves
the normal renderer installed and the previous local cache intact.

All Living settings default Off and remain independent of graphics presets.
For an isolated first test, close AO, run Prepare Newland Test.cmd, select the
AO folder, then launch normally and enable Ambient People Low in Newland.
That command explicitly opts into static-reviewed routes; it is not live
certification. Phasefront and additional orbital craft are unsupported and
disabled. Do not distribute the generated local cache.

Original AO model, texture, animation, terrain and world-export files are absent
from this package. Local-only copying does not establish Funcom's permission;
the license/provenance report records the unresolved terms separately.

Read PLAYTEST-22.txt and REPORT.txt before an F: test. This candidate has not
been installed into F: or launched in the game by the integration task.
''',encoding='utf-8')
        for name in ('REPORT.txt','PLAYTEST-22.txt'):
            if (ROOT.parent/name).exists():shutil.copy2(ROOT.parent/name,target/name)
        m=dict(old)
        m.update(version='0.6.0-preview.22',build='ao-vk-live-0.6.0-preview.22-local-candidate',
                 candidateStatus='combined-live-validation-pending',materialSourceVersion='0.6.0-preview.21',
                 addon={n:sha(target/n) for n in ('ao-vk.dll','ddraw.dll')},
                 previousAddons=[old['addon'],*old.get('previousAddons',[])])
        m.pop('excludedFeatures',None)
        m.pop('sourceManifestSha256',None)
        m['sourceCommit']=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
        m['integrationSources']=dict(preview21='85862ddf596646295e2bea9bd3045e879f835d60',renderer_origin='1887c4704b1293d91d12458130306cb9f40f908e',living='9b8a42fcb2259973387ba556d7810506c3c40aad',candidate=m['sourceCommit'])
        if (ROOT.parent/'SOURCE-AUDIT.json').is_file():shutil.copy2(ROOT.parent/'SOURCE-AUDIT.json',target/'SOURCE-AUDIT.json')
        m['livingAssets']=dict(provider='installation-local-v1',status='local-test-candidate',
            helperSha256=sha(target/'living-preparer/ao-vk-living-prepare.exe'),
            files={p.relative_to(target).as_posix():sha(p) for folder in ('living-preparer','living-metadata') for p in sorted((target/folder).rglob('*')) if p.is_file()},
            publicAssetBytes=0,originalFilesModified=False,liveVerified=False)
        m['cacheWarmup']=dict(helper='ao-vk-cache-warm.exe',sha256=sha(target/'ao-vk-cache-warm.exe'),relativeCache='randy-vk-preview22-v9-living.cache')
        m['payload']={p.relative_to(target).as_posix():sha(p) for p in sorted(target.rglob('*')) if p.is_file()}
        m['payload'].update(m['assets'])
        # Route metadata is authored, not an original terrain export. Reject any
        # original/converted resource file even if someone puts one in metadata.
        for path in target.rglob('*'):
            if not path.is_file():continue
            relative=path.relative_to(target).as_posix()
            if path.suffix.lower() in {'.lrm','.lrg','.tree','.npz','.idx','.dat','.dmp','.cache'}:
                raise ValueError('Prohibited original/local resource in package: '+relative)
            with path.open('rb') as stream:
                if stream.read(4) in (b'LRM2',b'LRM3',b'LRG1',b'RVKT'):raise ValueError('Prohibited resource signature: '+relative)
        (target/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
        archive=out/(target.name+('-setup.zip' if edition=='full' else '.zip'))
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for path in sorted(target.rglob('*')):
                if path.is_file():z.write(path,target.name+'/'+path.relative_to(target).as_posix())
        with zipfile.ZipFile(archive) as z:
            if z.testzip():raise ValueError('Archive integrity failure')
            for name,expected in m['payload'].items():
                if name not in omitted and hashlib.sha256(z.read(target.name+'/'+name)).hexdigest()!=expected:raise ValueError('Archive payload mismatch')
        results.append(dict(edition=edition,path=str(archive),bytes=archive.stat().st_size,sha256=sha(archive),assets=len(m['assets']),private_asset_files=0,payload_hashes_verified=True))
    (out/'PACKAGE-AUDIT.json').write_text(json.dumps(dict(packages=results,material_companions=companions,frozen_material_release='0.6.0-preview.21',published=False),indent=2))
    (out/'SHA256SUMS.txt').write_text(''.join(x['sha256']+'  '+Path(x['path']).name+'\n' for x in results+companions))
    print(json.dumps(results,indent=2))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('baseline','build','helper','output'):p.add_argument('--'+name,required=True,type=Path)
    a=p.parse_args();package(a.baseline,a.build,a.helper,a.output)


if __name__=='__main__':main()
