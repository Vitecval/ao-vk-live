"""Pack the generated RGB/depth sources into the embedded runtime room atlas.

Requires Pillow. Resampling/packing only; the source artwork is not regenerated.
"""
from pathlib import Path
import sys,json,hashlib
S=Path(__file__).resolve().parents[1]
for root in S.parents:
 if (root/'build/preview19/python-deps').is_dir():sys.path.insert(0,str(root/'build/preview19/python-deps'))
from PIL import Image
A=S/'assets/newland-interiors'
colour=Image.open(A/'rooms-colour-source.png').convert('RGB')
depth=Image.open(A/'rooms-depth-source.png').convert('L')
assert colour.size==depth.size and abs(colour.width/colour.height-2)<.01
runtime=colour.resize((1024,512),Image.Resampling.LANCZOS).convert('RGBA')
runtime.putalpha(depth.resize((1024,512),Image.Resampling.BILINEAR))
runtime.save(A/'rooms-rgb-depth.png',optimize=True)
data=(A/'rooms-rgb-depth.png').read_bytes()
inc=S/'rvk/window_rooms.png.inc'
inc.write_text('// Generated room RGB + approximate depth. See assets/newland-interiors/PROVENANCE.json.\nstatic const unsigned char kWindowRooms[] = {\n'+''.join('  '+','.join('0x%02x'%b for b in data[i:i+24])+',\n' for i in range(0,len(data),24))+'};\n')
files=[]
for p in sorted(A.glob('*.png')):files.append({'path':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(A/'PROVENANCE.json').write_text(json.dumps({'origin':'Built-in image_gen, generated for this local experiment; user reference supplied style direction','runtime_size':[1024,512],'grid':[4,2],'room_tile_size':[256,256],'channels':'RGB baked interior appearance, alpha approximate artist-generated depth (white far)','tiles':['bedroom','studio','home office','sleeping nook','sparse home','hero apartment','office','mixed studio'],'presets':'10 deterministic presets reuse these 8 cards; storage/hero offices reuse office artwork with tint/brightness/mirroring','packing':'tools/pack_newland_rooms.py; Lanczos colour downsample, bilinear depth; no runtime external asset lookup','runtime_gpu_bytes_with_mips':2796204,'depth_limitation':'Single-view approximate depth; no recovered hidden surfaces; disocclusion may stretch/clamp at angles. Baked lighting does not follow moving scene lights.','files':files},indent=2)+'\n')
print('Embedded',len(data),'bytes; 8 reusable 256px room cards')
