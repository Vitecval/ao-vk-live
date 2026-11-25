"""Bake reviewed generated environment paintings and record honest coverage."""
from pathlib import Path
import hashlib,json,re,shutil
import numpy as np
from PIL import Image,ImageFilter
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/environment-0.7.8'
PACK=WORK/'ao-penumbra-078'
LIB=ROOT/'build/preview23/artwork-completion-r32/full-library/randy-vk/packs/ao-cyber-world'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8')
records=json.loads((WORK/'generation-records.json').read_text())
for filename in ['related-generation-records.json','last-related-generation-records.json']:
    records['rows']+=json.loads((WORK/filename).read_text())['rows']
corrected=json.loads((WORK/'corrected-glass-record.json').read_text())
records['rows']=[corrected if x['id']==corrected['id'] else x for x in records['rows']]
source=PACK/'source-art';maps=PACK/'maps'
source.mkdir(parents=True,exist_ok=True);maps.mkdir(exist_ok=True)
for row in records['rows']:
    # Tool output also includes a prose path prefix. Retain the actual filename.
    name=re.search(r'exec-[\w-]+\.png',row['generated']).group()
    src=Path('C:/Users/valer/.codex/generated_images/01a1f42f-bfe7-7611-b1fa-4eeadcec8e1f')/name
    target=source/f"1010004_{row['id']}-v1.png"
    if target.exists():assert sha(target)==sha(src)
    else:shutil.copy2(src,target)
    row['generated']=str(src);row['source']=target.relative_to(PACK).as_posix()
snow=ROOT/'material-packs/ao-penumbra-078/source-art/snow-powder-v1.png'
shutil.copy2(snow,source/snow.name)
by_id={x['id']:x for x in records['rows']}
aliases={226466:206037,206036:'snow',437:'snow',224461:224308}
entries=[];files={}
for identity in [*by_id,*aliases]:
    kind=1010006 if identity==437 else 1010004
    master=aliases.get(identity,identity)
    art_path=source/snow.name if master=='snow' else PACK/by_id[master]['source']
    art=Image.open(art_path).convert('RGB')
    if identity in {224322,224465}:
        # Authored normalized UV layout: deliberately projected to this strip.
        size=(1024,128)
    elif art.height==art.width:size=(1024,1024)
    elif art.width>art.height:size=(1408,704)
    else:size=(704,1408)
    assert art.width>=size[0] and art.height>=size[1],('No upscaling',identity,art.size,size)
    art=art.resize(size,Image.Resampling.LANCZOS)
    stem=f'{kind}_{identity}'
    art.save(maps/(stem+'_d.png'))
    aux=tuple(max(1,round(n*512/max(size))) for n in size)
    small=art.resize(aux,Image.Resampling.LANCZOS)
    grey=small.convert('L'); lum=np.asarray(grey,dtype=float)/255
    fine=np.clip(lum-np.asarray(grey.filter(ImageFilter.GaussianBlur(2)),dtype=float)/255,-.12,.12)
    dy,dx=np.gradient(fine)
    normal=np.stack((-dx*.8,dy*.8,np.ones_like(dx)),axis=2)
    normal/=np.linalg.norm(normal,axis=2,keepdims=True)
    Image.fromarray(np.uint8((normal*.5+.5)*255+.5)).save(maps/(stem+'_n.png'))
    ice=identity in {206037,206353,206889,226466,226467,224460,229085}
    rough=np.clip(.18+np.clip((lum-.45)/.5,0,1)*.46,.18,.64) if ice else np.full_like(lum,.93 if master=='snow' else .87)
    # Non-metallic frost and ice. Snow/stone stay matte. No inferred emission.
    orm=np.stack((np.full_like(lum,250),rough*255,np.zeros_like(lum)),axis=2)
    Image.fromarray(np.uint8(orm+.5)).save(maps/(stem+'_orm.png'))
    Image.new('RGB',aux).save(maps/(stem+'_e.png'))
    entries.append(dict(type=kind,id=identity,size=size,material_map_size=aux,
        source=art_path.relative_to(PACK).as_posix(),source_sha256=sha(art_path),maps=['d','n','orm','e'],
        shared_master=master if master!=identity else None,preserve_original_alpha=kind!=1010006,
        response='dielectric ice, varied frost roughness' if ice else 'matte non-metallic',
        review='Artwork reviewed; retail mesh fit and appearance pending.'))
    for suffix in ['d','n','orm','e']:
        p=maps/(stem+'_'+suffix+'.png');files[p.name]=sha(p)
write(PACK/'manifest.json',dict(name='ao-penumbra-078',version='0.7.8-private',materials=entries,files=files,
    new_paintings=len(records['rows'])+1,surface_identities=len(entries),setting='RVK_ModernTex=2',
    notes='Normal maps derived from fine albedo detail; not scanned geometry. Ice uses dielectric reflection with frost diffusion; no volumetric transmission. Original alpha is retained by renderer.'))
write(PACK/'prompts.json',records)
related=json.loads((WORK/'RELATED-FAMILY-IDS.json').read_text())
covered={x['id'] for x in entries}
for row in related:
    identity=int(row['key'].split(':')[1]);p=LIB/f'1010004_{identity}_d.png'
    row.update(previous_file_exists=p.exists(),previous_size=Image.open(p).size if p.exists() else None,
        status='new reviewed painting candidate' if identity in covered else 'name-only identity: absent from the installed RDB index and current library; no guessed UV replacement')
write(WORK/'SNOW-FAMILY-COVERAGE.json',related)
print(json.dumps(dict(pack=str(PACK),paintings=len(records['rows'])+1,identities=len(entries),maps=len(files),related_pending=sum(int(x['key'].split(':')[1]) not in covered for x in related))))
