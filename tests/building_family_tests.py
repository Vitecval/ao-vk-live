"""Validate model dependency coverage and atomic building-map presentation.

Optional DIRECTORY holds rvk_demo --building-family-test modes 0..5, plus
family-art-0..5 renders using the installed production artwork.
"""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'material-packs/ao-cyber-world'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,nargs='?')
    parser.add_argument('--loader-directory',type=Path)
    args=parser.parse_args()
    manifest=json.loads((ROOT/'material-packs/building-families.json').read_text())
    artwork=json.loads((PACK/'family-catalog.json').read_text())+json.loads((PACK/'rubika-catalog.json').read_text())
    art={x['id']:x for x in artwork}
    assert len(art)==len(artwork)
    packs={name:json.loads((ROOT/'material-packs'/name/'manifest.json').read_text()) for name in ['ao-cyber-world','ao-cyber-icc']}
    spec=importlib.util.spec_from_file_location('families',ROOT/'tools/build-building-families.py')
    generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)
    resources,masks=generator.resolve(manifest)
    seen=set();models=0
    for family in manifest['families']:
        textures=set(family['texture_ids']);retained=set(family['retained_functional_ids']);shared=set(family.get('shared_texture_ids',[]))
        assert textures and not textures&seen and not textures&retained
        seen|=textures
        for mesh in family['models']:
            pending=set(family.get('pending_companion_ids',[]))
            assert set(mesh['textures'])<=textures|retained|shared|pending,(family['key'],mesh['mesh_id'])
            models+=1
        for identity in textures:
            resource=resources[identity];pack=ROOT/'material-packs'/resource['pack'];baked=packs[resource['pack']]
            for suffix,bit in generator.MAP_BITS.items():
                if not resource['maps']&bit:continue
                name=f'1010004_{identity}_{suffix}.png';path=pack/'maps'/name
                assert hashlib.sha256(path.read_bytes()).hexdigest()==baked['files'][name],(identity,suffix,resource['pack'])
    assert set(art)<=seen,'Family artwork lost eligibility'
    for item in artwork:
        assert max(item['size'])<=512
        assert (PACK/'source-art'/(item['source']+'.png')).is_file()
        assert (PACK/'source-art'/(item['atlas_source']+'.png')).is_file()
        for suffix in ['d','n','orm']:
            path=PACK/'maps'/f"1010004_{item['id']}_{suffix}.png"
            expected=item['size'] if suffix=='d' else [v//2 for v in item['size']]
            assert list(Image.open(path).size)==expected,(item['id'],suffix)
    icc_world=[x for x in packs['ao-cyber-icc']['materials'] if x['type']==1010004]
    for item in icc_world:
        assert item['id'] in seen,('Earlier ICC material disabled',item['id'])
        if 'd' in item['maps']:assert resources[item['id']]['pack']=='ao-cyber-icc',('Premium ICC art replaced',item['id'])
    for identity in [6050,6055,99647,99648]:assert identity in seen,('Earlier neutral art disabled',identity)
    assert resources[297326]['maps']&8,'ICC panel emission lost'
    assert any(x['maps']==4 for x in resources.values()),'Material-only ICC support lost'
    assert masks[1]==masks[3]==masks[4]==masks[5]==masks[6]==masks[7]==masks[8]
    for family in manifest['families']:
        assert masks[family['id']]&(1<<family['id'])
        for identity in family.get('shared_texture_ids',[]):assert masks[family['id']]==masks[resources[identity]['family']]
    for family in manifest['families']:
        if family['key']=='omni-hq-exteriors':
            assert len(family['models'])==126
            assert {6469,121466,51296}<=set(family['retained_functional_ids']),'Original branding lost'
    metal=[x for x in artwork if x['profile']=='architectural-metal']
    assert len(metal)>150,'Large Omni metal batch missing'
    for item in metal:
        orm=np.asarray(Image.open(PACK/'maps'/f"1010004_{item['id']}_orm.png"))
        assert (orm[:,:,2]>120).mean()>.05,item['id'] # glass inserts leave a bounded metal frame
    comparisons={}
    if args.directory:
        def pixels(name):return np.asarray(Image.open(args.directory/name/'result.bmp').convert('RGB')).astype(float)
        for prefix in ['family','family-art']:
            if not (args.directory/(prefix+'-0')/'result.bmp').exists():continue
            original=pixels(prefix+'-0');ready=pixels(prefix+'-2')
            for mode in [3,5]:
                delta=float(abs(ready-pixels(f'{prefix}-{mode}')).max())
                assert delta<=1,(prefix,mode,'Resident artwork hidden by aggregate readiness',delta)
                comparisons[f'{prefix}-{mode}_ready_delta']=delta
            assert abs(ready-original).mean()>2,(prefix,'Completed artwork did not render')
            assert abs(ready-pixels(prefix+'-4')).max()<=1,(prefix,'Personal override affected by family gate')
            for mode in range(6):
                log=(args.directory/f'{prefix}-{mode}'/'run.txt').read_text(errors='replace')
                assert 'rendered; screenshot result.bmp' in log
                assert not re.search(r'VK_ERROR|VUID-|Validation Error|device lost|allocation failed',log,re.I)
    loader_checks={}
    if args.loader_directory:
        directory=args.loader_directory
        groups=json.loads((directory/'loader-groups.json').read_text(encoding='utf-8-sig'))
        membership={i:f['id'] for f in manifest['families'] for i in f['texture_ids']}
        for group in groups:
            if group['Flag']=='--equipment-ids':continue
            log=(directory/(group['Name']+'.log')).read_text(errors='replace')
            for family in {membership[x['id']] for x in group['Items']}:
                assert f'building family {family}: resident replacements ready' in log,(group['Name'],family)
            for item in group['Items']:
                assert re.search(rf"RDB texture {group['Type']}:{item['id']} gets[^\r\n]*{item['pack']}",log),(group['Name'],item['id'],'Wrong artwork provider')
        for mode in ['missing','corrupt']:
            log=(directory/(mode+'.log')).read_text(errors='replace')
            assert 'presentation released' in log
            assert 'building family 1: resident replacements ready' in log,mode
            assert 'RDB texture 1010004:5942 gets' not in log,mode
            for other in [2,9]:
                assert f'building family {other}: resident replacements ready' in log,(mode,other)
            assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I)
        assert 'incomplete files; originals retained' in (directory/'missing.log').read_text()
        assert 'unknown image type' in (directory/'corrupt.log').read_text()
        personal=(directory/'personal.log').read_text(errors='replace')
        assert re.search(r'RDB texture 1010004:5942 gets[^\r\n]*materials[^\r\n]*\[albedo map\]',personal)
        assert 'building family 1: resident replacements ready' in personal
        assert 'presentation released' in personal
        for name in ['icc-shadow','icc-missing-emission']:
            log=(directory/(name+'.log')).read_text(errors='replace')
            assert 'presentation released' in log
            assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I)
        shadow=(directory/'icc-shadow.log').read_text(errors='replace')
        for identity in [297326,297327,16085,295444,6254]:
            assert re.search(rf'RDB texture 1010004:{identity} gets[^\r\n]*ao-cyber-icc',shadow),(identity,'ICC source shadowed')
        assert '[original alpha]' in re.search(r'RDB texture 1010004:297326 gets[^\r\n]*',shadow)[0]
        missing=(directory/'icc-missing-emission.log').read_text(errors='replace')
        assert 'building family 4: incomplete files; originals retained' in missing
        for family in [3,4,5]:assert f'building family {family}: resident replacements ready' in missing
        for family in [2,9]:
            assert f'building family {family}: resident replacements ready' in missing
        loader_checks={'complete_groups_and_priority':'passed','missing_and_corrupt_family_fallback':'passed',
                       'independent_families':'passed','restored_ICC_and_neutral_art':'passed','personal_override':'passed',
                       'ICC_emission_required_and_explicit_provider':'passed'}
    print(json.dumps({'approved_resources':len(seen),'family_companion_repaints':len(art),'inventoried_models':models,
        'architecture_metal_surfaces':len(metal),'model_dependencies':'inventoried; original functional inputs and queued shared companions are explicit',
        'pending_shared_companion_identities':len({i for f in manifest['families'] for i in f.get('pending_companion_ids',[])}),
        'atomic_presentation_pixel_checks':comparisons,'production_loader':loader_checks,'live_model_fit':'pending user test'},indent=2))


if __name__=='__main__':main()
