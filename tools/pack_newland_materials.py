"""Embed the small surface-material atlas used on real analytic room geometry."""
from pathlib import Path
import sys,hashlib,json
S=Path(__file__).resolve().parents[1]
for root in S.parents:
 if (root/'build/preview19/python-deps').is_dir():sys.path.insert(0,str(root/'build/preview19/python-deps'))
from PIL import Image
A=S/'assets/newland-materials'
im=Image.open(A/'materials-source.png').convert('RGBA').resize((1024,512),Image.Resampling.LANCZOS)
im.save(A/'materials-runtime.png',optimize=True)
data=(A/'materials-runtime.png').read_bytes()
(S/'rvk/window_rooms.png.inc').write_text('// Surface materials for analytic Newland rooms. See assets/newland-materials.\nstatic const unsigned char kWindowRooms[] = {\n'+''.join('  '+','.join('0x%02x'%b for b in data[i:i+24])+',\n' for i in range(0,len(data),24))+'};\n')
(A/'PROVENANCE.json').write_text(json.dumps({'origin':'Built-in image_gen; flat material atlas generated for geometric rooms, no whole-room pictures','runtime_size':[1024,512],'grid':[4,2],'tiles':['warm plaster','cool plaster','worn wood','brushed metal','quilted fabric','woven rug','book spines','monitor screen'],'runtime_gpu_bytes_with_mips':2796204,'packing':'tools/pack_newland_materials.py; Lanczos downsample, RGBA PNG embedded in renderer','files':[{'path':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(A.glob('*.png'))]},indent=2)+'\n')
print('Embedded geometric-room material atlas:',len(data),'bytes')
