"""Package Preview23's verified expanded artwork as pinned Preview23 companions."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; VERSION='0.6.0-preview.23'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n',encoding='utf8')
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def archive(folder,target):
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(folder.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(folder.parent).as_posix())
def main():
    p=argparse.ArgumentParser();p.add_argument('--seed',type=Path,required=True);p.add_argument('--library',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--revision',type=int,default=15);p.add_argument('--stage-only',action='store_true',help='Prepare complete local installer folders for private testing; no distributable archives')
    args=p.parse_args()
    assert not subprocess.check_output(['git','status','--porcelain'],cwd=ROOT),'Commit package source first'
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    out=args.output.resolve();out.mkdir(exist_ok=False,parents=True);editions={};coverage=read(ROOT/'material-packs/preview23-integration.json')['coverage']
    for edition in ['lite','full']:
        folder=out/f'ao-vk-live-{VERSION}-windows-x86-{edition}'
        def copy_file(src,dst):
            if Path(src).suffix.lower()=='.png':os.link(src,dst)
            else:shutil.copy2(src,dst)
            return dst
        shutil.copytree(args.seed.resolve()/folder.name,folder,copy_function=copy_file)
        m=read(folder/'manifest.json');previous=m['addon'].copy()
        if previous not in m['previousAddons']:m['previousAddons'].insert(0,previous)
        for name in ['ao-vk.dll','ddraw.dll','ao-vk-cache-warm.exe']:shutil.copy2(args.build.resolve()/name,folder/name)
        for name in ['setup-safety.ps1','install-live.ps1','setup-live.ps1','setup-progress.ps1','material-download.ps1','material-parts.ps1','large-address-aware.ps1','mesh-index.ps1','startup-report.ps1']:shutil.copy2(ROOT/'tools'/name,folder/name)
        for name in ['preview23.md','preview23-artwork.md','preview23-retained-originals.csv','settings.md','fsr-stability-audit.md','preview23-installer-audit.md','fsr-performance-audit.md']:shutil.copy2(ROOT/'docs'/name,folder/'docs'/name)
        for name in ['README.md','RELEASE-NOTES.md','PLAYTEST.md']:shutil.copy2(ROOT/'docs/preview23.md',folder/name)
        shutil.copy2(ROOT/'REPORT.md',folder/'REPORT.md')
        if edition=='full':
            # Replace the deferred old library inventory with exactly the staged
            # complete catalogues; preserve non-map metadata shipped by the seed.
            m['assets']={k:v for k,v in m['assets'].items() if not(k.startswith('randy-vk/packs/') and (k.endswith('.png') or k.endswith('/manifest.json')))}
            for manifest in sorted(args.library.resolve().glob('randy-vk/packs/*/manifest.json')):
                catalog=read(manifest);pack=manifest.parent.name;dest=folder/'randy-vk/packs'/pack;dest.mkdir(parents=True,exist_ok=True)
                shutil.copy2(manifest,dest/'manifest.json');m['assets'][(dest/'manifest.json').relative_to(folder).as_posix()]=sha(dest/'manifest.json')
                for name,expected in catalog['files'].items():
                    src=manifest.parent/name;assert sha(src)==expected,(pack,name)
                    target=dest/name
                    if target.exists():
                        if sha(target)!=expected:
                            # Replace the new folder's link, never modify a
                            # shared file in the immutable seed release.
                            temp=target.with_suffix('.staged.png');os.link(src,temp);os.replace(temp,target)
                    else:os.link(src,target)
                    m['assets'][target.relative_to(folder).as_posix()]=expected
            m.pop('materialParts',None);m.pop('materialDownload',None);m['materialSourceVersion']=VERSION
            m['artworkCoverage']=coverage
        m.update(version=VERSION,build='ao-vk-preview23-expanded-artwork',candidateRevision=args.revision,setupRevision=7,
                 sourceCommit=commit,setupSourceCommit=commit,livingIncluded=False,
                 candidateStatus='expanded-artwork-integrated-release-validation',publicDistributionApproved=False)
        m['addon']={name:sha(folder/name) for name in ['ao-vk.dll','ddraw.dll']}
        m['cacheWarmup']['sha256']=sha(folder/'ao-vk-cache-warm.exe')
        m['integrationSources']['preview23']=commit
        m['payload']={x.relative_to(folder).as_posix():sha(x) for x in sorted(folder.rglob('*')) if x.is_file() and x.name!='manifest.json' and x.relative_to(folder).as_posix() not in m['assets']}
        # Nested pack manifests are assets; only the root manifest is unpinned.
        for rel,h in m['assets'].items():assert (folder/rel).is_file() and sha(folder/rel)==h,rel
        write(folder/'manifest.json',m);editions[edition]=folder
    if args.stage_only:
        write(out/'STAGED-INSTALLERS.json',dict(result='passed',candidateRevision=args.revision,sourceCommit=commit,
             distributableArchives=False,publicDistributionApproved=False,folders={k:str(v) for k,v in editions.items()},coverage=coverage))
        print('Complete local installer folders prepared; distributable archives not built',flush=True)
        return
    lite=editions['lite'];litezip=out/(lite.name+'.zip');archive(lite,litezip)
    spec=importlib.util.spec_from_file_location('parts',ROOT/'tools/material-archives.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    print('Building immutable Preview23 material companions',flush=True)
    products=module.build_full_setup(editions['full'],VERSION,essential=read(lite/'manifest.json')['assets'])
    records=[]
    for file in [litezip,*products]:
        print('VERIFY',file.name,flush=True)
        with zipfile.ZipFile(file) as z:assert z.testzip() is None,file.name
        records.append(dict(name=file.name,path=str(file),bytes=file.stat().st_size,sha256=sha(file)))
    report=dict(result='passed',version=VERSION,candidateRevision=args.revision,setupRevision=7,sourceCommit=commit,
                rendererSha256=sha(args.build/'ao-vk.dll'),coverage=coverage,products=records,livingIncluded=False,published=False,
                scope='Archive integrity, complete pinned material membership and hashes. Retail all-zone model-fit playtests remain separate.')
    write(out/'PACKAGE-AUDIT.json',report)
    (out/'SHA256SUMS.txt').write_text(''.join(f"{x['sha256']}  {x['name']}\n" for x in records))
    print(json.dumps(dict(result='passed',products=len(records),coverage=coverage),indent=2),flush=True)
if __name__=='__main__':main()
