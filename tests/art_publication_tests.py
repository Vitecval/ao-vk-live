import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('publication',Path(__file__).resolve().parents[1]/'tools/art_publication.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
plan={'architecture':{'new':[1,2,17824]},'tiny':{'new':[3,16128]},'complete':{'new':[4,5]}}
status=m.publication_status(plan,[1,2,3,4],[5],[16128])
assert not status['architecture']['complete'] and status['architecture']['pending_art_ids']==[17824]
assert status['tiny']['complete'] and status['tiny']['deliberately_excluded_ids']==[16128]
assert status['complete']['complete']
assert not m.publication_status(plan,[1,2,3,4],[5])['tiny']['complete']
print('Whole-batch completeness, unfinished extracted inputs, explicit tiny exclusions: passed')
