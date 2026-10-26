"""Compare classic-live ground fixtures, including shader material debug views.

Inputs are rvk_demo --shared-terrain-test images produced with authored deck,
reviewed asphalt IDs, rotated cells, mixed road/soil masks and dark natural rock.
"""
import argparse
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    root = parser.parse_args().directory

    def image(name):
        return np.asarray(Image.open(root / name / 'result.bmp').convert('RGB')).astype(float)

    def unchanged(a, b):
        delta = float(abs(image(a) - image(b)).max())
        assert delta <= 1, (a, b, delta)
        return delta

    comparisons = {
        'authored_deck_shared_on_off': unchanged('deck-on', 'deck-off'),
        'deck_original_colour': unchanged('deck-on', 'deck-palette'),
        'paving21_original_colour': unchanged('paving21', 'paving21-palette'),
        'paving22_original_colour': unchanged('paving22', 'paving22-palette'),
        'road_UV_rotation': unchanged('road-on', 'road-turn'),
        'road_roughness_shared_on_off': unchanged('road-rough', 'road-rough-off'),
        'natural_detail_UV_rotation': unchanged('continuity-flat', 'continuity-turn'),
        'login_on_off': unchanged('login-on', 'login-off'),
    }
    roi = np.s_[130:390, 200:568]
    assert image('deck-on')[roi].std() > 12, 'Deck pattern disappeared'
    assert abs(image('deck-on')-image('deck-personal')).mean()>2, 'Personal ground colour override was blocked'
    assert image('road-on')[roi].std() > 3, 'Shared road detail did not render'
    assert abs(image('continuity-flat') - image('continuity-off')).mean() > 2
    assert abs(image('mask-on') - image('separate-mask')).max() <= 2, 'Ground draw paths differ'
    road = image('road-rough')[roi]
    rock = image('dark-rock')[roi]
    # These debug views encode material parameters directly as bytes.
    assert abs(road.mean() / 255 - .32) < .01, 'Road sheen missing'
    assert rock.mean() / 255 > .83, 'Dark natural rock became glossy'
    assert image('road-metal')[roi].max() == 0, 'Asphalt became metallic'
    assert image('deck-metal')[roi].mean() / 255 > .8, 'Authored deck lost its metal'
    mixed = image('road-mask')[roi].mean(2)
    assert (mixed < 130).mean() > .3 and (mixed > 200).mean() > .3, 'Road mask lost its rough soil edge'
    loader = (root / 'terrain-loader.log').read_text(errors='replace')
    for identity in (17, 20, 21, 22, 23, 24, 25, 26):
        assert re.search(rf'RDB texture (1010006|1010021|1010022):{identity} gets', loader), ('Missing city artwork', identity)
    assert re.search(r'RDB texture (1010006|1010021):17 shares uploaded material maps',loader), 'Deck quality copies did not share'
    assert not re.search(r'RDB texture (1010006|1010021):18 gets', loader), 'Partial asphalt repaint returned'
    assert 'RDB texture 1010006:673 gets' not in loader, 'Partial grass repaint returned'
    assert 'shared terrain materials: off;' in loader and 'shared terrain materials: on;' in loader
    assert 'presentation released' in loader
    for identity in (20,23,24,25,26):
        comparisons[f'paving{identity}_original_colour']=unchanged(f'paving{identity}',f'paving{identity}-palette')
        comparisons[f'paving{identity}_shared_on_off']=unchanged(f'paving{identity}',f'paving{identity}-off')
        rough=image(f'paving{identity}-rough')[roi].mean()/255
        assert .38<=rough<=.54,(identity,rough,'Paving sheen lost')
        assert image(f'paving{identity}-metal')[roi].max()==0,(identity,'Stone became metal')
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|decode failed', loader, re.I)
    assert 'preset checks passed' in (root / 'background-loader.txt').read_text(), 'Ultra defaults failed'
    for folder in root.iterdir():
        log = folder / 'run.txt'
        if not log.is_file():
            continue
        content = log.read_text(errors='replace')
        assert not re.search(r'VK_ERROR|VUID-|Validation Error|device lost|allocation failed', content, re.I), folder.name
        assert 'rendered; screenshot result.bmp' in content, folder.name
    print(json.dumps({'maximum_pixel_deltas': comparisons,
                      'road_roughness': round(float(road.mean() / 255), 3),
                      'rock_roughness': round(float(rock.mean() / 255), 3),
                      'road_nonmetal_and_deck_metal': 'passed',
                      'road_soil_transition_material_mask': 'passed',
                      'city_artwork_and_quality_copy_loader': 'passed',
                      'Ultra_shared_plus_daylight_zero': 'passed',
                      'combined_separate_draws': 'passed'}, indent=2))


if __name__ == '__main__':
    main()
