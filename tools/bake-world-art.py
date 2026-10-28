#!/usr/bin/env python3
"""Bake generated world artwork and authored glass/metal regions; Pillow + NumPy.

No game database or original pixels are required. The runtime keeps architecture
alpha from the original texture and leaves the existing ground mesh intact.
"""
import hashlib
import json
import argparse
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1] / 'material-packs/ao-cyber-world'


def regions(item, prefix, w, h):
    y,x = np.mgrid[:h,:w]
    x,y = (x+.5)/w,(y+.5)/h
    mask = np.zeros((h,w), dtype=bool)
    for x0,y0,x1,y1 in item.get(prefix+'_rects',[]):
        mask |= (x>=x0)&(x<=x1)&(y>=y0)&(y<=y1)
    for cx,cy,rx,ry in item.get(prefix+'_ellipses',[]):
        mask |= ((x-cx)/rx)**2+((y-cy)/ry)**2 <= 1
    if item.get(prefix+'_polys'):
        polygons=Image.new('1',(w,h))
        draw=ImageDraw.Draw(polygons)
        for polygon in item[prefix+'_polys']:
            draw.polygon([(round(px*w),round(py*h)) for px,py in polygon],fill=1)
        mask |= np.asarray(polygons,dtype=bool)
    return mask


def save(path, pixels, size=None):
    image = Image.fromarray(np.uint8(np.clip(pixels,0,1)*255+.5))
    if size: image = image.resize(size,Image.Resampling.BOX)
    image.save(path,optimize=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only-new', action='store_true', help='Verify and retain existing baked materials; bake new identities only.')
    args = parser.parse_args()
    output = ROOT/'maps'
    output.mkdir(exist_ok=True)
    files, materials = {},[]
    prior = json.loads((ROOT/'manifest.json').read_text()) if args.only_new else {}
    prior_materials = {(x['type'],x['id']):x for x in prior.get('materials',[])}
    baked_new = 0
    catalog=json.loads((ROOT/'catalog.json').read_text())
    city=ROOT/'city-catalog.json'
    if city.exists(): catalog+=json.loads(city.read_text())
    props=ROOT/'props-catalog.json'
    if props.exists(): catalog+=json.loads(props.read_text())
    for name in ['shared-catalog.json','picked-catalog.json','scan-catalog.json','completion-catalog.json']:
        path=ROOT/name
        if path.exists(): catalog+=json.loads(path.read_text())
    if len({(x['type'],x['id']) for x in catalog})!=len(catalog):
        raise RuntimeError('Duplicate resource identity in world catalogs')
    # A reviewed family replaces earlier isolated repaints as a whole. Older
    # source art/catalogs are retained for provenance, not installed twice.
    for name in ['family-catalog.json','rubika-catalog.json','rubika-ground-catalog.json']:
        family=ROOT/name
        if family.exists():
            combined={(x['type'],x['id']):x for x in catalog}
            for item in json.loads(family.read_text()): combined[item['type'],item['id']]=item
            catalog=list(combined.values())
    for item in catalog:
        stem = f"{item['type']}_{item['id']}"
        previous = prior_materials.get((item['type'],item['id']))
        if previous:
            if any(previous.get(k)!=v for k,v in item.items() if k!='review'):
                raise RuntimeError(f'Existing material inputs changed: {stem}; review explicitly instead of rebaking it')
            for suffix in previous['maps']:
                name=stem+'_'+suffix+'.png'
                if hashlib.sha256((output/name).read_bytes()).hexdigest()!=prior['files'][name]:
                    raise RuntimeError(f'Existing map integrity failure: {name}')
                files[name]=prior['files'][name]
            materials.append(previous)
            continue
        art = Image.open(ROOT/'source-art'/(item['source']+'.png')).convert('RGB')
        w,h = item['size']
        art = art.resize((w,h),Image.Resampling.LANCZOS)
        art.save(output/(stem+'_d.png'),optimize=True)
        rgb = np.asarray(art,dtype=np.float32)/255
        grey = art.convert('L')
        lum = np.asarray(grey,dtype=np.float32)/255
        blur = np.asarray(grey.filter(ImageFilter.GaussianBlur(2)),dtype=np.float32)/255
        detail = np.clip(lum-blur,-.14,.14)
        glass = regions(item,'glass',w,h)
        metal = regions(item,'metal',w,h) & ~glass
        profile = item['profile']
        terrain = item['type'] == 1010006
        rough = np.full((h,w),.85 if terrain else .79,dtype=np.float32)
        metallic = np.zeros((h,w),dtype=np.float32)
        if profile == 'metal': rough[:],metallic[:] = .34,.78
        if profile == 'painted-metal': rough[:],metallic[:] = .58,.08
        if profile == 'architectural-metal': rough[:],metallic[:] = .32,.62
        if profile == 'diffuser': rough[:],metallic[:] = .3,0
        if profile == 'fabric': rough[:],metallic[:] = .9,0
        if profile == 'rubber': rough[:],metallic[:] = .92,0
        if profile == 'foliage': rough[:],metallic[:] = .72,0
        if profile == 'dry-foliage': rough[:],metallic[:] = .89,0
        if profile == 'dry-organic': rough[:],metallic[:] = .94,0
        if profile == 'mineral': rough[:],metallic[:] = .96,0
        if profile == 'honed-stone': rough[:],metallic[:] = .88,0
        if profile == 'satin-paving': rough[:],metallic[:] = .44,0
        if profile == 'glass': rough[:],metallic[:] = .09,0
        if profile == 'grate':
            rib=np.clip((lum-.10)/.25,0,1)
            rough=.94*(1-rib)+.45*rib
            metallic=.65*rib
        painted=regions(item,'painted',w,h)
        rough[painted],metallic[painted]=.58,.08
        fabric=regions(item,'fabric',w,h)
        rubber=regions(item,'rubber',w,h)
        rough[metal],metallic[metal] = .31,.82
        rough[glass],metallic[glass] = .075,0
        rough[fabric],metallic[fabric]=.9,0
        rough[rubber],metallic[rubber]=.86,0
        if terrain:
            rough = np.clip(rough+detail*.18,.38,.54) if profile == 'satin-paving' else np.clip(rough+detail*.18,.75,.96)
        dy,dx = np.gradient(detail)
        slope = 1.15 if terrain else .8
        normal = np.stack((-dx*slope,dy*slope,np.ones_like(dx)),axis=2)
        normal /= np.linalg.norm(normal,axis=2,keepdims=True)
        normal[glass] = (0,0,1) # window glass must not inherit painted interior relief
        if profile == 'glass': normal[:]=(0,0,1)
        small = (w//2,h//2)
        save(output/(stem+'_n.png'),normal*.5+.5,small)
        ao = np.clip(.98+detail*.25,.93,1)
        ao[glass] = 1
        save(output/(stem+'_orm.png'),np.stack((ao,rough,metallic),axis=2),small)
        maps = ['d','n','orm']
        if item.get('emission'):
            if item['emission'] == 'letters':
                y,x = np.mgrid[:h,:w]
                light = (lum>.67)&(x>w*.21)&(x<w*.89)&(y>h*.35)&(y<h*.70)
            elif item['emission']=='cyan':
                light=(rgb[:,:,0]<.35)&(rgb[:,:,1]>.42)&(rgb[:,:,2]>.42)
            elif item['emission']=='white':
                light=(rgb.min(axis=2)>.48)&(lum>.65)
            else:
                light = (rgb[:,:,0]>.78)&(rgb[:,:,1]>.32)&(rgb[:,:,2]<.20)
            if any(key.startswith('emissive_') for key in item):
                light &= regions(item,'emissive',w,h)
            save(output/(stem+'_e.png'),rgb*light[:,:,None]*.4,(min(w,256),min(h,256)))
            maps.append('e')
        for suffix in maps:
            name = stem+'_'+suffix+'.png'
            files[name] = hashlib.sha256((output/name).read_bytes()).hexdigest()
        materials.append({**item,'maps':maps,'preserve_original_alpha':not terrain,
                          'review':'atlas-reviewed; live appearance and tile boundaries pending'})
        baked_new += 1
    unexpected = {p.name for p in output.glob('*.png')}-files.keys()
    if unexpected: raise RuntimeError(f'Unexpected baked maps: {sorted(unexpected)}')
    manifest = {'name':'ao-cyber-world','version':6,'setting':'RVK_ModernTex=2',
                'new_art_surfaces':len(materials),'materials':materials,'files':files}
    checkpoint = ROOT/'manifest.json.tmp'
    checkpoint.write_text(json.dumps(manifest,indent=2)+'\n')
    checkpoint.replace(ROOT/'manifest.json')
    print(f'{len(materials)} actual repaints, {len(files)} baked maps; {baked_new} newly baked identities')


if __name__ == '__main__': main()
