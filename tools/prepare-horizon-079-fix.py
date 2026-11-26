"""Prepare a local 0.7.9 installer fix; never publish or alter old artifacts."""
from pathlib import Path
import argparse, copy, gzip, hashlib, json, shutil, struct, zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/horizon-0.7.9'
ASSETS = ROOT / 'apps/horizon/Assets'
OLD = ROOT / 'build/preview24/release-r72'
NEW = ROOT / 'build/preview24/release-r73'
RENDERER = '6687e6b62778780f82c220d5c9f4d81af5a3d9e97866f0fe84877032e60a8c29'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def encoded(value):
    return (json.dumps(value, indent=2) + '\n').encode('utf-8')

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def write(path, value):
    path.write_bytes(encoded(value))

def prepare():
    assert WORK.resolve() == Path('F:/AO-VK-D2/horizon-0.7.9')
    assert NEW.resolve() == WORK.resolve() / 'core-r73'
    before_files = ['SetupService.cs', 'UpdateService.cs', 'App.xaml.cs',
                    'Assets/engine.json', 'Assets/settings.json', 'Assets/graphics-versions.json',
                    'Assets/patch-notes.txt', 'Assets/lite-manifest.json.gz',
                    'Assets/full-manifest.json.gz', 'Assets/core-files.json',
                    'Assets/temporary-assets.txt.gz']
    before = WORK / 'source-before'
    before.mkdir()
    source_hashes = {}
    for name in before_files:
        source = ROOT / 'apps/horizon' / name
        dest = before / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
        source_hashes[name] = sha(source)
    old_engine = read(ASSETS / 'engine.json')
    assert old_engine['revision'] == 72 and old_engine['version'] == '0.7.8'
    assert old_engine['rendererSha256'] == RENDERER
    safety = (ROOT / 'tools/setup-safety.ps1').read_bytes()
    safety_sha = hashlib.sha256(safety).hexdigest()
    assert safety_sha == '407262e6a4c310b9d7ba5e21feb91f4d08cccbc9ceb7646e621968088394259c'
    archives = []
    for edition in ('full-setup', 'lite'):
        stem = 'ao-vk-live-0.6.0-preview.24-windows-x86-' + edition
        source = OLD / (stem + '.zip')
        target = NEW / (stem + '.zip')
        assert not target.exists()
        source_sha = sha(source)
        with zipfile.ZipFile(source) as original:
            manifest_name = next(n for n in original.namelist() if n.count('/') == 1 and n.endswith('/manifest.json'))
            top = manifest_name.split('/')[0]
            old = json.loads(original.read(manifest_name))
            current = copy.deepcopy(old)
            assert old['candidateRevision'] == 72 and old['releaseVersion'] == '0.7.8'
            assert old['addon']['ao-vk.dll'] == RENDERER
            if edition == 'full-setup':
                assert len(old['materialParts']) == 21
                assert source_sha == old_engine['fullSetupSha256']
            current['payload']['setup-safety.ps1'] = safety_sha
            current.update(candidateRevision=73, releaseVersion='0.7.9',
                           candidateStatus='local-release-candidate',
                           publicDistributionApproved=False, privateTest=False,
                           localEnvironmentTrial=False)
            assert current.get('materialParts') == old.get('materialParts')
            assert current['sourceCommit'] == old['sourceCommit'] == old_engine['sourceCommit']
            copied = {}
            with zipfile.ZipFile(target, 'w', allowZip64=True) as output:
                for info in original.infolist():
                    if info.filename == manifest_name:
                        body = encoded(current)
                    elif info.filename == top + '/setup-safety.ps1':
                        body = safety
                    else:
                        body = original.read(info)
                        copied[info.filename] = hashlib.sha256(body).hexdigest()
                    output.writestr(info, body)
        with zipfile.ZipFile(target) as result:
            assert result.testzip() is None
            assert all(hashlib.sha256(result.read(n)).hexdigest() == h for n, h in copied.items())
            assert json.loads(result.read(manifest_name)) == current
            assert hashlib.sha256(result.read(top + '/ao-vk.dll')).hexdigest() == RENDERER
        assert sha(source) == source_sha
        folder = NEW / stem
        folder.mkdir()
        write(folder / 'manifest.json', current)
        archives.append(dict(edition=edition, archive=str(target.resolve()),
                             sha256=sha(target), bytes=target.stat().st_size,
                             source=str(source.resolve()), sourceSha256=source_sha,
                             unchangedEntryCount=len(copied),
                             changedZipEntries=[manifest_name, top + '/setup-safety.ps1'],
                             allOtherEntriesByteIdentical=True,
                             companionDescriptorsUnchanged=True))
    full = archives[0]
    engine = dict(old_engine, revision=73, version='0.7.9',
                  fullSetupBytes=full['bytes'], fullSetupSha256=full['sha256'], privateTest=False)
    write(ASSETS / 'engine.json', engine)
    settings = read(ASSETS / 'settings.json')
    settings['rendererRevision'] = 73
    write(ASSETS / 'settings.json', settings)
    versions = read(ASSETS / 'graphics-versions.json')
    versions[RENDERER] = 'r73 (0.7.9)'
    write(ASSETS / 'graphics-versions.json', versions)
    for name, old, new in [('SetupService.cs', 'Version="0.7.8"', 'Version="0.7.9"'),
                           ('UpdateService.cs', 'VersionTag="v0.7.8"', 'VersionTag="v0.7.9"'),
                           ('App.xaml.cs', 'AssemblyFileVersion("0.7.8.0")', 'AssemblyFileVersion("0.7.9.0")')]:
        path = ROOT / 'apps/horizon' / name
        text = path.read_text(encoding='utf-8')
        assert text.count(old) == 1
        path.write_text(text.replace(old, new), encoding='utf-8')
    notes = 'Rubi-Ka Horizon 0.7.9\n\nFixes installing Full graphics with the new texture collection.\n\n'
    old_notes = (ASSETS / 'patch-notes.txt').read_text(encoding='utf-8-sig')
    assert old_notes.startswith('Rubi-Ka Horizon 0.7.8')
    (ASSETS / 'patch-notes.txt').write_text(notes + old_notes, encoding='utf-8')
    (WORK / 'RELEASE-NOTES.txt').write_text(notes, encoding='utf-8')
    write(WORK / 'LOCAL-BOOTSTRAP.json', dict(result='passed', version='0.7.9',
          engine=engine, archives=archives, setupSafetySha256=safety_sha,
          sourceBeforeSha256=source_hashes, rendererUnchanged=True,
          all21CompanionDescriptorsUnchanged=True, publicationApproved=False,
          published=False, retailFilesChanged=False))
    print(json.dumps(engine, indent=2), flush=True)

