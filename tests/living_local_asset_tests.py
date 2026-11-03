"""No original AO assets: resource audit and continuous aerial rejection tests."""
import importlib.util
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parent.parent


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


audit = module('living_audit', ROOT/'tools/living-asset-audit.py')
clearance = module('aerial_clearance', ROOT/'tools/living/aerial_clearance.py')


class ExportAudit(unittest.TestCase):
    def image(self):
        data = bytearray(1024)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 60, 128)
        data[128:132] = b'PE\0\0'
        struct.pack_into('<HH', data, 132, 0x14c, 1)
        struct.pack_into('<H', data, 148, 224)
        struct.pack_into('<H', data, 152, 0x10b)
        struct.pack_into('<I', data, 244, 16)
        struct.pack_into('<II', data, 248, 4096, 128)
        struct.pack_into('<IIII', data, 384, 512, 4096, 512, 512)
        struct.pack_into('<IIIII', data, 532, 1, 1, 4144, 4160, 4176)
        struct.pack_into('<I', data, 576, 4192)
        data[608:617] = b'Resource\0'
        return data

    def test_reads_export_names_without_loading_library(self):
        self.assertEqual(audit.exports(self.image()), {'Resource'})

    def test_truncated_and_hostile_images_fail(self):
        for length in (0, 60, 150, 300, 600):
            with self.subTest(length=length), self.assertRaises(ValueError):
                audit.exports(self.image()[:length])
        data = self.image()
        struct.pack_into('<I', data, 536, 65537)
        with self.assertRaises(ValueError):
            audit.exports(data)

    def test_export_pointer_outside_file_fails(self):
        data = self.image()
        struct.pack_into('<I', data, 576, 0xffffffff)
        with self.assertRaises(ValueError):
            audit.exports(data)


class AerialClearance(unittest.TestCase):
    def setUp(self):
        self.args = dict(start=(10, 30, 50), end=(90, 30, 50), radius=2, margin=1,
                         bounds=((0, -20, 0), (100, 100, 100)), triangles=[],
                         terrain=[[(0, 0, 0), (100, 0, 0), (0, 0, 100)],
                                  [(100, 0, 100), (0, 0, 100), (100, 0, 0)]],
                         complete=True, outdoor=True, identity_matches=True, phasefront=True)

    def result(self, **changes):
        return clearance.validate(**(self.args | changes))[0]

    def test_complete_open_straight_route(self):
        self.assertTrue(self.result())

    def test_middle_obstacle_with_clear_endpoints(self):
        self.assertFalse(self.result(triangles=[[(50, 0, 40), (50, 60, 40), (50, 30, 60)]]))

    def test_vehicle_envelope_and_safety_margin(self):
        self.assertFalse(self.result(triangles=[[(50, 29, 52.9), (51, 31, 52.9), (49, 31, 52.9)]]))
        self.assertTrue(self.result(triangles=[[(50, 29, 54), (51, 31, 54), (49, 31, 54)]]))

    def test_unknown_geometry_indoor_and_wrong_version(self):
        for key in ('complete', 'outdoor', 'identity_matches'):
            with self.subTest(key=key):
                self.assertFalse(self.result(**{key: False}))

    def test_whole_vehicle_stays_in_bounds(self):
        self.assertFalse(self.result(start=(1, 30, 50)))

    def test_missing_terrain_and_projection_holes(self):
        self.assertFalse(self.result(terrain=[]))
        self.assertFalse(self.result(terrain=[[(0, 0, 0), (100, 0, 0), (0, 0, 100)]]))

    def test_altitude_extremes_and_intermediate_peak(self):
        self.assertFalse(self.result(start=(10, 19, 50), end=(90, 19, 50)))
        self.assertFalse(self.result(start=(10, 51, 50), end=(90, 51, 50)))
        peak = [[(40, 0, 40), (60, 29, 50), (40, 0, 60)]]
        self.assertFalse(self.result(terrain=self.args['terrain']+peak))

    def test_nonfinite_and_degenerate_inputs(self):
        self.assertFalse(self.result(radius=float('nan')))
        self.assertFalse(self.result(start=(float('inf'), 30, 50)))
        self.assertFalse(self.result(end=self.args['start']))
        self.assertFalse(self.result(terrain=[[(0, 0, 0)]*3]))


if __name__ == '__main__':
    unittest.main()
