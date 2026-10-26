#!/usr/bin/env python3
"""Inventory every saved texture scan and pick, without copying game artwork.

Frame dumps before type-aware diagnostics contain only resource IDs. Resolve
those against the local index; preserve ambiguity and runtime-only draws instead
of silently discarding them. Reports are private and never packaged.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ALIASES = {1010016:1010004,1010017:1010004,1010021:1010006,
           1010022:1010006,1010019:1010011,1010020:1010011}
TYPES = (1010004,1010006,1010011,1010009)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--client',type=Path,required=True)
    p.add_argument('--names',type=Path,required=True)
    p.add_argument('--index',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    names=json.loads(a.names.read_text()); index=json.loads(a.index.read_text())
    by_id=defaultdict(list)
    for k in index:
        t,i=map(int,k.split(':'))
        if t in TYPES: by_id[i].append(t)
    art={}; material=set()
    for path in (ROOT/'material-packs').glob('*/manifest.json'):
        for x in json.loads(path.read_text()).get('materials',[]):
            key=(x['type'],x['id']);material.add(key)
            if {'d','albedo'} & set(x.get('maps',[])):art[key]=path.parent.name
    frames=defaultdict(set); visible=defaultdict(set); draws=Counter()
    contexts=defaultdict(Counter); picks=Counter(); files=[]; unresolved=[]; runtime=[]
    for path in sorted(a.client.glob('rvk-frame-*.txt')):
        raw=path.read_bytes()
        files.append({'name':path.name,'sha256':hashlib.sha256(raw).hexdigest()})
        viewport=re.search(r'scene (\d+)x(\d+)',raw[:500].decode(errors='replace'))
        width,height=map(int,viewport.groups()) if viewport else (0,0)
        for line in raw.decode(errors='replace').splitlines():
            if not re.match(r'D \d+:',line):continue
            context=re.search(r'visual (.*?) \(',line)
            context=context[1] if context else 'unlabelled draw'
            for stage,w,h,identity,explicit_type in re.findall(r'tex(\d) (\d+)x(\d+) rdb:(\d+)(?: type:(\d+))?',line):
                identity=int(identity)
                if identity==0:
                    runtime.append({'frame':path.name,'context':context,'stage':int(stage),'size':[int(w),int(h)]})
                    continue
                t=int(explicit_type) if explicit_type else 0
                types=[ALIASES.get(t,t)] if t else by_id.get(identity,[])
                if len(types)!=1:
                    unresolved.append({'frame':path.name,'id':identity,'candidate_types':types,'context':context})
                    continue
                key=(types[0],identity);frames[key].add(path.name);draws[key]+=1
                contexts[key][context]+=1
                rect=re.search(r'rect (-?\d+),(-?\d+)-(-?\d+),(-?\d+)',line)
                onscreen='offscreen' not in line
                if rect and width:
                    x0,y0,x1,y1=map(int,rect.groups())
                    onscreen=onscreen and x1>0 and y1>0 and x0<width and y0<height
                if onscreen: visible[key].add(path.name)
    for path in sorted(a.client.glob('randy-vk*.log')):
        # Stream even very large logs: picks must not disappear at a size cutoff.
        for line in path.open(errors='replace'):
            m=re.search(r'pick: RDB texture (\d+):(\d+)',line)
            if m:
                t,i=map(int,m.groups());picks[ALIASES.get(t,t),i]+=1
    result=[]
    for t,i in set(frames)|set(picks):
        key=t,i;name=names.get(f'{t}:{i}','')
        result.append({'type':t,'id':i,'name':name,'picks':picks[key],
                       'captures':sorted(frames[key]),'onscreen_captures':len(visible[key]),
                       'draws':draws[key],'contexts':dict(contexts[key]),
                       'status':'repainted' if key in art else 'material-only' if key in material else 'pending',
                       'pack':art.get(key)})
    result.sort(key=lambda x:(not bool(x['picks']),-x['onscreen_captures'],-x['draws'],x['id']))
    report={'scan_files':files,'textures':result,'unresolved':unresolved,
            'runtime_draws':runtime,'summary':{'scans':len(files),'unique_resolved_textures':len(result),
            'statuses':dict(Counter(x['status'] for x in result)),
            'runtime_draws':len(runtime),'unresolved_occurrences':len(unresolved)}}
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['summary'],indent=2))

if __name__=='__main__':main()
