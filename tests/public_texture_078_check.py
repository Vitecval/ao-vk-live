"""Verify actual public078 archives and execute offline32-bit companion expansion.

No renderer/game/GPU/publishing. Writes an isolated physicalF test package only
when --expand is explicitly supplied. Legacy28GB companions are not downloaded.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,os,re,struct,subprocess,traceback,uuid,zipfile

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--expand',action='store_true')
p.add_argument('--output',type=Path,default=ROOT/'build/horizon-0.7.8/actual-texture-check')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
sha=digest
read=lambda path:json.loads(Path(path).read_text(encoding='utf-8-sig'))
def safe_relative(name):
    p=PurePosixPath(name)
    assert not p.is_absolute() and '\\' not in name and ':' not in name and '..' not in p.parts and str(p)==name,name
    return p
def zip_manifest(path):
    with zipfile.ZipFile(path) as z:
        roots=[n for n in z.namelist() if n.count('/')==1 and n.endswith('/manifest.json')];assert len(roots)==1
        return json.loads(z.read(roots[0])),roots[0].split('/')[0],hashlib.sha256(z.read(roots[0])).hexdigest()

report=dict(result='running',scope='Actual5456newPNG+12metadata ZIP hashes,Full/Lite membership, actual offline32-bit newcompanion extraction.',
    gpuExecuted=False,liveClientChanged=False,published=False,legacyArchiveBytesRehashed=False)
def save():(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
save()
try:
    work=ROOT/'build/horizon-0.7.8';plan_path=work/'texture-audit/FULL-NEW-TEXTURE-FILES.json';plan=read(plan_path)
    assert plan['result']=='ready' and plan['readyIdentities']==1364 and plan['runtimeMapFiles']==5456
    pins={r['target']:r for r in plan['files']};assert len(pins)==5468
    maps={k:v for k,v in pins.items() if v['role']=='runtime-map'};metadata=set(pins)-set(maps)
    assert len(maps)==5456 and len(metadata)==12
    bootstrap=read(work/'PUBLIC-BOOTSTRAP.json');parts_report=read(work/'NEW-COMPANIONS.json')
    assert bootstrap['result']==parts_report['result']=='passed'
    assert bootstrap['texturePlanSha256']==parts_report['texturePlanSha256']==sha(plan_path)
    paths={row['edition']:Path(row['archive']) for row in bootstrap['archives']}
    full,top,full_manifest_sha=zip_manifest(paths['full-setup']);lite,lite_top,lite_manifest_sha=zip_manifest(paths['lite'])
    assert full['candidateRevision']==lite['candidateRevision']==72
    assert full['publicDistributionApproved'] is True and lite['publicDistributionApproved'] is True
    assert full['edition']=='full' and lite['edition']=='lite'
    for row in bootstrap['archives']:
        assert sha(Path(row['archive']))==row['sha256'] and Path(row['archive']).stat().st_size==row['bytes']
    assert paths['full-setup'].stat().st_size<512*2**20
    baseline_path=ROOT/'build/horizon-0.7.7/publish/ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip'
    baseline,_,baseline_manifest_sha=zip_manifest(baseline_path)
    assert len(full['materialParts'])==21 and len(baseline['materialParts'])==19
    assert full['materialParts'][:19]==baseline['materialParts'],'Legacy19 descriptor bytes/values changed'
    assert all(full['assets'][path]==digest for path,digest in baseline['assets'].items()),'Legacy texture/cache payload changed'
    new=full['materialParts'][19:];assert new==parts_report['parts']==bootstrap['newParts'] and len(new)==2
    all_members=[]
    for part in full['materialParts']:
        all_members+=part['files']
    assert len(all_members)==len(set(all_members)),'Duplicate companion members'
    assert set(path for part in new for path in part['files'])==set(maps)
    for path,row in pins.items():assert full['assets'][path]==full['payload'][path]==row['sha256'],path
    for key in ['materialParts','materialDownload','preparedMaterials']:assert not lite.get(key),('Lite downloads textures',key)
    assert lite['optionalTexturePack'] is False
    assert not any('/ao-penumbra-078/' in path or '/ao-reworked-characters/' in path for path in [*lite['assets'],*lite['payload']])
    held=set(plan['heldIdentitiesExcluded']);ready=set()
    for path in maps:
        m=re.fullmatch(r'randy-vk/packs/(ao-penumbra-078|ao-reworked-characters)/(\d+)_(\d+)_(d|n|orm|e)\.png',path);assert m,path
        ready.add(m[2]+':'+m[3])
    assert len(ready)==1364 and not ready.intersection(held)
    archives=[];downloads=(work/'downloads').resolve()
    assert downloads==Path('F:/AO-VK-D2/horizon-0.7.8/downloads').resolve()
    for part in new:
        assert part['sourceVersion']=='0.7.8' and part['baseUrl']=='https://github.com/Vitecval/ao-vk-live/releases/download/v0.7.8'
        assert re.fullmatch(r'ao-vk-live-0\.7\.8-materials-(environment|characters)-001\.zip',part['name'])
        assert not part.get('notices'),'New public PNG-only archive contains unexpected notices'
        path=downloads/part['name'];assert path.is_file() and path.stat().st_size==part['bytes']<2**31
        assert sha(path)==part['sha256'];count=0;unpacked=0
        with zipfile.ZipFile(path) as z:
            names=z.namelist();assert len(names)==len(set(names)) and set(names)==set(part['files'])
            for entry in z.infolist():
                safe_relative(entry.filename);assert not entry.is_dir();expected=pins[entry.filename]
                assert entry.file_size==expected['bytes']
                with z.open(entry) as f:assert hashlib.file_digest(f,'sha256').hexdigest()==expected['sha256'],entry.filename
                count+=1;unpacked+=entry.file_size
        assert unpacked==part['unpackedBytes']
        archives.append(dict(name=part['name'],sha256=part['sha256'],bytes=part['bytes'],pngMembers=count,decodedBytes=unpacked,
            everyMemberSha256Verified=True,strictNamespace=True))
        print('Verified actual companion',part['name'],count,flush=True)
    with zipfile.ZipFile(paths['full-setup']) as z:
        entries={entry.filename:entry for entry in z.infolist()};assert len(entries)==len(z.infolist())
        for name,entry in entries.items():
            assert name.startswith(top+'/');relative=name[len(top)+1:];safe_relative(relative)
            if relative=='manifest.json':continue
            assert not entry.is_dir() and relative in full['payload']
            with z.open(entry) as f:assert hashlib.file_digest(f,'sha256').hexdigest()==full['payload'][relative],relative
        physically_present={name[len(top)+1:] for name in entries if name!=top+'/manifest.json'}
        assert physically_present==set(full['payload'])-set(all_members)
        assert not physically_present.intersection(maps),'Small bootstrap embeds new runtime maps'
        assert metadata<=physically_present
        for path in metadata:assert hashlib.sha256(z.read(top+'/'+path)).hexdigest()==pins[path]['sha256']
        assert hashlib.sha256(z.read(top+'/LICENSE-TEXTURES.md')).hexdigest()==full['artworkLicense']['sha256']
        for name in ['ao-penumbra-078','ao-reworked-characters']:
            manifest=json.loads(z.read(top+'/randy-vk/packs/'+name+'/manifest.json'))
            assert len(manifest['materials'])==(879 if name=='ao-penumbra-078' else 485)
            assert len(manifest['files'])==(3516 if name=='ao-penumbra-078' else 1940)
            assert manifest['publication']['acceptedOnly'] and manifest['publication']['originalReferencesIncluded'] is False
            # Both files retain their own exact pinned SHA. Authoring metadata
            # uses CRLF while the immutable bootstrap license uses LF; require
            # identical license TEXT without inventing a byte-identity claim.
            nested_license=z.read(top+'/randy-vk/packs/'+name+'/LICENSE-TEXTURES.md')
            setup_license=z.read(top+'/LICENSE-TEXTURES.md')
            assert nested_license.replace(b'\r\n',b'\n')==setup_license.replace(b'\r\n',b'\n')
            assert '0.7.8' in z.read(top+'/randy-vk/packs/'+name+'/CREDITS.txt').decode()
    with zipfile.ZipFile(paths['lite']) as z:
        for entry in z.infolist():
            assert entry.filename.startswith(lite_top+'/');relative=entry.filename[len(lite_top)+1:];safe_relative(relative)
            if relative=='manifest.json':continue
            with z.open(entry) as f:assert hashlib.file_digest(f,'sha256').hexdigest()==lite['payload'][relative]
    app=read(work/'candidate/BUILD.json');app_path=Path(app['executable']);assert sha(app_path)==app['sha256']
    with app_path.open('rb') as f:
        f.seek(-56,2);footer=f.read(56);assert footer[:8]==b'RKHCORE1'
        offset,length=struct.unpack('<qq',footer[8:24]);assert offset+length+56==app_path.stat().st_size
        assert length==paths['full-setup'].stat().st_size
        f.seek(offset);hasher=hashlib.sha256();remaining=length
        while remaining:
            data=f.read(min(1<<20,remaining));assert data;hasher.update(data);remaining-=len(data)
        assert hasher.hexdigest()==footer[24:].hex()==sha(paths['full-setup'])==app['engineSha256']
    report.update(result='archives-passed-expansion-pending' if a.expand else 'passed-archive-only',
        texturePlanSha256=sha(plan_path),coreSha256=sha(paths['full-setup']),liteSha256=sha(paths['lite']),
        fullManifestSha256=full_manifest_sha,liteManifestSha256=lite_manifest_sha,baseline077ManifestSha256=baseline_manifest_sha,
        baseline19DescriptorsAndAssetHashesIdentical=True,totalCompanions=21,newArchiveChecks=archives,
        verifiedRuntimePngMembers=5456,verifiedMetadataMembers=12,heldIdentitiesAbsentFromNewPacks=True,
        licenseTextIdentical=True,licenseFilesIndividuallyHashPinned=True,licenseLineEndingsDiffer=True,
        fullAssetsAndPayloadComplete=True,liteNewMaps=0,liteTextureDownloads=False,appSha256=app['sha256'],embeddedBootstrapVerified=True)
    save()
    if a.expand:
        # Physical F path beneath the observed downloads directory means the
        # real search policy reuses real parent ZIPs without copying2.4GB or HTTP.
        physical=downloads/('actual-texture-check-'+uuid.uuid4().hex);package=physical/'package'
        assert physical.parent.resolve()==downloads and not physical.exists()
        assert not any(parent.is_symlink() for parent in [downloads,*downloads.parents])
        package.mkdir(parents=True)
        with zipfile.ZipFile(paths['full-setup']) as z:
            for entry in z.infolist():
                relative=entry.filename[len(top)+1:];safe_relative(relative);target=package/relative
                assert target.resolve().is_relative_to(package.resolve());target.parent.mkdir(parents=True,exist_ok=True)
                with z.open(entry) as src,target.open('xb') as dst:
                    while data:=src.read(1<<20):dst.write(data)
        # Preserve original Full manifest unchanged in package. Only the object
        # passed to Expand narrows the parts list; all real asset hashes remain.
        harness=physical/'expand-actual.ps1'
        harness.write_text("""param([string]$Package,[string]$Output)
