"""The traced plate family publishes only matte material maps in both editions."""
import argparse,json,re
from pathlib import Path
import numpy as np
from PIL import Image

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime-dir',type=Path)
args=parser.parse_args()
pack=root/'material-packs/ao-cyber-icc'
family=json.loads((pack/'deck-family.json').read_text())
manifest=json.loads((pack/'manifest.json').read_text())
members={x['id']:x for x in family['members']}
published={x['id']:x for x in manifest['materials'] if x['type']==1010006}
header=(root/'rvk/terrain_materials.h').read_text().split('inline bool DeckGround')[1].split('inline bool AuthoredGround')[0]
assert set(map(int,re.findall(r'case (\d+):',header)))==members.keys()
for identity in members:
    item=published[identity]
    assert item['maps']==['orm'],(identity,'repaint or noisy normal republished')
    assert item['source'] is None
    stem=f'1010006_{identity}_'
    assert {x[len(stem):] for x in manifest['files'] if x.startswith(stem)}=={'orm.png'},identity
    orm=np.array(Image.open(pack/'maps'/f'{stem}orm.png').convert('RGB')).astype(float)/255
    assert np.all(orm[:,:,0]==1),identity
    assert orm[:,:,1].min()>=.815 and orm[:,:,1].max()<=.945,identity
    assert orm[:,:,2].max()<=.705 and orm[:,:,2].max()>=.695,identity
    if members[identity]['shape']!='full':assert orm[:,:,2].min()==0,identity
# A diagonal joining asphalt, its inverse, and an inset plate preserve soil.
a=np.array(Image.open(pack/'maps/1010006_176_orm.png'))
b=np.array(Image.open(pack/'maps/1010006_186_orm.png'))
assert a[16,16,2]>170 and a[112,112,2]==0
assert b[16,16,2]==0 and b[112,112,2]>170
c=np.array(Image.open(pack/'maps/1010006_183_orm.png'))
assert c[64,64,2]>170 and c[8,8,2]==0
code=(root/'tools/package-release.py').read_text()
assert "{x['id'] for x in deck['members']}" in code, 'Lite omitted floor transitions'
if args.runtime_dir:
    attached=set()
    for path in args.runtime_dir.glob('loader-*.log'):
        log=path.read_text(errors='replace')
        assert 'presentation released' in log,path
        assert not re.search(r'VUID-|VK_ERROR|device lost|decode failed',log,re.I),path
        for kind,identity,detail in re.findall(r'RDB texture (1010006|1010021|1010022):(\d+) gets([^\r\n]+)',log):
            identity=int(identity)
            if identity not in members:continue
            assert '[PBR material ' in detail and '[albedo map]' not in detail and '[normal map]' not in detail,(path,identity,detail)
            attached.add((int(kind),identity))
    assert {i for kind,i in attached if kind==1010006}==members.keys(),('missing runtime floor material',attached)
    assert {(1010021,i) for i in [17,174,176,186]}<=attached,'quality aliases did not load'
print(json.dumps({'family_members':len(members),'original_albedo_and_tread':'retained',
                  'additional_normal_and_height':'excluded','roughness':.82,'metallic':.70,
                  'transition_material_masks':'passed','full_and_lite_selection':'passed'},indent=2))
