"""A/B identical AO cobblestone/rocky-road inputs through classic/native terrain.

Stage images are raw UNORM diagnostics in LDR (no tone mapping); HDR final
images separately compare the displayed result. This does not certify gameplay.
"""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw

p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--validate-only',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(exist_ok=True)
stages={0:'displayed',12:'raw-albedo',13:'vertex-material-diffuse',14:'ambient',15:'directional-local',16:'applied-shadow',17:'raw-lightmap',18:'final-linear',19:'lightmap-modulation-half',20:'lit-diffuse',21:'caster-visibility'}
cases={
 'cobblestone-day':(23,False,False,False),
 'rocky-road-day':(5,True,False,False),
 'cobblestone-night':(23,False,True,False),
 'rocky-road-night':(5,True,True,False),
 'cobblestone-hdr':(23,False,False,True),
 'rocky-road-hdr':(5,True,False,True),
}
metrics={};images={}
for case,(identity,road,night,hdr) in cases.items():
 selected=[0] if hdr else [0,14,17,18] if night else list(stages)
 for stage in selected:
  for mode in [1,2]:
   d=o/f'{case}-mode{mode}-stage{stage}';d.mkdir(exist_ok=True)
   args=['--shared-terrain-test','--terrain-lighting-fixture','--terrain-lighting-occluder','--terrain-pebbles','--terrain-blend',str(mode),'--terrain-id',str(identity),'--terrain-source',str(r/f'build/rubika-overhaul/ground-originals/{identity}.png'),'--size','384','384','--frames','180','--pbr-debug',str(stage),'--shadows','--fixed-clock','1000','--sun-dir','0.55','-0.3','0.45','--shot','result.bmp']
   if identity==23:args+=['--terrain-authored','--shared-ground-maps',str(r/'material-packs/ao-cyber-world/maps')]
   if road:args+=['--terrain-rocky-road']
   if night:args+=['--terrain-lighting-night','--night-dark','0.5']
   if hdr:args+=['--hdr']
   if not a.validate_only:
    with (d/'run.txt').open('w') as log:subprocess.run([str(r/'build/windows-live/rvk_demo.exe'),*args],cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(o/'pipeline.cache')),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
   text=(d/'run.txt').read_text(errors='replace');assert 'rendered; screenshot result.bmp' in text
   assert not re.search(r'VK_ERROR|VUID-|device lost|Validation Error|AddressSanitizer',text,re.I),d
   dump=(d/'terrain-frame.txt').read_text(errors='replace')
   if mode==2:
    assert 'patches/frame 1.00' in dump and 'lighting=classic-AO' in dump and 'ripples=disabled' in dump,d
    assert 'st0 7/2/0 st1 4/2/1' in dump,'Native classic lost the retail ADD/MODULATE stages'
   images[case,stage,mode]=np.array(Image.open(d/'result.bmp').convert('RGB')).astype(float)
  x,y=[images[case,stage,m][100:284,100:284] for m in [1,2]]
  row={'classic_mean':float(x.mean()),'plus_mean':float(y.mean()),'mean_absolute_delta':float(abs(x-y).mean()),'mean_ratio':float(y.mean()/max(x.mean(),.001))}
  if x.mean()<.1:assert y.mean()<.1,(case,stage,row)
  elif stage in [13,14,15,16,17,19,20]:assert row['mean_absolute_delta']<1.25,(case,stage,row)
  elif stage==21:assert row['mean_absolute_delta']<3,(case,stage,row)
  else:assert .95<row['mean_ratio']<1.05,(case,stage,row)
  if stage==0:
   row['baked_dark_lit_halves']=[]
   for region in [np.s_[:,:70],np.s_[:,114:]]:
    left=x[region].mean();right=y[region].mean();ratio=float(right/max(left,.001))
    row['baked_dark_lit_halves'].append({'classic':float(left),'plus':float(right),'ratio':ratio})
    assert .94<ratio<1.06,(case,region,row)
  metrics[f'{case}/{stages[stage]}']=row
  (o/'progress.json').write_text(json.dumps(metrics,indent=2))
  print(case,stages[stage],round(row['mean_ratio'],5),'passed',flush=True)
for surface in ['cobblestone','rocky-road']:
 assert metrics[f'{surface}-night/displayed']['plus_mean']<metrics[f'{surface}-day/displayed']['plus_mean']*.65,'Night response lost'
board=Image.new('RGB',(768,832),'#202020');draw=ImageDraw.Draw(board)
for j,case in enumerate(['cobblestone-day','rocky-road-day']):
 for m in [1,2]:
  draw.text(((m-1)*384+8,j*416+8),case+(' classic Shared' if m==1 else ' Shared Ground+'),fill='white')
  board.paste(Image.fromarray(images[case,0,m].astype('uint8')),((m-1)*384,j*416+28))
board.save(o/'comparison.png')
report={'result':'passed','stage_metrics':metrics,'same_inputs_camera_clock_weather_settings':True,'day_night_and_hdr':True,'native_geometry_and_retail_material_equation':True,'raw_diagnostic_color_space':'UNORM API values before tone mapping; both paths use identical image formats and samplers','in_game_visual_verification':False}
(o/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print('All terrain lighting comparisons passed.')
