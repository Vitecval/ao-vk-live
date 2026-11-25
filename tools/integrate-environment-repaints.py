"""Integrate completed worker records, with exact source hashes and material policy."""
import hashlib,json,re,runpy,struct
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/environment-078-r65'
api=runpy.run_path(str(ROOT/'tools/environment-repaint.py'))
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
existing={r['identity']:r for r in read(WORK/'individual-generation-records.json')}
paths=list((WORK/'workers').glob('*/records.json'))+list((WORK/'workers').glob('*/ground-expansion/records.json'))+list((WORK/'workers').glob('*/carved-floors/records.json'))
paths+=list((WORK/'workers').glob('*/grid-expansion/records.json'))
snapshots={path:read(path) for path in sorted(paths)}
primaries=dict(existing)
for rows in snapshots.values():
    for row in rows:
        if row.get('status')=='reviewed-accepted':primaries[row['identity']]=row
fingerprints=read(WORK/'native-pixel-fingerprints.json')['rows']

def verify_alias(row):
    identity=row['identity'];primary_id=row['alias_of'];primary=primaries.get(primary_id)
    assert primary and primary_id!=identity,('Missing alias primary',identity,primary_id)
    native=fingerprints[identity];other=fingerprints[primary_id]
    assert native.get('rgba_sha256') and native['rgba_sha256']==other.get('rgba_sha256') and native['size']==other['size'],('Native alias mismatch',identity,primary_id)
    assert api['sha'](Path(primary['source']))==api['sha'](Path(row['source'])),('Alias painting differs',identity)
    with Image.open(row['reference']) as im:original=im.convert('RGBA')
    digest=hashlib.sha256(struct.pack('<II',*original.size)+original.tobytes()).hexdigest()
    assert digest==native['rgba_sha256'],('Alias reference differs from native proof',identity)
    return primary

for path in sorted(paths):
    reviewed=snapshots[path];by_identity={r['identity']:r for r in reviewed}
    for row in reviewed:
        if row.get('status')!='reviewed-accepted':continue
        identity=row['identity'];source=Path(row['source'])
        primary=verify_alias(row) if row.get('alias_of') else None
        if identity in existing and api['sha'](Path(existing[identity]['source']))!=api['sha'](source):
            assert primary and primary.get('status') in {'reviewed-accepted','reviewed-and-baked'},('Different accepted sources',identity)
            assert api['sha'](Path(primary['source']))==api['sha'](source),('Alias painting differs',identity)
            log=WORK/'verified-source-aliases.json';aliases=read(log) if log.exists() else {}
            aliases.setdefault(identity,dict(primary=primary['identity'],prior_source=existing[identity]['source'],
                prior_source_sha256=api['sha'](Path(existing[identity]['source'])),selected_source=str(source),
                selected_source_sha256=api['sha'](source),proof='Exact native RGBA raster and dimensions; shared reviewed source. Prior source remains archived.'))
            log.write_text(json.dumps(aliases,indent=2)+'\n')
            print('Verified original/UV alias replaces archived redundant painting:',identity,'->',primary['identity'])
        assert api['sha'](source)==row['source_sha256'],('Source changed after review',identity)
        stem=identity.replace(':','_')
        original=Path('F:/AO_SInstall/Anarchy Online/randy-vk/packs/ao-cyber-world')
        response=row.get('material_response',row.get('runtime_response','matte non-metallic'))
        ice=row.get('is_ice',bool(re.search(r'\b(?:ice|icy)\b',str(response),re.I)))
        material=dict(response=response,ice=bool(ice) or identity=='1010004:217790',native_size=row.get('native_size'))
        if (identity.startswith('1010006:') or row.get('preserve_original_alpha')) and row.get('reference'):
            material['original_alpha']=row['reference']
        if row.get('preserve_original_palette') and row.get('reference'):
            material['original_palette']=row['reference']
        if row.get('painting_atlas'):material['painting_atlas']=row['painting_atlas']
        if row.get('use_surface_relief') is False:material['flat_normal']=True
        if row.get('alias_of'):material['alias_of']=row['alias_of']
        for suffix in ['n','orm','e','h']:
            companion=row.get('retained_companions',{}).get(suffix)
            if companion:
                saved=Path(companion.get('retained_copy',companion['source']))
                assert api['sha'](saved)==companion['sha256']
                material[suffix]=str(saved)
            elif row.get({'n':'reuse_original_normal','orm':'reuse_original_orm','e':'reuse_original_emissive','h':'reuse_original_height'}[suffix]):
                file=original/(stem+'_'+suffix+'.png')
                if file.exists():material[suffix]=str(file)
        if identity in existing and existing[identity].get('material')==material:continue
        api['accept'](identity,source,row['prompt'],material)
        existing[identity]=dict(source=str(source),material=material)
print('Total individual accepted paintings:',len(existing))
