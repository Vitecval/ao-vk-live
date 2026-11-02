"""Offline publication guards: no clone or network is needed for preflight."""
import hashlib,json,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
class Publishing(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='ao-vk-publish-')
  self.root=Path(self.temp.name);(self.root/'tools').mkdir()
  self.script=self.root/'tools/publish-release.ps1';shutil.copy2(ROOT/'tools/publish-release.ps1',self.script)
  self.release=self.root/'dist/releases/0.6.0-preview.18';self.release.mkdir(parents=True)
  self.source=self.root/'snapshot';self.source.mkdir();(self.source/'example.cpp').write_text('verified source\n')
  inventory={'format':1,'files':[{'path':'example.cpp','sha256':sha(self.source/'example.cpp')}]}
  (self.release/'SOURCE-FILES.json').write_text(json.dumps(inventory))
  self.assets={}
  for edition in ['lite','full']:
   p=self.release/f'ao-vk-live-0.6.0-preview.18-windows-x86-{edition}.zip';p.write_bytes(edition.encode())
   self.assets[edition]={'sha256':sha(p)}
  (self.release/'SHA256SUMS.txt').write_text(''.join(f"{self.assets[e]['sha256']}  ao-vk-live-0.6.0-preview.18-windows-x86-{e}.zip\n" for e in ['lite','full']))
  (self.release/'RELEASE-NOTES.md').write_text('Reviewed release notes\n')
  self.validation={'version':'0.6.0-preview.18','release_state':'ready_for_experimental_prerelease_publication','critical_failures':[],
    'source':{'source_manifest_sha256':sha(self.release/'SOURCE-FILES.json'),'snapshot':str(self.source)},
    'archives':{'editions':self.assets},'publication':{'release_notes_sha256':sha(self.release/'RELEASE-NOTES.md')}}
  self.save()
 def tearDown(self):self.temp.cleanup()
 def save(self):(self.release/'validation.json').write_text(json.dumps(self.validation))
 def run_script(self,*args):return subprocess.run(['pwsh','-NoProfile','-File',str(self.script),'-Version','0.6.0-preview.18',*args],capture_output=True,text=True,timeout=30)
 def test_default_is_offline_read_only(self):
  p=self.run_script();self.assertEqual(p.returncode,0,p.stderr);self.assertIn('No Git/GitHub mutation or network',p.stdout)
  self.assertFalse((self.root.parent/'ao-vk-live-update').exists())
 def test_changed_source_refused(self):
  (self.source/'example.cpp').write_text('changed');p=self.run_script('-CheckOnly');self.assertNotEqual(p.returncode,0);self.assertIn('Source differs',p.stderr)
 def test_changed_archive_refused(self):
  (self.release/'ao-vk-live-0.6.0-preview.18-windows-x86-full.zip').write_bytes(b'changed');p=self.run_script();self.assertNotEqual(p.returncode,0);self.assertIn('checksum failed',p.stderr)
 def test_changed_notes_refused(self):
  (self.release/'RELEASE-NOTES.md').write_text('changed');p=self.run_script();self.assertNotEqual(p.returncode,0);self.assertIn('release notes changed',p.stderr)
 def test_critical_failure_refused(self):
  self.validation['critical_failures']=['failed renderer test'];self.save();p=self.run_script();self.assertNotEqual(p.returncode,0);self.assertIn('validation is incomplete',p.stderr)
 def test_ambiguous_steps_refused(self):
  p=self.run_script('-PrepareSource','-Publish');self.assertNotEqual(p.returncode,0);self.assertIn('Choose one explicit',p.stderr)
if __name__=='__main__':unittest.main()
