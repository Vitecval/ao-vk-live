#!/usr/bin/env python3
"""Check every staged colour map for native fidelity and small-export regressions.

Run against a complete staged pack tree. File dimensions do not certify artistic
quality; the separate provenance records distinguish recovery, technical detail
reconstruction and independent renders. Functional exceptions are hash pinned.
"""
import argparse,hashlib,importlib.util,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('packs',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 spec=importlib.util.spec_from_file_location('resolution',ROOT/'tools/check-material-resolution.py');gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
 ledger=json.loads(gate.DEFAULT_LEDGER.read_text())['sizes']
 records=json.loads((ROOT/'material-packs/preview23-detail-restoration.json').read_text())
 exceptions={(x['pack'],x['identity']):x for x in records['functionalEffectExceptions']}
 count=0;errors=[];small=[]
 for path in sorted(a.packs.rglob('*_d.png')):
  match=re.fullmatch(r'(\d+)_(\d+)_d.png',path.name)
  if not match:continue
  kind,ident=map(int,match.groups());identity=f'{kind}:{ident}';key=path.parent.name,identity
  try:
   size=gate.png_size(path);native=ledger[f'{gate.canonical(kind)}:{ident}']
   if size[0]<native[0] or size[1]<native[1]:raise ValueError(f'Export {size} smaller than native {native}')
   exception=exceptions.get(key)
   gate.require_detail_size(identity,size,exception)
   if max(size)<512:
    with path.open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
    if actual!=exception.get('sha256'):raise ValueError('Functional input changed without review')
    small.append({'identity':identity,'pack':path.parent.name,'reason':exception['reason']})
   count+=1
  except (KeyError,ValueError,OSError) as e:errors.append({'path':str(path),'error':str(e)})
 if not count:errors.append({'error':'Empty input cannot pass'})
 result={'result':'failed' if errors else 'passed','colourMapsChecked':count,'functionalSmallInputs':small,'errors':errors,'allArtworkArtisticallyCertified':False}
 a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(result,indent=2)+'\n');print(result['result'],count,'colour maps',len(errors),'errors')
 return bool(errors)
if __name__=='__main__':raise SystemExit(main())
