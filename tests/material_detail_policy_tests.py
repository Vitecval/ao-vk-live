"""Protect source selection and HD export policy independently of renderer tests."""
import importlib.util,unittest
from pathlib import Path
path=Path(__file__).resolve().parents[1]/'tools/check-material-resolution.py'
spec=importlib.util.spec_from_file_location('resolution',path);gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
class Policy(unittest.TestCase):
 def test_native_sized_is_not_hd(self):
  with self.assertRaises(ValueError):gate.require_detail_size('1010004:1',(256,256))
 def test_rectangular_hd(self):gate.require_detail_size('1010004:1',(128,512))
 def test_functional_exception(self):gate.require_detail_size('1010004:1',(64,64),{'reason':'Hash-pinned alpha coverage input'})
 def test_family_downgrade(self):
  with self.assertRaises(ValueError):gate.merge_family([{'type':4,'id':1,'size':[1024,512]}],[{'type':4,'id':1,'size':[256,128]}])
 def test_family_upgrade(self):
  result=gate.merge_family([{'type':4,'id':1,'size':[256,128]}],[{'type':4,'id':1,'size':[1024,512]}]);self.assertEqual(result[0]['size'],[1024,512])
 def test_native_detail_retained(self):
  with self.assertRaises(ValueError):gate.require_size('1010004:1',(512,512),(1024,1024))
if __name__=='__main__':unittest.main()
