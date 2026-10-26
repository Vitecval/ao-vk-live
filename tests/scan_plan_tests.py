"""Old/new scan parsing, type aliases, screen visibility and honest worklist coverage."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory(prefix='scan-audit-') as td:
        d=Path(td)
        index={'1010004:7':0,'1010011:7':1,'1010006:12':2,'1010009:99':3}
        (d/'index.json').write_text(json.dumps(index));(d/'names.json').write_text(json.dumps({'1010004:7':'rock.png'}))
        (d/'rvk-frame-one.txt').write_text('# scene 800x600\nD 0: visual RTriMesh_t (1) tex0 64x64 rdb:7 type:1010016 | rect 0,0-50,50\nD 1: visual AnarchyGround_t (4) tex0 64x64 rdb:12 | rect 900,0-1000,100\nD 2: visual RTriMesh_t (1) tex0 64x64 rdb:99 | rect 10,10-40,40\nD 3: visual RTriMesh_t (1) tex0 64x64 rdb:7 | rect 10,10-40,40\nD 4: visual RCATMesh_t (2) tex0 64x64 rdb:0 | rect 10,10-40,40\n')
        (d/'randy-vk.log').write_text('pick: RDB texture 1010021:12\npick: RDB texture 1010019:7\n')
        out=d/'audit.json'
        subprocess.run([sys.executable,str(ROOT/'tools/audit-texture-scans.py'),'--client',str(d),'--names',str(d/'names.json'),'--index',str(d/'index.json'),'--output',str(out)],check=True,capture_output=True)
        report=json.loads(out.read_text());items={(x['type'],x['id']):x for x in report['textures']}
        assert len(items)==4
        assert items[1010004,7]['onscreen_captures']==1
        assert items[1010006,12]['onscreen_captures']==0
        assert items[1010006,12]['picks']==1 and items[1010011,7]['picks']==1
        assert items[1010009,99]['name']=='' # unnamed room pixels must not vanish
        assert len(report['runtime_draws'])==1 and len(report['unresolved'])==1
        plan=d/'plan.json'
        subprocess.run([sys.executable,str(ROOT/'tools/plan-texture-rework.py'),str(out),'--output',str(plan)],check=True,capture_output=True)
        result=json.loads(plan.read_text())
        assert {(x['type'],x['id']) for x in result['textures']}==set(items)
        assert result['resolved_ids']==4 and result['textures'][0]['phase']==1
    print('Scan plan: aliases, ambiguous/unnamed/runtime textures, offscreen filtering, full worklist coverage passed')

if __name__=='__main__':main()
