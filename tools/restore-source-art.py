"""Restore byte-identical derived crops omitted from compact source exports.

Requires Pillow. No game assets, network or image generation are used.
"""
import argparse,hashlib,io,json
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true',help='Verify reproducibility without writing files')
    args=parser.parse_args();atlases={};count=0
    for name,item in json.loads((ROOT/'release/derived-art.json').read_text()).items():
        path=ROOT/name;atlas=ROOT/item['atlas']
        if not path.resolve().is_relative_to(ROOT/'material-packs') or not atlas.resolve().is_relative_to(ROOT/'material-packs'):
            raise ValueError('Invalid artwork path')
        if atlas not in atlases:atlases[atlas]=Image.open(atlas).convert('RGB')
        crop=atlases[atlas].crop(item['box']).resize(tuple(item['size']),Image.Resampling.LANCZOS)
        result=io.BytesIO();crop.save(result,format='PNG');data=result.getvalue()
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise RuntimeError(f'Crop is not byte-identical: {name}')
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:
            raise RuntimeError(f'Existing artwork differs; preserved: {name}')
        if not args.check and not path.exists():path.write_bytes(data)
        count+=1
    print(f'{count} derived crops verified'+(' (no files written)' if args.check else ' and restored where absent'))

if __name__=='__main__':main()
