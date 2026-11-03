"""Transactional local preparation with synthetic data, no AO binaries or art."""
from pathlib import Path
import contextlib
import io
import json
import sys
import tempfile
import hashlib
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/living'))
import prepare_installed as prep


class Database:
    index_sha256='synthetic-index'
    def __init__(self,path):pass


class Preparation(unittest.TestCase):
    def setUp(self):
        base=ROOT/'build/preparation-tests';base.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=base)
        self.root=Path(self.temp.name)
        self.client=self.root/'Anarchy Online';self.client.mkdir()
        (self.client/'AnarchyOnline.exe').write_text('fixture bytes, not executable')
        database=self.client/'cd_image/data/db';database.mkdir(parents=True)
        self.archive=database/'ResourceDatabase.dat';self.archive.write_bytes(b'preserved original fixture')
        placement=self.client/'cd_image/data/statels';placement.mkdir()
        (placement/'566.pf').write_bytes(b'original fixture placement')
        self.meta=self.root/'metadata';self.meta.mkdir()
        (self.meta/'playfield-sources-preview22.json').write_text(json.dumps({'566':hashlib.sha256(b'original fixture placement').hexdigest()}))
        for name,data in {'routes-v4.json':{'source_index':'synthetic-index'},'CITY-WARDROBES.json':{'cities':{},'themes':{}},'original-catalog.json':{'assets':[],'statics':[]}}.items():
            (self.meta/name).write_text(json.dumps(data))
        scenes=self.meta/'city-wardrobes-v5';scenes.mkdir()
        (scenes/'566.lrs').write_text('LRSCENE5 566 1 0 1 0 synthetic-reviewed\nmissing.lrm\n0\nground-566.lrg\n')
        (self.meta/'ground-data.py').write_text("import numpy as np\ndef load(db,pf):return {'heights':np.zeros((2,2),np.float32),'cell':4.0}\n")
        self.cache=self.client/'randy-vk/living-local'

    def tearDown(self):
        self.assertEqual(self.archive.read_bytes(),b'preserved original fixture')
        prep.cache.LOCAL_ROOT=None
        # TemporaryDirectory was created directly under the known fixture root.
        self.assertEqual(self.root.resolve().parent,(ROOT/'build/preparation-tests').resolve())
        self.temp.cleanup()

    def run_prepare(self,review=False):
        with patch.object(prep,'Database',Database),patch.object(prep,'fan',side_effect=ValueError('unsupported original fan')),contextlib.redirect_stdout(io.StringIO()):
            return prep.prepare(self.client,self.meta,[566],review)

    def active(self):return (self.cache/'active.txt').read_text().strip()

    def test_generated_files_stay_inside_client_and_default_review_off(self):
        report=self.run_prepare()
        generation=self.cache/self.active()
        self.assertTrue(generation.resolve().is_relative_to(self.client.resolve()))
        self.assertIn('ReviewUnverifiedRoutes=0',(generation/'review.ini').read_text())
        self.assertIn('ao-verified-fan-rotors',report['errors'])
        self.assertFalse((self.cache/'preparing.lock').exists())

    def test_review_is_explicit_and_previous_generation_is_reversible(self):
        self.run_prepare();old=self.active();self.run_prepare(True)
        self.assertEqual((self.cache/'previous.txt').read_text().strip(),old)
        self.assertTrue((self.cache/old).is_dir())
        self.assertIn('ReviewUnverifiedRoutes=1',(self.cache/self.active()/'review.ini').read_text())

    def test_failed_preparation_preserves_active_cache_and_removes_incomplete_generation(self):
        self.run_prepare();old=self.active()
        with patch.object(Database,'index_sha256','unsupported'):
            with self.assertRaisesRegex(ValueError,'Unsupported resource database'):self.run_prepare()
        self.assertEqual(self.active(),old)
        self.assertEqual(len(list(self.cache.glob('generation-*'))),1)

    def test_successful_cache_is_bounded_to_active_and_previous(self):
        for _ in range(3):self.run_prepare()
        self.assertEqual(len(list(self.cache.glob('generation-*'))),2)

    def test_changed_old_cache_is_preserved(self):
        self.run_prepare();old=self.cache/self.active()
        (old/'566.lrs').write_text('owner edit')
        self.run_prepare();self.run_prepare()
        self.assertTrue(old.is_dir())

    def test_lock_refuses_concurrent_preparation(self):
        self.cache.mkdir(parents=True);(self.cache/'preparing.lock').write_text('another invocation')
        with self.assertRaisesRegex(ValueError,'preparation is running'):self.run_prepare()
        self.assertFalse((self.cache/'active.txt').exists())

    def test_disk_space_failure_does_not_create_cache(self):
        class Disk:free=0
        with patch.object(prep.shutil,'disk_usage',return_value=Disk()):
            with self.assertRaisesRegex(ValueError,'2 GiB'):self.run_prepare()
        self.assertFalse(self.cache.exists())

    def test_changed_placement_refuses_old_routes(self):
        self.run_prepare();old=self.active()
        (self.client/'cd_image/data/statels/566.pf').write_bytes(b'changed fixture placement')
        with self.assertRaisesRegex(ValueError,'Unsupported placement'):self.run_prepare()
        self.assertEqual(self.active(),old)


if __name__=='__main__':unittest.main()
