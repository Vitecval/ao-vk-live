"""Validate delivered archives, not just packaging decisions."""
import hashlib,json,re,sys,zipfile
from pathlib import Path

root=Path(sys.argv[1]);version=root.name;prefix=f'ao-vk-live-{version}'
full_only='--full-only' in sys.argv[2:]
no_source=full_only or '--no-source' in sys.argv[2:]
manifests={};sizes={}
for edition in (['full'] if full_only else ['full','lite']):
    archive=root/f'{prefix}-windows-x86-{edition}.zip'
    with zipfile.ZipFile(archive) as z:
        top=f'{prefix}-windows-x86-{edition}/'
        manifest=json.loads(z.read(top+'manifest.json'));manifests[edition]=manifest
        assert manifest['edition']==edition
        assert manifest['version']==version and manifest['build']==prefix
        assert z.testzip() is None,('ZIP integrity',edition)
        assert not any('/private/' in n or n.endswith(('randy-vk.ini','.log','.dmp','.pdb','.tmp','.bak')) for n in z.namelist())
        for name,wanted in dict(manifest['assets'],**manifest['addon']).items():
            assert hashlib.sha256(z.read(top+name)).hexdigest()==wanted,(edition,name)
        for name in ['Install.cmd','Update.cmd','Uninstall.cmd','install-live.ps1','setup-live.ps1','LICENSE','THIRD-PARTY-NOTICES.md']:
            assert top+name in z.namelist(),(edition,name)
        for action in ['Install','Update','Uninstall']:
            launcher=z.read(top+action+'.cmd').decode('ascii')
            assert '%SystemRoot%\\SysWOW64\\WindowsPowerShell\\v1.0\\powershell.exe' in launcher,(edition,action,'WOW64 host missing')
            assert '"%AO_VK_POWERSHELL%"' in launcher and f'-Action {action}' in launcher
        maps=[n for n in manifest['assets'] if n.endswith('.png') and '/source-art/' not in n]
        atlases=[n for n in manifest['assets'] if '/source-art/' in n]
        if edition=='lite':
            assert not atlases
            family=json.loads((Path(__file__).resolve().parents[1]/'material-packs/ao-cyber-icc/deck-family.json').read_text())
            deck_ids={x['id'] for x in family['members']}
            assert all(re.search(r'/1010006_\d+_(d|n|orm)\.png$',n) for n in maps)
            for identity in deck_ids:
                selected=[n for n in maps if f'/1010006_{identity}_' in n]
                assert len(selected)==1 and selected[0].endswith('_orm.png'),(identity,selected)
            assert not any(f'/1010006_{identity}_{suffix}.png' in n for identity in deck_ids for suffix in ['d','n','h'] for n in maps)
        else:
            assert len(atlases)>=179
            assert any('/1010004_297326_d.png' in n for n in maps),'Premium ICC panel missing'
        sizes[edition]={'bytes':archive.stat().st_size,'runtime_png_maps':len(maps),'authoring_atlases':len(atlases)}
if not full_only:assert manifests['full']['addon']==manifests['lite']['addon'],'Edition DLLs differ'
for name,digest in manifests.get('lite',{}).get('assets',{}).items():
    if name.endswith('.png'):assert manifests['full']['assets'][name]==digest,('Core ground mismatch',name)
for line in (root/'SHA256SUMS.txt').read_text().splitlines():
    wanted,name=line.split('  ',1);h=hashlib.sha256()
    with (root/name).open('rb') as f:
        for chunk in iter(lambda:f.read(8<<20),b''):h.update(chunk)
    assert h.hexdigest()==wanted,name
    sizes.setdefault(name,{'bytes':(root/name).stat().st_size})
if full_only:
    assert not list(root.glob('*-source*')) and not list(root.glob('*-windows-x86-lite*')),'Full-only workflow exported an unwanted edition/source artifact'
    with zipfile.ZipFile(root/f'{prefix}-windows-x86-full.zip') as z:
        assert not any('/private/' in n or n.endswith(('randy-vk.ini','.log','.dmp')) for n in z.namelist())
elif not no_source:
    with zipfile.ZipFile(root/f'{prefix}-source.zip') as z:
        assert not any('/private/' in n or n.endswith(('.dll','.exe','.ini','.log')) for n in z.namelist())
if no_source:assert not list(root.glob('*-source*')),'Unexpected source export/archive'
expected={prefix+'-windows-x86-full.zip'}|({prefix+'-windows-x86-lite.zip'} if not full_only else set())
if no_source:
    assert {line.split('  ',1)[1] for line in (root/'SHA256SUMS.txt').read_text().splitlines()}==expected
print(json.dumps({'archive_validation':'passed','editions':sizes},indent=2))
