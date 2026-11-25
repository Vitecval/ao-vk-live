"""Withdraw only uninstalled, redundant aliases whose actual current art is adequate."""
import hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'build/environment-078-r65';PACK=ROOT/'material-packs/ao-penumbra-078'
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
ids={'1010006:3','1010006:4','1010006:9','1010006:14','1010006:396','1010004:259433'}
installed=read(Path('F:/AO_SInstall/Anarchy Online/randy-vk/packs/ao-penumbra-078/manifest.json'))
assert not ids.intersection(f"{r['type']}:{r['id']}" for r in installed['materials']),'An installed identity needs an explicit rollback'
manifest=read(PACK/'manifest.json');records=read(WORK/'individual-generation-records.json')
removed=[r for r in records if r['identity'] in ids]
assert all(r.get('material',{}).get('alias_of') for r in removed),'Only reviewed redundant aliases may be withdrawn here'
archive=WORK/'retained-adequate-alias-archive';archive.mkdir(exist_ok=True)
history=archive/'withdrawn-records.json'
if removed:
 assert not history.exists(),'Archive exists; inspect before replacing history'
 history.write_text(json.dumps(removed,indent=2)+'\n')
 rows=[r for r in manifest['materials'] if f"{r['type']}:{r['id']}" in ids]
 (archive/'withdrawn-materials.json').write_text(json.dumps(rows,indent=2)+'\n')
 for row in rows:
  for suffix in row['maps']:
   name=f"{row['type']}_{row['id']}_{suffix}.png";source=(PACK/'maps'/name).resolve()
   assert source.parent==(PACK/'maps').resolve() and source.name==name
   assert sha(source)==manifest['files'][name]
   destination=archive/name;assert not destination.exists()
   shutil.move(str(source),str(destination));assert sha(destination)==manifest['files'][name]
   del manifest['files'][name]
 manifest['materials']=[r for r in manifest['materials'] if f"{r['type']}:{r['id']}" not in ids]
 manifest['surface_identities']=len(manifest['materials']);manifest['new_paintings']=len({r['source_sha256'] for r in manifest['materials']})
 (PACK/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (WORK/'individual-generation-records.json').write_text(json.dumps([r for r in records if r['identity'] not in ids],indent=2)+'\n')
print('Withdrawn',len(removed),'uninstalled alias identities; original/current artwork retained; generated source history preserved.')