def approve():
    """Archive the tested local candidate, then apply explicit release approval."""
    assert WORK.resolve() == Path('F:/AO-VK-D2/horizon-0.7.9')
    record = read(WORK / 'LOCAL-BOOTSTRAP.json')
    assert record['publicationApproved'] is False
    archive = WORK / 'preapproval'
    archive.mkdir()
    for name in ('app', 'candidate'):
        shutil.copytree(WORK / name, archive / name)
    for name in ('LOCAL-BOOTSTRAP.json', 'READY.json', 'APP-SOURCE-FILES.json'):
        shutil.copyfile(WORK / name, archive / name)
    prior = []
    for row in record['archives']:
        current_path = Path(row['archive'])
        assert current_path.parent == NEW.resolve()
        assert sha(current_path) == row['sha256']
        staged = current_path.with_suffix('.approved-staging.zip')
        assert not staged.exists()
        with zipfile.ZipFile(current_path) as source:
            manifest_name = next(n for n in source.namelist() if n.count('/') == 1 and n.endswith('/manifest.json'))
            original = json.loads(source.read(manifest_name))
            assert original['candidateRevision'] == 73 and original['releaseVersion'] == '0.7.9'
            assert original['publicDistributionApproved'] is False
            approved = copy.deepcopy(original)
            approved.update(candidateStatus='stable', publicDistributionApproved=True,
                            localEnvironmentTrial=False, privateTest=False)
            assert approved.get('materialParts') == original.get('materialParts')
            other_hashes = {}
            with zipfile.ZipFile(staged, 'w', allowZip64=True) as destination:
                for info in source.infolist():
                    body = encoded(approved) if info.filename == manifest_name else source.read(info)
                    if info.filename != manifest_name:
                        other_hashes[info.filename] = hashlib.sha256(body).hexdigest()
                    destination.writestr(info, body)
        with zipfile.ZipFile(staged) as destination:
            assert destination.testzip() is None
            assert json.loads(destination.read(manifest_name)) == approved
            assert all(hashlib.sha256(destination.read(n)).hexdigest() == h for n, h in other_hashes.items())
        archived = archive / 'core-r73' / current_path.name
        archived.parent.mkdir(exist_ok=True)
        current_path.rename(archived)
        staged.rename(current_path)
        write(NEW / manifest_name, approved)
        prior.append(dict(original=str(current_path), archived=str(archived.resolve()),
                          sha256=sha(archived), bytes=archived.stat().st_size))
        row.update(sha256=sha(current_path), bytes=current_path.stat().st_size,
                   preapprovalArchive=str(archived.resolve()),
                   approvalChangedZipEntry=manifest_name,
                   approvalAllOtherEntriesByteIdentical=True)
    engine = read(ASSETS / 'engine.json')
    engine.update(fullSetupSha256=record['archives'][0]['sha256'],
                  fullSetupBytes=record['archives'][0]['bytes'])
    write(ASSETS / 'engine.json', engine)
    record.update(engine=engine, publicationApproved=True, candidateStatus='stable')
    write(WORK / 'LOCAL-BOOTSTRAP.json', record)
    write(archive / 'ARCHIVED-PREAPPROVAL.json', dict(result='passed', archives=prior,
          candidate=read(archive / 'candidate/BUILD.json'),
          originalReadySha256=sha(archive / 'READY.json')))
    print(json.dumps(engine, indent=2), flush=True)

