#!/usr/bin/env python3
"""Audit actual repaint coverage separately from material-only profiles.

Optionally supply local extracted --index and --names JSON dictionaries to build
a private authoring queue. Game pixels are never read. Quality-copy tables are
excluded: only full-quality world, character and terrain IDs are counted.
"""
import argparse
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPES = {1010004: 'world-and-equipment', 1010011: 'character', 1010006: 'terrain'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index', type=Path)
    parser.add_argument('--names', type=Path)
    parser.add_argument('--queue', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    art, materials, packs = set(), set(), []
    for path in sorted((ROOT / 'material-packs').glob('*/manifest.json')):
        manifest = json.loads(path.read_text())
        entries = manifest.get('materials', [])
        painted = {(x['type'], x['id']) for x in entries if {'d', 'albedo'}.intersection(x.get('maps', []))}
        art.update(painted)
        materials.update((x['type'], x['id']) for x in entries)
        packs.append({'pack': manifest['name'], 'repaint_ids': len(painted), 'material_ids': len(entries)})
    catalog = json.loads((ROOT / 'material-packs/ao-character-materials/catalog.json').read_text())
    # The inferred catalog is metadata only, and is deliberately not added to art.
    inferred_entries = catalog['materials'] if isinstance(catalog, dict) and 'materials' in catalog else catalog
    inferred_ids = {(x['type'], x['id']) for x in inferred_entries}
    inferred = len(inferred_entries)
    report = {'actual_repaint_ids': len(art), 'authored_material_only_ids': len(materials - art),
              'inferred_material_assignments': inferred, 'packs': packs,
              'scope': 'Existing model textures; original geometry and animations. UI and effects require separate review.',
              'live_review': 'Character atlas fit pending; actual rework is incomplete.'}
    family_path=ROOT/'material-packs/building-families.json'
    if family_path.exists():
        families=json.loads(family_path.read_text())['families']
        approved={i for f in families for i in f['texture_ids']}
        managed={x['id'] for x in json.loads((ROOT/'material-packs/ao-world-materials/catalog.json').read_text())}
        managed|={i for f in families for i in f['retained_functional_ids']}
        managed|={i for f in families for i in f.get('pending_companion_ids',[])}
        managed|={x['id'] for x in json.loads((ROOT/'material-packs/scan-coverage.json').read_text())['textures'] if x['type']==1010004 and x['phase']==2}
        eligible={(kind,i) for kind,i in art if kind!=1010006 and (kind!=1010004 or i not in managed or i in approved)}
        report['retained_repaint_ids']=len(art)
        report['eligible_repaint_ids']=len(eligible)
        report['eligibility_scope']='Identity eligibility before selected style, character switch and live loading; manufactured ground keeps original RGB with upgraded normal/ORM maps.'
        report['building_family_surfaces']=sum(len(f['texture_ids']) for f in families)
        report['building_family_models']=sum(len(f['models']) for f in families)
        report['pending_shared_companion_identities']=len({i for f in families for i in f.get('pending_companion_ids',[])})
        report['building_families']=[{'key':f['key'],'surfaces':len(f['texture_ids']),'models':len(f['models']),'live_review':f['live_review']} for f in families]
    world = ROOT / 'material-packs/ao-world-materials/catalog.json'
    if world.exists():
        world_entries = json.loads(world.read_text())
        inferred_ids.update((x['type'],x['id']) for x in world_entries)
        report['named_architecture_profiles'] = len(world_entries)
        report['global_terrain'] = 'Every actual terrain draw; detail/material fallback, not per-tile repaints'
    if args.index:
        index = json.loads(args.index.read_text())
        names = json.loads(args.names.read_text()) if args.names else {}
        queue = []
        for key in index:
            parts = key.split(':')
            if len(parts) != 2:
                continue
            kind, identity = map(int, parts)
            if kind not in TYPES:
                continue
            queue.append({'type': kind, 'id': identity, 'name': names.get(key, ''),
                          'family': TYPES[kind], 'status': 'repaint-preview' if (kind, identity) in art
                          else 'authored-material-only' if (kind, identity) in materials
                          else 'inferred-material-only' if (kind, identity) in inferred_ids else 'pending'})
        queue.sort(key=lambda x: (x['type'], x['id']))
        report['indexed_full_quality_ids'] = len(queue)
        report['queue_counts'] = dict(Counter(x['status'] for x in queue))
        report['families'] = dict(Counter(x['family'] for x in queue))
        if args.queue:
            args.queue.write_text(json.dumps(queue, indent=2) + '\n')
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
