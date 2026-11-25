"""Persist the actual six-board visual review, including role/alpha exceptions."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/environment-078-r65/workers/root-effects'
evidence=json.loads((OUT/'evidence.json').read_text());rows=[]
adequate={18263,223035,225444,245026,36944,40479,40773,6085}
held={154372:'Reflection/environment input: new marble-like fine structure needs runtime role verification before changing it.',
 200767:'Replacement adds an outer RGB ring to a soft halo. Verify alpha/blend semantics before changing this effect.',
 263104:'Replacement adds an egg-shaped RGB cloud over the original flat field. Verify original alpha and blend semantics.',
 222809:'Original scanline/ring glow versus smoother replacement: verify authored pattern and alpha use before repainting.',
 223779:'Tree surface/glow input: natural-material versus glow-mask role needs verification.',
 25568:'Carved circular floor/glow decal: inspect actual surface and original glow coverage before repainting.'}
for i,source in enumerate(evidence):
 row=dict(source);number=int(row['identity'].split(':')[1])
 row.update(review_board=str(OUT/f'pairs-{i//24+1}.png'),viewed_original_and_current=True,retail_uv_fit_verified=False)
 if number==10620:
  row.update(status='confirmed-poor-painted-source-ready',reason='Missing physical readout plate replacement; enlarged original and faithful new painting reviewed.')
 elif number==119021:
  row.update(status='held-layout-drift',reason='Physical strip needs repaint; two generated outputs letterboxed and changed native aspect. Rejected outputs are not packaged.')
 elif number==30879:
  row.update(status='transferred-character-review',reason='Enlarged original is a blue trouser/armour texture, not an environmental glow effect.')
 elif number in {6561,269833}:
  row.update(status='transferred-architecture-review',reason='Physical panel/plaque RGB, not a soft effect; needs architecture/layout review independently of small dimensions.')
 elif number in {281620,285382}:
  row.update(status='transferred-prop-atlas-review',reason='RGB inspection reveals a full turret/loot prop atlas behind its glow alpha; not a soft gradient-only input.')
 elif number==25572:
  row.update(status='transferred-natural-review',reason='Original RGB is a complete cracked stone floor with emissive cracks; preserve both surface layout and its authored alpha.')
 elif number in held:
  row.update(status='held-role-or-pattern-verification',reason=held[number])
 elif number in adequate:
  row.update(status='retained-good',reason='Original/current layout and physical detail adequate in the viewed pair. Glow alpha may hide original RGB framing on a composited preview; no repaint for size alone.')
 else:
  row.update(status='retained-functional-effect',reason='Viewed gradient, noise/reflection field, cloud/particle/sprite or emissive-pattern input. Softness and original frame/alpha layout serve its role; no independent diffuse repaint warranted by dimensions alone.')
 row['review_board_rgb']=str(OUT/f'pairs-rgb-{i//24+1}.png')
 row['visual_quality_confirmed_poor']=number in {10620,119021}
 rows.append(row)
(OUT/'visual-review-ledger.json').write_text(json.dumps(rows,indent=2)+'\n')
from collections import Counter
print(json.dumps(dict(reviewed=len(rows),statuses=dict(Counter(r['status'] for r in rows))),indent=2))
