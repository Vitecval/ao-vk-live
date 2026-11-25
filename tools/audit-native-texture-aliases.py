"""Fingerprint exact native RGBA rasters; aliases are evidence, never quality verdicts."""
import hashlib,io,json,struct,sys
from collections import Counter,defaultdict
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from rdb_records import read_record_at
from PIL import Image
WORK=ROOT/'build/environment-078-r65';META=ROOT/'build/character-rework/references/modern-icc'
DB=Path('F:/AO_SInstall/Anarchy Online/cd_image/data/db')
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
index=read(META/'rdb-index.json');fmt=read(META/'rdb-format.json')
rows=read(WORK/'workers/world-audit/refined-texture-audit.json')['rows'];by_id={r['identity']:r for r in rows}
stamp=[(p.name,p.stat().st_size,p.stat().st_mtime_ns) for p in sorted(DB.iterdir()) if p.is_file()]
cache_path=WORK/'native-pixel-fingerprints.json';old=read(cache_path) if cache_path.exists() else {}
cache=old.get('rows',{}) if old.get('database_stamp')==[list(x) for x in stamp] else {}
def save():
 temp=cache_path.with_suffix('.writing.json');temp.write_text(json.dumps(dict(database_stamp=stamp,rows=cache),separators=(',',':')));temp.replace(cache_path)
def decode(data):
 starts=[12,36]
 for marker in [b'\x89PNG',b'\xff\xd8\xff',b'DDS ']:
  offset=data.find(marker,0,4096)
  if offset>=0:starts.append(offset)
 for offset in dict.fromkeys(starts):
  payload=data[offset:]
  if payload[4:8]==b'\r\n\x1a\n' and payload[12:16]==b'IHDR':payload=b'\x89PNG'+payload[4:]
  try:
   im=Image.open(io.BytesIO(payload));im.load();return im.convert('RGBA'),offset
  except Exception:pass
 raise ValueError('No supported image payload at verified image/header candidates')
for n,row in enumerate(sorted(rows,key=lambda r:index[r['identity']])):
 identity=row['identity']
 if identity not in cache:
  kind,number=map(int,identity.split(':'))
  try:
   data=read_record_at(DB,index[identity],kind,number,fmt['dataFileSize'],fmt['blockOffset']);image,offset=decode(data)
   digest=hashlib.sha256(struct.pack('<II',*image.size)+image.tobytes()).hexdigest()
   cache[identity]=dict(size=list(image.size),rgba_sha256=digest,payload_offset=offset)
  except Exception as error:cache[identity]=dict(error=str(error))
 if (n+1)%500==0:save();print(n+1,'native identities fingerprinted',flush=True)
save();groups=defaultdict(list)
for identity,row in cache.items():
 if row.get('rgba_sha256'):groups[row['rgba_sha256']].append(identity)
baked={r['identity'] for r in read(WORK/'individual-generation-records.json')};aliases=[]
for digest,identities in groups.items():
 if len(identities)<2:continue
 scopes=sorted({by_id[k]['scope'] for k in identities})
 aliases.append(dict(rgba_sha256=digest,size=cache[identities[0]]['size'],identities=sorted(identities),
  scopes=scopes,mixed_scopes=len(scopes)>1,accepted_repaint_ids=sorted(baked.intersection(identities)),
  other_ids=sorted(set(identities)-baked),proof='Exact native decoded RGBA and dimensions; runtime material role/companions remain separate.'))
summary=dict(indexed_identities=len(rows),decoded=sum('rgba_sha256' in r for r in cache.values()),
 unsupported=sum('error' in r for r in cache.values()),exact_alias_groups=len(aliases),
 duplicate_native_identities=sum(len(g['identities'])-1 for g in aliases),
 groups_linked_to_accepted_paintings=sum(bool(g['accepted_repaint_ids']) for g in aliases),
 interpretation='Pixel equivalence does not establish low quality, eligibility or material response. Review role and target companions before source sharing.')
(WORK/'NATIVE-TEXTURE-ALIASES.json').write_text(json.dumps(dict(summary=summary,groups=aliases),indent=2)+'\n')
print(json.dumps(summary,indent=2),flush=True)
