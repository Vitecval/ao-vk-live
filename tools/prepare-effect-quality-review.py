"""Private visual reference pairs for effect/sky role review; no automatic quality verdicts."""
import argparse,hashlib,io,json,sys
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from rdb_records import read_record_at
parser=argparse.ArgumentParser();parser.add_argument('--scope',default='effect-or-sky-review');parser.add_argument('--start',type=int,default=0);parser.add_argument('--limit',type=int);parser.add_argument('--folder',default='root-effects');args=parser.parse_args()
WORK=ROOT/'build/environment-078-r65';OUT=WORK/'workers'/args.folder;OUT.mkdir(exist_ok=True)
REF=OUT/'references';REF.mkdir(exist_ok=True)
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
meta=ROOT/'build/character-rework/references/modern-icc';index=read(meta/'rdb-index.json');fmt=read(meta/'rdb-format.json')
rows=[r for r in read(WORK/'workers/world-audit/refined-texture-audit.json')['rows'] if r['scope']==args.scope]
rows=rows[args.start:args.start+args.limit if args.limit else None]
def tile(path):
 im=Image.open(path).convert('RGB');im.thumbnail((154,154))
 canvas=Image.new('RGB',(160,160),'#656565');canvas.paste(im,((160-im.width)//2,(160-im.height)//2));return canvas
evidence=[];errors=[]
for row in rows:
 identity=row['identity'];kind,number=map(int,identity.split(':'));ref=REF/(identity.replace(':','_')+'.png')
 if not ref.exists():
  data=read_record_at(Path('F:/AO_SInstall/Anarchy Online/cd_image/data/db'),index[identity],kind,number,fmt['dataFileSize'],fmt['blockOffset'])
  payload=data[12:]
  # Some AO records replace the first PNG word with a format enum; the PNG
  # suffix, IHDR and chunk CRCs remain intact. Restore only that known header.
  if payload[4:8]==b'\r\n\x1a\n' and payload[12:16]==b'IHDR':payload=b'\x89PNG'+payload[4:]
  try:Image.open(io.BytesIO(payload)).save(ref)
  except Exception as error:
   errors.append(dict(identity=identity,error=str(error),header=data[:32].hex()));continue
 current=row.get('selected_albedo');current=Path(current) if current else None
 evidence.append(dict(identity=identity,name=row['name'],reference=str(ref),native_size=list(Image.open(ref).size),
  reference_sha256=hashlib.sha256(ref.read_bytes()).hexdigest(),current=str(current) if current and current.exists() else None,
  current_sha256=hashlib.sha256(current.read_bytes()).hexdigest() if current and current.exists() else None))
for start in range(0,len(evidence),24):
 board=Image.new('RGB',(6*328,4*197),'#222222');draw=ImageDraw.Draw(board)
 for n,row in enumerate(evidence[start:start+24]):
  x=(n%6)*328;y=(n//6)*197;board.paste(tile(row['reference']),(x,y+32))
  if row['current']:board.paste(tile(row['current']),(x+164,y+32))
  draw.text((x+2,y+2),row['identity']+' '+row['name'][:29],fill='white')
  draw.text((x+2,y+17),'original / '+('selected replacement' if row['current'] else 'no replacement'),fill='#bbbbbb')
 board.save(OUT/f'pairs-{start//24+1}.png')
(OUT/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
(OUT/'reference-errors.json').write_text(json.dumps(errors,indent=2)+'\n')
print('Prepared',len(evidence),'original/current role-review pairs.')
if errors:print('Unsupported references held:',len(errors))
