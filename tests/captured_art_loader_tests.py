"""Verify a captured-art batch through the production proxy's asynchronous loader.

DIRECTORY contains loader-groups.json (Name/Type/Items/Flag), matching .log/.txt
files from randy_harness --art-ids / --equipment-ids, and pressure.log/.txt.
Use small groups so rate-limited diagnostic lines cover every resource identity.
"""
import argparse,json,re
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path);r=p.parse_args().directory
    groups=json.loads((r/'loader-groups.json').read_text(encoding='utf-8-sig'))
    checked=set();repaints=set();aliases=0
    for group in groups:
        name=group['Name'];kind=group['Type']
        log=(r/(name+'.log')).read_text(errors='replace')
        assert 'presentation released' in log,name
        assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|decode failed',log,re.I),name
        counts=re.findall(r'rvk: textures [\d.]+ MB \((\d+)\)',log)
        assert counts and all(int(x)<10000 for x in counts),(name,'texture accounting overflow')
        for item in group['Items']:
            identity=item['id']
            line=re.search(rf'materials: RDB texture {kind}:{identity} gets[^\r\n]+',log)
            assert line,(name,identity,'missing authored material')
            if 'd' in item.get('maps',['d']):
                assert '[albedo map]' in line[0],(name,identity,'missing repaint')
                assert '[original alpha]' in line[0],(name,identity,'lost original coverage/glow')
            else:assert '[PBR material ' in line[0],(name,identity,'missing material-only profile')
            canonical=1010004 if kind in (1010016,1010017) else kind
            assert f'RDB texture {canonical}:{identity} from nothing (maps removed)' in log,(name,identity,'toggle did not remove maps')
            if kind in (1010016,1010017):aliases+=1
            else:
                checked.add((kind,identity))
                if 'd' in item.get('maps',['d']):repaints.add((kind,identity))
    pressure=(r/'pressure.log').read_text(errors='replace')
    assert 'low process address space' in pressure
    assert '[albedo map]' in pressure and 'presentation released' in pressure
    assert 'reservations released' in (r/'pressure.txt').read_text()
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|decode failed',pressure,re.I)
    print(json.dumps({'authored_identities_loaded':len(checked),'repaint_identities_loaded':len(repaints),'material_only_identities_loaded':len(checked-repaints),'quality_aliases_loaded':aliases,
        'all_original_alpha_preserved':True,'style_and_equipment_toggles':'passed',
        'low_address_space_recovery':'passed','clean_shutdown':'passed'},indent=2))

if __name__=='__main__':main()
