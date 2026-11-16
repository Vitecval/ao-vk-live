"""Freeze compact manifests and a code-drawn Horizon icon for the local app."""
from pathlib import Path
import gzip,hashlib,json,math,struct,zlib,zipfile
ROOT=Path(__file__).resolve().parents[1];A=ROOT/'apps/horizon/Assets'
metadata=json.loads((A/'engine.json').read_text(encoding='utf8'))
BASE=ROOT/('build/preview24/release-r'+str(metadata['revision']))
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
for edition in ['lite','full-setup']:
 source=BASE/f'ao-vk-live-0.6.0-preview.24-windows-x86-{edition}'/'manifest.json'
 if source.exists():(A/('lite-manifest.json.gz' if edition=='lite' else 'full-manifest.json.gz')).write_bytes(gzip.compress(source.read_bytes(),mtime=0))
core=BASE/'ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip'
if core.exists():
 # Fresh source builds need only the published bootstrap and the checked-in
 # pins, not a private packaging audit from the original build machine.
 assert sha(core)==metadata['fullSetupSha256'] and core.stat().st_size==metadata['fullSetupBytes'], 'The graphics bootstrap does not match its checked-in integrity pin.'
 with zipfile.ZipFile(core) as archive:
  files={e.filename.split('/',1)[1]:hashlib.sha256(archive.read(e)).hexdigest() for e in archive.infolist() if not e.is_dir()}
 (A/'core-files.json').write_text(json.dumps(files,indent=2),encoding='utf8')
 full=json.loads(gzip.decompress((A/'full-manifest.json.gz').read_bytes()))
 (A/'temporary-assets.txt.gz').write_bytes(gzip.compress(('\n'.join(p for p in full['assets'] if p not in files)+'\n').encode(),mtime=0))
# Original vector mark: a gold sun above two horizon lines on a dark disc.
# Render analytically into the Windows ICO; no third-party image tools/assets.
n=128;raw=bytearray()
for y in range(n):
 raw.append(0)
 for x in range(n):
  channels=[0.,0.,0.,0.]
  for sy in [.125,.375,.625,.875]:
   for sx in [.125,.375,.625,.875]:
    px=(x+sx)*2;py=(y+sy)*2
    alpha=255 if math.hypot(px-128,py-128)<126 else 0
    mark=(py<=130 and abs(math.hypot(px-128,py-126)-79)<3) or (22<px<234 and abs(py-132)<3) or (72<px<184 and abs(py-164)<2)
    rgb=(216,185,130) if mark else (16,31,39)
    for k,c in enumerate((*rgb,alpha)):channels[k]+=c/16
  raw.extend(round(v) for v in channels)
def chunk(name,data):return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',n,n,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
(A/'horizon.ico').write_bytes(struct.pack('<HHH',0,1,1)+struct.pack('<BBBBHHII',n,n,0,0,1,32,len(png),22)+png)
assert (A/'engine.json').exists() and (A/'settings.json').exists()
print('Horizon manifests, engine pin and original icon are ready.')
