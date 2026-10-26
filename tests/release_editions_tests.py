"""Validate delivered archives, not just packaging decisions."""
import hashlib,json,re,sys,zipfile
from pathlib import Path

root=Path(sys.argv[1]);version=root.name;prefix=f'ao-vk-live-{version}'
manifests={};sizes={}
for edition in ['full','lite']:
    archive=root/f'{prefix}-windows-x86-{edition}.zip'
    with zipfile.ZipFile(archive) as z:
        top=f'{prefix}-windows-x86-{edition}/'
        manifest=json.loads(z.read(top+'manifest.json'));manifests[edition]=manifest
        assert manifest['edition']==edition
        for name,wanted in dict(manifest['assets'],**manifest['addon']).items():
            assert hashlib.sha256(z.read(top+name)).hexdigest()==wanted,(edition,name)
        for name in ['Install.cmd','Update.cmd','Uninstall.cmd','install-live.ps1','setup-live.ps1','LICENSE','THIRD-PARTY-NOTICES.md']:
            assert top+name in z.namelist(),(edition,name)
        maps=[n for n in manifest['assets'] if n.endswith('.png') and '/source-art/' not in n]
        atlases=[n for n in manifest['assets'] if '/source-art/' in n]
        if edition=='lite':
            assert not atlases
            assert len(maps)==24,len(maps)
            assert all(re.search(r'/1010006_(17|20|21|22|23|24|25|26)_(d|n|orm)\.png$',n) for n in maps)
        else:
            assert len(atlases)>=179
            assert any('/1010004_297326_d.png' in n for n in maps),'Premium ICC panel missing'
        sizes[edition]={'bytes':archive.stat().st_size,'runtime_png_maps':len(maps),'authoring_atlases':len(atlases)}
assert manifests['full']['addon']==manifests['lite']['addon'],'Edition DLLs differ'
for name,digest in manifests['lite']['assets'].items():
    if name.endswith('.png'):assert manifests['full']['assets'][name]==digest,('Core ground mismatch',name)
for line in (root/'SHA256SUMS.txt').read_text().splitlines():
    wanted,name=line.split('  ',1);h=hashlib.sha256()
    with (root/name).open('rb') as f:
        for chunk in iter(lambda:f.read(8<<20),b''):h.update(chunk)
    assert h.hexdigest()==wanted,name
    sizes.setdefault(name,{'bytes':(root/name).stat().st_size})
with zipfile.ZipFile(root/f'{prefix}-source.zip') as z:
    assert not any('/private/' in n or n.endswith(('.dll','.exe','.ini','.log')) for n in z.namelist())
print(json.dumps({'archive_validation':'passed','editions':sizes},indent=2))
