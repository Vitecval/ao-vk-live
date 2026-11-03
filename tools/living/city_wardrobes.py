"""Owner-cache clothing grades. No client access, new geometry, or runtime changes."""
from pathlib import Path
import argparse,json,hashlib,struct,random,collections,shutil
import numpy as np
from cache_v3 import safe,mesh,save,rgba,argb
R=Path(__file__).resolve().parents[2]
FAMILIES=[('resident-jacket',['resident','frontier-coat','navy-security'],{42252,42253,42250}),('traveler-bomber',['traveler','market-merchant','olive-traveler'],{247894,247940,244580}),('technician-workshirt',['technician','maintenance-crew','market-worker'],{9607,9609,244580})]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def grade(pixels,colour):
 a=rgba(pixels);rgb=a[:,:,:3].astype(np.float32)/255;lum=rgb@np.array([.2126,.7152,.0722],np.float32);t=np.array([int(colour[i:i+2],16) for i in (1,3,5)],np.float32)/255;light=float(t@np.array([.2126,.7152,.0722]));hue=.8*t/max(light,.1)+.2;brightness=np.clip(.64+light*.9,.72,1.18)
 a[:,:,:3]=np.clip((.14*rgb+.86*lum[:,:,None]*hue)*brightness*255,0,255).astype(np.uint8);return argb(a)
def select_variants(lines,mc,ac,weights,pf):
 rng=random.Random(0x4c525700+pf);placed=[];counts=collections.Counter();changes=0;family_counts=collections.Counter()
 for idx in range(1+mc,1+mc+ac):
  fields=lines[idx].split()
  if int(fields[0])!=0:continue
  old=int(fields[1]);family=old//3;assert 0<=old<9;family_counts[family]+=1;start=family*3;w=weights[start:start+3];knots=int(fields[9]);p=np.asarray(list(map(float,fields[10:10+knots*3]))).reshape(-1,3)[:,(0,2)].mean(0)
  # Stable assignment, biased per city, with a penalty for repeating nearby clothes.
  scores=[]
  for v in range(3):
   slot=start+v;expected=family_counts[family]*w[v]/sum(w);repeat=sum(max(0.,1-float(np.linalg.norm(p-q))/18) for q,s in placed if s==slot)
   scores.append((expected-counts[slot]+rng.random()*.3-repeat*2,slot))
  slot=max(scores)[1];counts[slot]+=1;placed.append((p,slot));changes+=slot!=old;fields[1]=str(slot);lines[idx]=' '.join(fields)
 return dict(counts={str(k):counts[k] for k in range(9)},changed_variant_assignments=changes,people=len(placed))
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=R/'private/v5-wardrobes/assets');args=p.parse_args();out=safe(args.output);out.mkdir(parents=True,exist_ok=True);base=R/'private/v5-cities/assets-complete';config=json.loads((R/'experiment/CITY-WARDROBES.json').read_text());receipts=json.loads((R/'private/v2/assets/asset-receipts.json').read_text())['assets'];assets=[]
 for theme,profile in config['themes'].items():
  for family,(original,styles,ids) in enumerate(FAMILIES):
   receipt=next(x for x in receipts if x['name']==original);tiles=[i for i,x in enumerate(receipt['textures']) if x['id'] in ids];assert len(tiles)==len(ids);first=len(receipt['textures']);reference=mesh(base/f'civilian-{styles[0]}.lrm');dim=reference['header'][5];side=dim//4;atlas=reference['maps'][0].copy();mask=np.zeros((dim,dim),bool)
   for variant in range(3):
    for j,tile in enumerate(tiles):
     target=tile if variant==0 else first+(variant-1)*len(tiles)+j;assert target<16;sy,sx=divmod(tile,4);dy,dx=divmod(target,4);block=reference['maps'][0][sy*side:(sy+1)*side,sx*side:(sx+1)*side];atlas[dy*side:(dy+1)*side,dx*side:(dx+1)*side]=grade(block,profile['colours'][family*3+variant]);mask[dy*side:(dy+1)*side,dx*side:(dx+1)*side]=True
   for style in styles:
    source=base/f'civilian-{style}.lrm';m=mesh(source);before=m['maps'][0].copy();m['maps'][0]=atlas;target=out/f'cw-{theme}-{style}.lrm';save(target,m)
    # Byte-level invariants: geometry, UVs, gait, palettes, alpha, non-clothing and all other maps unchanged.
    b=source.read_bytes();a=target.read_bytes();start=len(b)-16*dim*dim;end=start+4*dim*dim
    assert len(a)==len(b) and a[:start]==b[:start] and a[end:]==b[end:]
    assert np.array_equal(atlas[~mask],before[~mask]) and np.array_equal(atlas>>24,before>>24)
    assert np.any(atlas[mask]!=before[mask]);assets.append(dict(name=target.name,source=source.name,theme=theme,sha256=sha(target),bytes=len(a),garment_pixels_changed=int(np.count_nonzero(atlas!=before)),geometry_rig_and_other_maps_identical=True,non_clothing_and_alpha_identical=True))
 scenes=[];required=set()
 for source in sorted(base.glob('*.lrs')):
  if not source.stem.isdigit():continue
  pf=source.stem;lines=source.read_text().splitlines();header=lines[0].split();mc,ac=map(int,header[4:6]);stats={}
  if pf in config['cities']:
   profile=config['cities'][pf];theme=profile['theme']
   for i in range(1,10):assert lines[i].startswith('civilian-');lines[i]='cw-'+theme+'-'+lines[i][len('civilian-'):]
   stats=select_variants(lines,mc,ac,profile['weights'],int(pf));header[-1]='v5-city-clothing-static-review-live-check-required';lines[0]=' '.join(header)
  target=out/source.name;target.write_text('\n'.join(lines)+'\n',encoding='utf-8');required.update(lines[1:1+mc]);ground=lines[-1]
  if ground!='-':required.add(ground)
  scenes.append(dict(pf=int(pf),theme=config['cities'].get(pf,{}).get('theme','unchanged outdoor habitat'),**stats))
 for name in sorted(required):
  target=out/name
  if not target.exists():shutil.copy2(safe(base/name),target)
 for name in ['review.ini','PRIVATE-NOT-FOR-DISTRIBUTION.txt']:shutil.copy2(base/name,out/name)
 manifest=[dict(name=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(out.iterdir()) if p.name in required or p.name in ['review.ini','PRIVATE-NOT-FOR-DISTRIBUTION.txt'] or (p.suffix=='.lrs' and p.stem.isdigit())]
 receipt=dict(version='V5.1',source_index='9f3633726602ead5003bd3aec82f67f63462ff1080a5676f91c2f149ddfbf6d9',private_assets_not_distributable=True,themed_outfits=len(assets),themes=len(config['themes']),assets=assets,scenes=scenes,files=manifest)
 E=R/'evidence/v5-wardrobes';E.mkdir(exist_ok=True);(E/'asset-validation.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');(R/'experiment/OWNER-WARDROBES-V5.json').write_text(json.dumps({k:v for k,v in receipt.items() if k not in ['assets','scenes']},indent=2),encoding='utf-8')
 D=R/'experiment/assets/city-wardrobes-v5';D.mkdir(exist_ok=True)
 for p in out.glob('*.lrs'):shutil.copy2(p,D/p.name)
 print(json.dumps(dict(themes=len(config['themes']),themed_outfits=len(assets),city_packs=len(config['cities']),runtime_files=len(manifest),bytes=sum(f['bytes'] for f in manifest)),indent=2))
if __name__=='__main__':main()
