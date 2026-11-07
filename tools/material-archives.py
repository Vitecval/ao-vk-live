"""Build pinned, complete Full HD Setup and immutable material companions."""
from pathlib import Path
import collections, hashlib, json, zipfile

def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def build_full_setup(folder,version,essential=(),parts=None,part_limit=1450*2**20):
    folder=Path(folder);out=folder.parent
    manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8-sig'))
    if version not in {'0.6.0-preview.21','0.6.0-preview.23'} or manifest['version']!=version:raise ValueError('Unsupported pinned setup release')
    license_path=folder/'LICENSE-TEXTURES.md'
    notices={'LICENSE-TEXTURES.md':digest(license_path)} if license_path.exists() else {}
    if notices:
        manifest['artworkLicense']=dict(file='LICENSE-TEXTURES.md',sha256=notices['LICENSE-TEXTURES.md'])
    if parts is None:
        groups=collections.defaultdict(list)
        for name in manifest['assets']:
            if name.endswith('.png') and name not in essential:
                p=Path(name);groups[(p.parent.as_posix(),'_'.join(p.stem.split('_')[:2]))].append(name)
        chunks=[];current=[];size=0
        for names in (groups[k] for k in sorted(groups)):
            amount=sum((folder/n).stat().st_size for n in names)
            if current and size+amount>part_limit:chunks.append(current);current=[];size=0
            current.extend(names);size+=amount
        if current:chunks.append(current)
        parts=[]
        for index,names in enumerate(chunks,1):
            name=f'ao-vk-live-{version}-windows-x86-full-materials-{index}.zip';path=out/name
            with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
                for n in names:z.write(folder/n,n)
                if notices:z.write(license_path,'LICENSE-TEXTURES.md')
            with zipfile.ZipFile(path) as z:
                assert z.testzip() is None
                for n in names:assert hashlib.sha256(z.read(n)).hexdigest()==manifest['assets'][n],n
            parts.append(dict(name=name,sha256=digest(path),bytes=path.stat().st_size,files=names,notices=dict(notices)))
    omitted=set()
    for part in parts:
        path=out/part['name']
        assert path.stat().st_size==part['bytes'] and path.stat().st_size<2**31 and digest(path)==part['sha256']
        with zipfile.ZipFile(path) as z:
            assert set(z.namelist())==set(part['files'])|set(part.get('notices',{}))
            assert part.get('notices',{})==notices,'Rebuild companions to include the matching artwork license'
            for n,h in notices.items():assert hashlib.sha256(z.read(n)).hexdigest()==h
            part['unpackedBytes']=sum(x.file_size for x in z.infolist())
        assert not omitted.intersection(part['files']);omitted.update(part['files'])
    manifest['materialParts']=parts
    manifest['materialDownload']=dict(repository='Vitecval/ao-vk-live',tag='v'+version,baseUrl=f'https://github.com/Vitecval/ao-vk-live/releases/download/v{version}/')
    manifest['installBytes']=sum(x['unpackedBytes'] for x in parts)+sum((folder/n).stat().st_size for n in manifest['assets'] if n not in omitted)+sum((folder/n).stat().st_size for n in manifest['addon'])
    # Every required asset belongs to either the Setup ZIP or one companion.
    # Compare full relative paths; nested pack manifest.json files are required.
    manifest['payload'].update(manifest['assets'])
    (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    setup=out/f'ao-vk-live-{version}-windows-x86-full-setup.zip'
    temporary=setup.with_suffix('.building.zip')
    with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name in ['manifest.json',*sorted(manifest['payload'])]:
            if name not in omitted:z.write(folder/name,folder.name+'/'+name)
    assert temporary.stat().st_size<2**31
    with zipfile.ZipFile(temporary) as z:
        assert z.testzip() is None
        for name,h in manifest['payload'].items():
            if name not in omitted:assert hashlib.sha256(z.read(folder.name+'/'+name)).hexdigest()==h,name
        for name in manifest['assets']:
            assert name in omitted or folder.name+'/'+name in z.namelist(),name
    temporary.replace(setup)
    return [setup,*[out/x['name'] for x in parts]]