$ErrorActionPreference='Stop'
if([IntPtr]::Size -ne 4){throw 'Actual32bit PowerShell is required'}
$m=Get-Content -LiteralPath (Join-Path $Package 'manifest.json') -Raw | ConvertFrom-Json
$fullCount=@($m.materialParts).Count
if($fullCount -ne 21){throw 'Expected real Full21part manifest'}
. (Join-Path $Package 'material-parts.ps1')
$m.materialParts=@($m.materialParts | Where-Object {$_.sourceVersion -ceq '0.7.8'})
if(@($m.materialParts).Count -ne 2){throw 'Expected exactly2actual public parts'}
$required=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
foreach($part in $m.materialParts){foreach($name in $part.files){$null=$required.Add([string]$name)}}
if($required.Count -ne 5456){throw 'Expected all5456newPNGfiles'}
$script:networkCalls=0
function Open-AoMaterialResponse([string]$Url,[long]$Offset){$script:networkCalls++;throw 'Network disabled in offline real-ZIP fixture'}
$messages=New-Object 'Collections.Generic.List[string]'
$progress={param($message,$percent);$messages.Add([string]$message)}
Expand-AoMaterialParts -Package $Package -Manifest $m -Client '' -ProgressCallback $progress -RequiredAssets $required
if($script:networkCalls){throw 'Unexpected download attempt'}
[ordered]@{result='passed';processBits=([IntPtr]::Size*8);fullManifestParts=$fullCount;fixtureParts=2;expectedPngs=$required.Count;
 networkCalls=$script:networkCalls;package=$Package;licenseSha256=(Get-FileHash -LiteralPath (Join-Path $Package 'LICENSE-TEXTURES.md') -Algorithm SHA256).Hash.ToLowerInvariant();
 manifestFileUnmodified=$true;scope='Actual public ZIP offline extraction only; no legacy archive download or live install';messages=@($messages)} |
 ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Output -Encoding UTF8