def package():
    engine = read(ASSETS / 'engine.json')
    assert engine['version'] == '0.7.9' and engine['revision'] == 73
    core = NEW / 'ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip'
    assert sha(core) == engine['fullSetupSha256']
    with zipfile.ZipFile(core) as archive:
        manifest = next(n for n in archive.namelist() if n.count('/') == 1 and n.endswith('/manifest.json'))
        assert json.loads(archive.read(manifest)) == json.loads(gzip.decompress((ASSETS / 'full-manifest.json.gz').read_bytes()))
    app = WORK / 'app/Rubi-Ka Horizon.exe'
    assert app.read_bytes()[:2] == b'MZ'
    output = WORK / 'candidate/Rubi-Ka-Horizon-Setup.exe'
    output.parent.mkdir(exist_ok=True)
    if output.exists():
        archived = WORK / 'preapproval/candidate/Rubi-Ka-Horizon-Setup.exe'
        assert archived.is_file() and sha(output) == sha(archived)
        output.unlink()
    with output.open('xb') as destination, app.open('rb') as source, core.open('rb') as payload:
        shutil.copyfileobj(source, destination)
        offset = destination.tell()
        shutil.copyfileobj(payload, destination)
        destination.write(b'RKHCORE1' + struct.pack('<qq', offset, core.stat().st_size) + bytes.fromhex(sha(core)))
    assert output.stat().st_size < 512 * 2**20
    approval = read(WORK / 'LOCAL-BOOTSTRAP.json')['publicationApproved']
    report = dict(product='Rubi-Ka Horizon', version='0.7.9', state='release-candidate' if approval else 'local-release-candidate',
                  published=False, publicationApproved=approval, executable=str(output.resolve()),
                  sha256=sha(output), bytes=output.stat().st_size, engineRevision=73,
                  engineSha256=sha(core), rendererSha256=RENDERER,
                  applicationSha256=sha(app), liteTextureDownloads=False,
                  fullPreparedCacheFiles=14345, all21CompanionDescriptorsUnchanged=True)
    write(output.parent / 'BUILD.json', report)
    print(json.dumps(report, indent=2), flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--approve', action='store_true')
    args = parser.parse_args()
    if args.approve:
        approve()
    elif args.package:
        package()
    else:
        prepare()
