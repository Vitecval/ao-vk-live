"""Exercise shared dependency closure and explicit material requirements."""
import copy,importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('families',Path(__file__).resolve().parents[1]/'tools/build-building-families.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def family(i,shared=(),maps=('orm',)):
 return {'id':i,'key':str(i),'name':str(i),'texture_ids':[i*10],'shared_texture_ids':list(shared),'retained_functional_ids':[999], 'resources':[{'id':i*10,'pack':'ao-cyber-icc','maps':list(maps)}],'models':[{'mesh_id':i,'textures':[i*10,999,*shared]}]}
x={'families':[family(1),family(2,[10],('d','n','orm','e','h')),family(3,[20]),family(4)]}
r,mask=m.resolve(x)
assert r[10]['maps']==4 and r[20]['maps']==31
assert mask[1]==mask[2]==mask[3]==14 and mask[4]==16
pending=copy.deepcopy(x);pending['families'][0]['pending_companion_ids']=[998];pending['families'][0]['models'][0]['textures'].append(998)
assert m.resolve(pending)==(r,mask),'Queued original companions must not become approved resources'
for mutation in ['missing-owner','missing-map-declaration','duplicate-owner','unknown-map','uncovered-model']:
 bad=copy.deepcopy(x)
 if mutation=='missing-owner':bad['families'][1]['shared_texture_ids'].append(998)
 if mutation=='missing-map-declaration':bad['families'][0]['resources'][0]['id']=998
 if mutation=='duplicate-owner':bad['families'][1]['texture_ids']=[10];bad['families'][1]['resources'][0]['id']=10
 if mutation=='unknown-map':bad['families'][0]['resources'][0]['maps']=['oops']
 if mutation=='uncovered-model':bad['families'][0]['models'][0]['textures'].append(997)
 try:m.resolve(bad)
 except AssertionError:pass
 else:raise AssertionError(('Invalid family accepted',mutation))
print('Shared dependency closure, independent families, ORM-only and required maps: passed')