""")
        ps=Path(os.environ.get('WINDIR','C:/Windows'))/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
        command=[str(ps),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(harness),'-Package',str(package),'-Output',str(physical/'EXPANSION.json')]
        with (out/'actual-expand.log').open('w') as log:
            run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
        assert run.returncode==0,('Actual companion expansion failed',run.returncode,str(out/'actual-expand.log'))
        expansion=read(physical/'EXPANSION.json');assert expansion['result']=='passed' and expansion['processBits']==32 and expansion['networkCalls']==0
        assert sha(package/'manifest.json')==full_manifest_sha,'Fixture modified Full manifest file'
        for path,row in pins.items():assert sha(package/path)==row['sha256'],('Actual extracted file differs',path)
        assert {str(f.relative_to(package)).replace('\\','/') for name in ['ao-penumbra-078','ao-reworked-characters']
            for f in (package/'randy-vk/packs'/name).glob('*.png')}==set(maps)
        report.update(result='passed',actualExpansion=expansion,actualExpandedPngsVerified=5456,
            actualExpandedMetadataVerified=12,physicalFixture=str(physical),expansionReportSha256=sha(physical/'EXPANSION.json'),
            powershellCommand=command,narrowedFixture=True,
            scopeLimit='Actual new2companion expansion verified against real Full manifest assets;19legacy archives descriptor/hash unchanged but not downloaded/extracted; no live client installation, GPU, or remote HTTP tested.')
        print('Actual32bit expansion and all extracted hashes passed',flush=True)
except Exception as error:
    report.update(result='failed',error=str(error),traceback=traceback.format_exc());raise
finally:save()
