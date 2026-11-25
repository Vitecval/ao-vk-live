"""Audit installed world/ground coverage; export exact RDB authoring references.

Resolution is evidence, not a claim of visual quality. Runtime captures prove
residency separately. Original game pixels are retained only under build/.
"""
import argparse, collections, io, json, re, sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from rdb_records import read_record_at

def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--client', type=Path, default=Path('F:/AO_SInstall/Anarchy Online'))
    p.add_argument('--output', type=Path, default=ROOT/'build/environment-078-r65')
    a = p.parse_args(); out = a.output; out.mkdir(parents=True, exist_ok=True)
    meta = ROOT/'build/character-rework/references/modern-icc'
    names, index, fmt = [read(meta/n) for n in ['rdb-names.json','rdb-index.json','rdb-format.json']]
    dims = read(ROOT/'material-packs/native-texture-dimensions.json')['sizes']
    captures = sorted(a.client.glob('rvk-frame-*.txt'), key=lambda p:p.stat().st_mtime, reverse=True)[:8]
    resident = {}; seen = set()
    for cap in captures:
        for line in cap.read_text(errors='replace').splitlines():
            hit = re.search(r'rdb:(\d+) type:(1010004|1010006)', line)
            if not hit: continue
            key = hit[2]+':'+hit[1]; seen.add(key)
            size = re.search(r'resident-albedo:(\d+)x(\d+)',line)
            if size: resident[key] = list(map(int,size.groups()))
    packs = a.client/'randy-vk/packs'
    providers = [a.client/'randy-vk/materials', packs/'ao-penumbra-078', packs/'ao-cyber-world']
    rows=[]
    for key in sorted(k for k in index if k.startswith(('1010004:','1010006:'))):
        kind, identity = map(int,key.split(':')); stem=key.replace(':','_')
        files = [(folder,folder/(stem+'_d.png')) for folder in providers]
        chosen = next(((folder,f) for folder,f in files if f.exists()),None)
        row=dict(identity=key,name=names.get(key,''),native_size=dims.get(key),captured=key in seen,resident_size=resident.get(key))
        if chosen:
            folder,f=chosen
            try:
                with Image.open(f) as im: size=list(im.size)
                row.update(provider=folder.name,path=str(f),replacement_size=size,
                    complete=all((folder/(stem+s+'.png')).exists() for s in ['_d','_n','_orm']))
                native=dims.get(key)
                row['status']='missing-companion-map' if not row['complete'] else 'below-1k-review' if max(size)<1024 else 'resolution-present-visual-review'
                if native and max(size)<=max(native):row['status']='no-resolution-gain-review'
            except Exception as e:row.update(status='unreadable',error=str(e))
        else: row['status']='no-installed-albedo'
        rows.append(row)
    counts=collections.Counter(r['status'] for r in rows)
    write=lambda name,value:(out/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
    write('texture-audit.json',dict(scope='All indexed full-quality world and ground identities. Personal, private environment and selected world pack candidates; capture residency is direct evidence. Missing albedo can be intentional UI/effect/stock retention. Quality requires visual review.',counts=counts,rows=rows,captures=[str(p) for p in captures]))
    selected=[209665,211094,209763]
    # Exact resource names provide leads for emission review, never automatic lights.
    lava=[r for r in rows if re.search(r'lava',r['name'],re.I)]
    write('lava-candidates.json',lava)
    refs=out/'references';refs.mkdir(exist_ok=True)
    for identity in selected+[int(r['identity'].split(':')[1]) for r in lava]:
        key='1010004:'+str(identity)
        if key not in index:continue
        data=read_record_at(a.client/'cd_image/data/db',index[key],1010004,identity,fmt['dataFileSize'],fmt['blockOffset'])
        try:Image.open(io.BytesIO(data[12:])).convert('RGB').save(refs/(key.replace(':','_')+'.png'))
        except Exception:continue
    focus=[r for r in rows if int(r['identity'].split(':')[1]) in selected or (r['captured'] and r['status']!='resolution-present-visual-review')]
    write('captured-review-queue.json',focus)
    thumbs=[]
    for r in focus[:60]:
        if 'path' not in r:continue
        tile=Image.new('RGB',(250,170),'#20252b');im=Image.open(r['path']).convert('RGB');im.thumbnail((244,132))
        tile.paste(im,((250-im.width)//2,0));d=ImageDraw.Draw(tile);d.text((4,134),r['identity']+' '+str(r['replacement_size']),fill='white');d.text((4,150),r['name'][:35],fill='white');thumbs.append(tile)
    if thumbs:
        sheet=Image.new('RGB',(250*5,170*((len(thumbs)+4)//5)),'#20252b')
        for i,t in enumerate(thumbs):sheet.paste(t,((i%5)*250,(i//5)*170))
        sheet.save(out/'captured-review.png')
    print(json.dumps(dict(counts=counts,captured=len(seen),priority=len(focus),lava=len(lava),named=[r for r in rows if int(r['identity'].split(':')[1]) in selected]),indent=2))

if __name__=='__main__': main()
