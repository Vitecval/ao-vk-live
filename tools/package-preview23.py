"""Assemble private Preview 23 test packages from frozen Candidate 9 setup ZIPs.

Does not publish, install, fetch artwork, or duplicate the three large material
archives. Public redistribution is intentionally not approved for the new art.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[1]
BASE='8225354f4b779cc8b07fc56a72ae3eb8b8da9670'
VERSION='0.6.0-preview.23'

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    assert subprocess.call(['git','diff','--quiet','HEAD'],cwd=ROOT)==0,'Commit source changes before packaging'
    assert subprocess.call(['git','merge-base','--is-ancestor',BASE,commit],cwd=ROOT)==0
    build=a.build.resolve();baseline=a.baseline.resolve();rows=[]
    skip={'REPORT.txt','SOURCE-AUDIT.json','REGRESSION-RESULTS.json','FINAL-VALIDATION.json','PLAYTEST-22.txt','PACKAGE-AUDIT.json','SHA256SUMS.txt','ALL-OFF-PARITY.json','living-preparer','living-metadata','prepare-living.ps1','Prepare Living City Test.cmd','Prepare Newland Test.cmd'}
    for edition,suffix in [('lite','lite.zip'),('full','full-setup.zip')]:
        seed=baseline/f'ao-vk-live-0.6.0-preview.22-windows-x86-{suffix}'
        folder=out/f'ao-vk-live-{VERSION}-windows-x86-{edition}';folder.mkdir()
        with zipfile.ZipFile(seed) as z:
            for item in z.infolist():
                rel=Path(*Path(item.filename).parts[1:])
                if not rel.parts or rel.parts[0] in skip or item.is_dir() or 'living' in rel.name.lower():continue
                assert not rel.is_absolute() and '..' not in rel.parts
                dest=folder/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(item))
        manifest=json.loads((folder/'manifest.json').read_text())
        manifest.pop('livingAssets',None)
        previous=manifest['addon'].copy()
        manifest['previousAddons'].insert(0,previous)
        for name in ['ao-vk.dll','ddraw.dll','ao-vk-cache-warm.exe']:shutil.copy2(build/name,folder/name)
        for name in ['install-live.ps1','setup-live.ps1','setup-progress.ps1','material-download.ps1','material-parts.ps1','large-address-aware.ps1','mesh-index.ps1','startup-report.ps1']:
            shutil.copy2(ROOT/'tools'/name,folder/name)
        for name in ['preview23.md','settings.md']:shutil.copy2(ROOT/'docs'/name,folder/'docs'/name)
        shutil.copy2(ROOT/'REPORT.md',folder/'REPORT.md')
        text=(ROOT/'docs/preview23.md').read_text(encoding='utf-8')
        (folder/'README.md').write_text(text,encoding='utf-8')
        (folder/'RELEASE-NOTES.md').write_text(text,encoding='utf-8')
        (folder/'PLAYTEST.md').write_text(text,encoding='utf-8')
        pack=ROOT/'material-packs/ao-icc-enhanced-original';dest=folder/'randy-vk/packs/ao-icc-enhanced-original';dest.mkdir(parents=True)
        for name in ['manifest.json','generation-prompts.json','README.md']:shutil.copy2(pack/name,dest/name)
        added=0
        for name in ['manifest.json','generation-prompts.json','README.md']:
            manifest['assets'][(dest/name).relative_to(folder).as_posix()]=sha(dest/name)
        for file in sorted((pack/'maps').glob('*.png')):
            shutil.copy2(file,dest/file.name);rel=(dest/file.name).relative_to(folder).as_posix()
            manifest['assets'][rel]=sha(dest/file.name);added+=file.stat().st_size
        manifest.update(version=VERSION,build='ao-vk-preview23-private-stabilization-candidate',sourceCommit=commit,
            setupSourceCommit=commit,baselineCommit=BASE,setupRevision=5,candidateRevision=9,livingIncluded=False,candidateStatus='nvidia-presentation-completion-regression-validated-live-retest-pending-and-art-rights-pending',
            publicDistributionApproved=False,originalGameAssetsIncluded=False,
            iccArtwork=dict(status='private-generated-reference-derived-art-rights-review-pending',playfield=655,targets=['1010004:295610','1010006:21','1010006:22']))
        manifest['addon']={name:sha(folder/name) for name in ['ao-vk.dll','ddraw.dll']}
        manifest['cacheWarmup']=dict(helper='ao-vk-cache-warm.exe',sha256=sha(folder/'ao-vk-cache-warm.exe'),relativeCache='auto')
        manifest['integrationSources']['preview23']=commit
        if 'installBytes' in manifest:manifest['installBytes']+=added
        manifest['payload']={file.relative_to(folder).as_posix():sha(file) for file in sorted(folder.rglob('*'))
            if file.is_file() and file.relative_to(folder).as_posix()!='manifest.json' and file.relative_to(folder).as_posix() not in manifest['assets']}
        # Pack manifests are payload, not material source pixels. Preserve every
        # earlier asset digest and all three exact archive/download descriptors.
        for rel,digest in manifest['assets'].items():
            file=folder/rel
            if file.is_file():assert sha(file)==digest,rel
        (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        zipname=out/f'ao-vk-live-{VERSION}-windows-x86-{edition+"-setup" if edition=="full" else edition}.zip'
        with zipfile.ZipFile(zipname,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for file in sorted(folder.rglob('*')):
                if file.is_file():
                    assert file.suffix.lower() not in ['.cache','.bmp'],file
                    z.write(file,file.relative_to(out).as_posix())
        rows.append(dict(edition=edition,path=str(zipname),sha256=sha(zipname),bytes=zipname.stat().st_size,
            manifest_sha256=sha(folder/'manifest.json'),sourceCommit=commit,seed_sha256=sha(seed)))
    (out/'PACKAGE-AUDIT.json').write_text(json.dumps(dict(result='passed',sourceCommit=commit,baselineCommit=BASE,
        scope='Assembly/hash audit only. Candidate packages have not been installed into a real AO client.',
        large_material_archives_reused_from=str(baseline),packages=rows),indent=2)+'\n')
    (out/'SHA256SUMS.txt').write_text(''.join(f'{x["sha256"]}  {Path(x["path"]).name}\n' for x in rows))
    print(json.dumps(rows,indent=2))

if __name__=='__main__':main()
