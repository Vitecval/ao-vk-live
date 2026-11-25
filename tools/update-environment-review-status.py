"""Reconcile audited identities with reviewed paintings; never turn size flags into verdicts."""
from collections import Counter
from datetime import datetime,timezone
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'build/environment-078-r65'
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
audit=read(WORK/'workers/world-audit/refined-texture-audit.json')
baked={r['identity']:r for r in read(WORK/'individual-generation-records.json')}
ready={}
paths=list((WORK/'workers').glob('*/records.json'))+list((WORK/'workers').glob('*/ground-expansion/records.json'))+list((WORK/'workers').glob('*/carved-floors/records.json'))
paths+=list((WORK/'workers').glob('*/grid-expansion/records.json'))
for path in paths:
 for row in read(path):
  if row.get('status')=='reviewed-accepted':ready[row['identity']]=dict(row,review_record=str(path.relative_to(ROOT)))
installed_path=Path('F:/AO_SInstall/Anarchy Online/randy-vk/packs/ao-penumbra-078/manifest.json')
installed={f"{r['type']}:{r['id']}":r for r in read(installed_path)['materials']}
packaged={f"{r['type']}:{r['id']}":r for r in read(ROOT/'material-packs/ao-penumbra-078/manifest.json')['materials']}
ground_plans={r['identity']:r for r in read(WORK/'workers/biomes/ground-expansion/plans.json')}
visual={}
for path in (WORK/'workers').glob('*/visual-review-ledger.json'):
 for row in read(path):visual[row['identity']]=dict(row,ledger=str(path.relative_to(ROOT)))
for path in (WORK/'workers').glob('*/visual-review-ledger-recovery.json'):
 for row in read(path):visual.setdefault(row['identity'],dict(row,ledger=str(path.relative_to(ROOT))))
rows=[]
for original in audit['rows']:
 row=dict(original);identity=row['identity'];state='pending-visual-review'
 if identity in ready or identity in baked:
  evidence=ready.get(identity,baked.get(identity));state='painting-reviewed-awaiting-package'
  row['painting_evidence']=evidence.get('review_record','build/environment-078-r65/individual-generation-records.json')
  theme=bool(evidence.get('user_requested_theme_enhancement') or visual.get(identity,{}).get('user_requested_theme_enhancement'))
  row['visual_quality_confirmed_poor']=not theme
  if theme:row['user_requested_theme_enhancement']=True
  if identity in baked:
   state='painting-packaged'
   if identity in installed and installed[identity]['source_sha256']==packaged[identity]['source_sha256']:state='painting-installed-private-test'
  row['retail_visual_fit_verified']=False
 elif identity in ground_plans:
  state='confirmed-poor-awaiting-painting';row['visual_quality_confirmed_poor']=True
  row['quality_evidence']='build/environment-078-r65/workers/biomes/ground-expansion/original-current-evidence.json'
 elif identity in visual:
  verdict=visual[identity];row['quality_evidence']=verdict['ledger'];row['visual_review_reason']=verdict.get('reason',verdict.get('review_reason',''))
  if verdict.get('visual_quality_confirmed_poor'):row['visual_quality_confirmed_poor']=True
  status=verdict['status']
  if status.startswith('retained-'):state=status
  elif status.startswith('retain-'):state='retained-functional' if 'functional' in status else 'retained-adequate'
  elif status.startswith('restore-original-'):state=status
  elif status.startswith('held-'):state='held-for-layout-or-role-review'
  elif status.startswith('transferred-'):state=status
  else:state='confirmed-poor-awaiting-painting' if status.startswith('confirmed-poor') else 'review-pending'
 elif row['action']=='retain-functional-original-or-approved-coverage':state='retained-functional-or-approved-coverage'
 elif identity in installed:state='prior-private-painting-retail-review-pending'
 row['review_state']=state;rows.append(row)
counts=Counter(r['review_state'] for r in rows)
summary=dict(updated_utc=datetime.now(timezone.utc).isoformat(),indexed_identities=len(rows),review_states=dict(counts),
 accepted_individual_identity_paintings=len(set(ready)|set(baked)),packaged_individual_identity_paintings=len(baked),
 installed_private_material_identities=len(installed),embedded_lava_paintings=1,
 original_current_pairs_in_visual_ledgers=len(visual),complete_metadata_inventory=True,complete_visual_assessment=False,
 interpretation='Dimension/provenance flags remain review candidates. Only individually reviewed poor sources are marked confirmed. Installation is not a claim of retail mesh-fit verification.')
(WORK/'CURRENT-TEXTURE-ASSESSMENT.json').write_text(json.dumps(dict(summary=summary,rows=rows),indent=2)+'\n')
(WORK/'TEXTURE-REVIEW-PROGRESS.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
