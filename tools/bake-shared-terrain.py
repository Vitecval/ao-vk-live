#!/usr/bin/env python3
"""Select five previously generated master surfaces for continuous terrain detail.

Only generated pack artwork is read. Original AO tiles remain in the client and
provide the palette/transition pattern at runtime; no transition art is painted.
"""
import hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    dest=ROOT/'rvk/assets/terrain';world=ROOT/'material-packs/ao-cyber-world/maps'
    entries=[]
    for name,identity in [('grass',258),('dirt',8),('gravel',19),('sand',2),('rock',473)]:
        source=world/f'1010006_{identity}_d.png';target=dest/f'shared-{name}.png'
        shutil.copy2(source,target)
        entries.append({'material':name,'source':f'ao-cyber-world/1010006:{identity}',
                        'image':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
    (dest/'shared-materials.json').write_text(json.dumps({'mode':'reuse of built-in image_gen artwork; technical selection only','mapping':'world space, original AO palette/transition masks','materials':entries},indent=2)+'\n')
    print('Five shared master materials selected; no transition variants generated.')
if __name__=='__main__':main()
