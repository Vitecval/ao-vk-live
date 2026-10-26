"""Classification boundaries for equipment and scenery names used by AO."""
import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('catalog', Path(__file__).resolve().parents[1] / 'tools/build-material-catalog.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)

class Classification(unittest.TestCase):
    def test_surfaces(self):
        cases = {'body_solitusmale_african_naked.png': 'skin', 'head_opifexfemale_default.png': 'skin',
                 'body_energizedmetal.png': 'steel', 'arms_plasteel.png': 'painted_metal',
                 'body_enhancedleather.png': 'leather', 'legs_combatpants.png': 'cloth',
                 'body_kevlar.png': 'cloth', 'body_metaplast.png': 'polymer',
                 'head_weapon_tongue.png': 'organic', 'weapon_bone.png': 'bone',
                 'body_unidentified.png': 'mixed_outfit', 'ep03_pistol_03.png': 'painted_metal',
                 'head_solitusmale028.png': 'skin', 'medium_blaster.png': 'painted_metal',
                 'ai_c6_scout_body.png': 'polymer', 'shoulderpads_pande_armor_blue.png': 'painted_metal',
                 'hood_aquaans-trenchcoat.png': 'cloth'}
        for name, expected in cases.items():
            with self.subTest(name=name): self.assertEqual(catalog.classify(name, 1010004)[0], expected)

    def test_non_equipment(self):
        for name in ['hq_armor_sign.png', 'new_vending_terminal_icc_weapons.png', 'windmill_blade.png',
                     'special_statue_sword_d.png', 'weapon_beam.png', 'arms_alpha.png',
                     'omni_chair_metal.png', 'melonskin.png']:
            with self.subTest(name=name): self.assertIsNone(catalog.classify(name, 1010004))
        self.assertIsNone(catalog.classify('body_unidentified.png', 1010011))

if __name__ == '__main__': unittest.main()
