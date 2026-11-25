"""Release packaging regression: texture metadata must match its actual bootstrap."""
from pathlib import Path
import gzip,importlib.util,json,zipfile
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('horizon_packaging',ROOT/'tools/package-horizon.py')
pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
out=ROOT/'build/horizon-texture-release-check';out.mkdir(parents=True,exist_ok=True)
base={'build':'same-renderer','assets':{'randy-vk/packs/ao-cyber-world/1010004_1_d.png':'a'*64}}
core=out/'bootstrap.zip';resource=out/'full-manifest.json.gz'
with zipfile.ZipFile(core,'w') as z:z.writestr('fixture/manifest.json',json.dumps(base))
resource.write_bytes(gzip.compress(json.dumps(base,indent=2).encode(),mtime=0));pack.verify_full_manifest(core,resource)
changed={'build':'same-renderer','assets':{next(iter(base['assets'])):'b'*64}}
resource.write_bytes(gzip.compress(json.dumps(changed).encode(),mtime=0))
try:pack.verify_full_manifest(core,resource)
except RuntimeError:pass
else:raise AssertionError('New texture metadata was allowed with an old bootstrap')
removed={'build':'same-renderer','assets':{}}
resource.write_bytes(gzip.compress(json.dumps(removed).encode(),mtime=0))
try:pack.verify_full_manifest(core,resource)
except RuntimeError:pass
else:raise AssertionError('Texture-removal metadata was allowed with an old bootstrap')
engine=json.loads((ROOT/'apps/horizon/Assets/engine.json').read_text(encoding='utf-8'))
pack.verify_full_manifest(ROOT/('build/preview24/release-r'+str(engine['revision']))/'ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip',ROOT/'apps/horizon/Assets/full-manifest.json.gz')
report={'result':'passed','matchingCatalogue':True,'replacementMismatchRefused':True,'removalMismatchRefused':True,'currentBundledPackageMatches':True,'retailClientsChanged':False}
(out/'RESULTS.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
