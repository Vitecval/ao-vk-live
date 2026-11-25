"""Accept individually generated paintings; preserve provenance and runtime budgets."""
import argparse,hashlib,json,shutil
from pathlib import Path
import numpy as np
from PIL import Image,ImageFilter
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'material-packs/ao-penumbra-078'
WORK=ROOT/'build/environment-078-r65'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def accept(identity,generated,prompt,material=None):
    material=material or {}
    kind,number=map(int,identity.split(':'));stem=f'{kind}_{number}'
    manifest=read(PACK/'manifest.json')
    art=Image.open(generated).convert('RGB')
    atlas=material.get('painting_atlas')
    atlas_source=None
    if atlas:
        atlas_source=Path(atlas['source'])
        assert sha(atlas_source)==atlas['sha256'],'Shared painting changed after review'
        with Image.open(atlas_source) as whole:
            assert max(whole.size)>=1024,'Shared painting is not HD'
            box=tuple(atlas['box'])
            assert len(box)==4 and 0<=box[0]<box[2]<=whole.width and 0<=box[1]<box[3]<=whole.height
            crop=whole.convert('RGB').crop(box)
            assert crop.size==art.size and crop.tobytes()==art.tobytes(),'Export is not an exact crop of the shared painting'
    else:
        assert max(art.size)>=1024,'Do not upscale a low-resolution painting'
    native=material.get('native_size')
    if native:
        assert abs((art.width/art.height)/(native[0]/native[1])-1)<.04,('Review generated aspect/UV fit',identity,native,art.size)
        assert art.width>=native[0] and art.height>=native[1],('Painting loses native detail',identity)
    source=PACK/'source-art'/f'{stem}-r65-v1.png'
    version=1
    while source.exists() and sha(source)!=sha(generated):
        version+=1;source=PACK/'source-art'/f'{stem}-r65-v{version}.png'
    if not source.exists():shutil.copy2(generated,source)
    # More colour detail than the older 512px atlas crops; auxiliaries retain
    # their own memory limit. These maps do not bake new lighting into albedo.
    scale=min(1.,1024/max(art.size))
    width=max(1,round(art.width*scale));height=max(1,round(art.height*scale))
    art=art.resize((width,height),Image.Resampling.LANCZOS)
    palette_source=material.get('original_palette');palette=None
    if palette_source:
        with Image.open(palette_source) as original:
            assert original.convert('RGBA').getchannel('A').getextrema()==(255,255),'Review non-opaque palette weighting separately'
            target=np.asarray(original.convert('RGB'),dtype=float).mean(axis=(0,1))
        pixels=np.asarray(art,dtype=float);mean=pixels.mean(axis=(0,1));gain=target/np.maximum(mean,1.)
        assert np.all((gain>=.65)&(gain<=1.5)),('Palette drift needs a new source review',identity,gain)
        art=Image.fromarray(np.uint8(np.clip(pixels*gain,0,255)+.5))
        palette=dict(source=str(palette_source),source_sha256=sha(Path(palette_source)),target_mean_rgb=target.tolist(),gain=gain.tolist(),policy='Original broad mean only; independent painting supplies every fine texel and UV joint.')
    maps=PACK/'maps'
    alpha_source=material.get('original_alpha')
    exported=art
    if alpha_source:
        original=Image.open(alpha_source).convert('RGBA')
        alpha=original.getchannel('A').resize((width,height),Image.Resampling.LANCZOS)
        if alpha.getextrema()!=(255,255):
            exported=art.convert('RGBA');exported.putalpha(alpha)
    exported.save(maps/(stem+'_d.png'))
    size=(max(1,width//2),max(1,height//2));small=art.resize(size,Image.Resampling.LANCZOS)
    gray=small.convert('L');lum=np.asarray(gray,dtype=float)/255.
    fine=lum-np.asarray(gray.filter(ImageFilter.GaussianBlur(2)),dtype=float)/255.
    dy,dx=np.gradient(np.clip(fine,-.12,.12))
    normal=np.stack((-dx*.7,dy*.7,np.ones_like(dx)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
    Image.fromarray(np.uint8((normal*.5+.5)*255+.5)).save(maps/(stem+'_n.png'))
    if material.get('flat_normal'):Image.new('RGB',size,(128,128,255)).save(maps/(stem+'_n.png'))
    Image.new('RGB',size,(255,232,0)).save(maps/(stem+'_orm.png'))
    Image.new('RGB',size).save(maps/(stem+'_e.png'))
    response=material.get('response','matte non-metallic')
    if material.get('ice'):
        rough=np.clip(.24+(lum-.4)*.5,.20,.65)
        orm=np.stack((np.full_like(lum,255),rough*255,np.zeros_like(lum)),axis=2)
        Image.fromarray(np.uint8(orm+.5)).save(maps/(stem+'_orm.png'))
    retained={}
    for suffix in ['n','orm','e','h']:
        src=material.get(suffix)
        if not src:continue
        src=Path(src);assert src.exists(),src
        shutil.copy2(src,maps/(stem+'_'+suffix+'.png'));retained[suffix]=dict(source=str(src),sha256=sha(src))
    suffixes=['d','n','orm','e']+(['h'] if 'h' in retained else [])
    row=dict(type=kind,id=number,size=[width,height],material_map_size=list(size),
        map_sizes={suffix:list(Image.open(maps/(stem+'_'+suffix+'.png')).size) for suffix in suffixes},
        source=source.relative_to(PACK).as_posix(),source_sha256=sha(source),maps=suffixes,
        preserve_original_alpha=kind!=1010006 or bool(alpha_source),response=response,retained_companions=retained,
        review='Individual full-frame painting reviewed; runtime maps downsampled, never upscaled. Retail mesh fit pending.')
    manifest['materials']=[r for r in manifest['materials'] if (r['type'],r['id'])!=(kind,number)]+[row]
    if alpha_source:row['original_alpha_source']=str(alpha_source);row['original_alpha_source_sha256']=sha(Path(alpha_source))
    if palette:row['palette_calibration']=palette
    if material.get('flat_normal'):row['normal_policy']='flat-functional-surface'
    if atlas:
        full=PACK/'source-art'/('shared-atlas-'+atlas['sha256']+'.png')
        if not full.exists():shutil.copy2(atlas_source,full)
        assert sha(full)==atlas['sha256']
        row['painting_atlas']=dict(source=full.relative_to(PACK).as_posix(),sha256=atlas['sha256'],box=list(atlas['box']))
    for suffix in row['maps']:
        file=maps/(stem+'_'+suffix+'.png');manifest['files'][file.name]=sha(file)
    manifest['surface_identities']=len(manifest['materials']);manifest['new_paintings']=len({r.get('painting_atlas',{}).get('sha256',r['source_sha256']) for r in manifest['materials']})
    write(PACK/'manifest.json',manifest)
    file=WORK/'individual-generation-records.json';records=read(file) if file.exists() else []
    records=[r for r in records if r['identity']!=identity]+[dict(identity=identity,source=str(source),generated=str(generated),source_size=list(Image.open(source).size),runtime_size=[width,height],prompt=prompt,provider='built-in image generation',status='reviewed-and-baked',material=material)]
    write(file,records)
    print(identity,list(art.size),'reviewed and baked')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('identity');p.add_argument('generated',type=Path);p.add_argument('--prompt-file',required=True,type=Path);a=p.parse_args()
    accept(a.identity,a.generated,a.prompt_file.read_text(encoding='utf-8'))
