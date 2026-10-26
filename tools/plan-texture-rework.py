#!/usr/bin/env python3
"""Turn a private scan audit into a reproducible, frequency-ranked art worklist.

Only resource identity/name metadata and counts are exported. No pixels, player
names, window titles, log paths or original game assets enter the public plan.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
PHASES={1:'terrain',2:'buildings',3:'props-and-foliage',4:'armour-and-equipment',5:'characters-and-creatures',6:'effects-review'}
UTILITY={'blacktexture.png','whitemap8x8.png','noise.png'}

def phase(x):
    name=x['name'].lower();contexts=' '.join(x.get('contexts',{}))
    if x['type']==1010006:return 1
    if name in UTILITY:return 6
    if re.search(r'^head_|^face|naked|default_skin|^hair|_hair\.|^pumkinman',name):return 5
    if re.search(r'armou?r|helmet|shoulder|_pads_|cloak_|^arms_|^feet_|^legs_|^body_|rifle|pistol|backpiece|wristpad|shotgun|blaster|^weapon_|proj_weapon|^ai_.*_(?:arms|body|feet|legs)',name):return 4
    if 'RCAT' in contexts and 'RTriMesh' not in contexts:return 5
    if re.search(r'gm_aologo|massive_screen_effect|redbeamglow|^scrolling_',name):return 6
    if x['type']==1010009:return 2
    if 'RTriMesh' not in contexts and re.search(r'Gfx|VisualSun|VisualSingleCloud|VisualLiquid|GenericMeshObject',contexts):return 6
    if re.search(r'reflection|fakefog|^single_cloud|^newsun|^vein\d|^noise\.|bank_glow|glow_.*grid',name):return 6
    if not re.search(r'leaf|leaves|bark|shrub|grass|foliage|foilage|tree',name) and re.search(r'building|window|windpw|wall|roof|facade|door|ceiling|pillar|column|tower|bridge|jobe|redeemed|temple|midtech|hitech|plateyell|tile|floor|paving|pavement|asphalt|street',name):return 2
    return 3

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('audit',type=Path);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();audit=json.loads(a.audit.read_text())
    painted={}
    for path in (ROOT/'material-packs').glob('*/manifest.json'):
        for x in json.loads(path.read_text()).get('materials',[]):
            if {'d','albedo'}&set(x.get('maps',[])):painted[x['type'],x['id']]=path.parent.name
    queue=[]
    for x in audit['textures']:
        p=phase(x);key=x['type'],x['id']
        status='repainted' if key in painted else 'retain-functional' if x['name'].lower() in UTILITY else 'pending-review' if p==6 else 'pending-art'
        queue.append({'type':x['type'],'id':x['id'],'name':x['name'],'phase':p,'category':PHASES[p],
                      'scan_frequency':x['onscreen_captures'],'draw_sightings':x['draws'],'picks':x['picks'],
                      'priority_score':round(x['onscreen_captures']*10+min(math.log2(x['draws']+1),12)+min(x['picks'],3),3),
                      'status':status,'pack':painted.get(key),
                      **({'reason':'Constant colour or noise input; changing it would alter compositing/effects rather than add surface detail.'} if status=='retain-functional' else {})})
    queue.sort(key=lambda x:(x['phase'],-x['priority_score'],x['type'],x['id']))
    report={'scope':'All resolved texture identities in the saved scan snapshot; not the entire AO database.',
            'ranking':'User category order, then observed on-screen scan frequency. Draw/pick counts break ties. These are sample reuse estimates, not measured global game usage.',
            'scans':len(audit['scan_files']),'resolved_ids':len(queue),
            'runtime_draws_without_id':len(audit['runtime_draws']),
            'unresolved_occurrences':len(audit['unresolved']),
            'counts':dict(Counter(x['status'] for x in queue)),
            'phases':[{ 'phase':p,'category':name,'counts':dict(Counter(x['status'] for x in queue if x['phase']==p))} for p,name in PHASES.items()],
            'textures':queue}
    if audit.get('ranking_snapshot_merge'):
        report['historical_inventory_merge']=audit['ranking_snapshot_merge']
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='textures'},indent=2))

if __name__=='__main__':main()
