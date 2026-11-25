"""CPU counterexamples for natural Shared+ triangle-independent field mapping.

Consumes preserved, source-proven r71 audit samples rather than altering them.
This checks material weights and world-coordinate UV Jacobians, not GPU pixels.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def smooth(a: float, b: float, value: float) -> float:
    t = max(0., min(1., (value - a) / (b - a)))
    return t * t * (3. - 2. * t)


def field(h: list[float], u: float, v: float, scale: list[float]) -> tuple[float, float, float]:
    mix = lambda a, b, t: a * (1. - t) + b * t
    height = mix(mix(h[0], h[1], u), mix(h[2], h[3], u), v)
    gx = mix(h[1] - h[0], h[3] - h[2], v) / scale[0]
    gz = mix(h[2] - h[0], h[3] - h[1], u) / scale[1]
    return height, gx, gz


def main() -> None:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--audit', type=Path, default=ROOT / 'build/environment-078-r71/triangle-audit')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/environment-078-r71/natural-field-cpu-check/RESULTS.json')
    args = parser.parse_args()
    result = dict(result='failed', rendererSha256=hashlib.sha256((ROOT / 'rvk/shaders/terrain.glsl').read_bytes()).hexdigest(),
                  gpuExecuted=False, scope='CPU source-layout weights, lighting direction and material UV/Jacobian continuity',
                  cases={}, scans={})
    requested = {'215630': {(199, 36): 473, (218, 42): 583},
                 '215604': {(214, 59): 474}, '215655': {(230, 64): 581}}
    try:
        summary = json.loads((args.audit / 'FIXED-WEIGHT-SUMMARY.json').read_text())
        result['previousShaderSha256'] = summary['shaderSha256']
        sample_count = 0
        largest_prior_gap = 0.
        for scan in ('215604', '215630', '215655', '220250'):
            folder = args.audit / ('scan-' + scan)
            source = json.loads((folder / 'FIXED-WEIGHT-MODEL.json').read_text())
            evidence = json.loads((folder / 'REPLAY-EVIDENCE.json').read_text())
            scale = evidence['cellScale']
            if isinstance(scale, (float, int)):
                scale = [scale, scale]
            else:
                scale = [scale[0], scale[-1]]
            maximum_uv_delta = 0.
            for record in source['records']:
                h = record['heights']
                slopes = [((h[1]-h[0])/scale[0], (h[2]-h[0])/scale[1]),
                          ((h[3]-h[2])/scale[0], (h[3]-h[1])/scale[1])]
                max_gap = 0.
                max_prior = max(sample['newGap'] for sample in record['samples'])
                largest_prior_gap = max(largest_prior_gap, max_prior)
                for sample in record['samples']:
                    u, v = sample['t'], 1. - sample['t']
                    height, gx, gz = field(h, u, v, scale)
                    physical_y = h[2] * (1.-u) + h[1] * u
                    side = ((1. - smooth(.7, .92, abs(sample['fieldNy']) + sample['wander'])) *
                            (1. - smooth(.97, .9995, abs(sample['fieldNy']))) * sample['join'])
                    # Opposite source triangles have different slopes/support.
                    # Neither is an input of the repaired material/lighting field.
                    sides, uvs, jacobians = [], [], []
                    for _triangle_slope in slopes:
                        sides.append(side)
                        original_uv_y = -physical_y / 24. + .173  # retained X/Z strata offset
                        uvs.append(original_uv_y + (physical_y - height) / 24.)
                        jacobians.append((-gx / 24., -gz / 24.))
                    max_gap = max(max_gap, abs(sides[0]-sides[1]))
                    maximum_uv_delta = max(maximum_uv_delta, abs(uvs[0]-uvs[1]),
                                           *(abs(a-b) for a, b in zip(jacobians[0], jacobians[1])))
                    assert abs(uvs[0] - (-height / 24. + .173)) < 1e-12
                    # Confirm the supplied Jacobian differentiates the actual
                    # bilinear UV, including the previously collapsed flat half.
                    epsilon = 1e-5
                    hx = field(h, u+epsilon, v, scale)[0]
                    hz = field(h, u, v+epsilon, scale)[0]
                    assert abs((hx-height)/(epsilon*scale[0]) - gx) < 1e-8
                    assert abs((hz-height)/(epsilon*scale[1]) - gz) < 1e-8
                    assert max_gap == 0.
                    sample_count += 1
                key = tuple(record['worldCell'])
                if key in requested.get(scan, {}):
                    assert record['tileId'] == requested[scan][key]
                    assert max_prior > .7, (scan, key, 'Counterexample no longer exercises the old defect')
                    worst = record['worstSample']
                    _, gx, gz = field(h, worst['t'], 1.-worst['t'], scale)
                    result['cases'][f'{scan}:{key[0]},{key[1]}'] = dict(tile=record['tileId'], previousMaximumGap=max_prior,
                        repairedMaximumGap=max_gap, analyticGradient=[gx, gz], geometricSupports=record['geometrySupports'],
                        flatTrianglePreviouslyGated=record['oneFlatGatedHalf'])
                    if record['oneFlatGatedHalf']:
                        assert math.hypot(gx, gz) > .01, 'Flat mesh half still has collapsed side UV derivatives'
            result['scans'][scan] = dict(cells=len(source['records']), maximumMaterialGap=0., maximumSideUvOrJacobianDelta=maximum_uv_delta,
                inputsSha256={name: hashlib.sha256((folder/name).read_bytes()).hexdigest()
                              for name in ('FIXED-WEIGHT-MODEL.json', 'REPLAY-EVIDENCE.json', 'height-gradient-context24.f32')})
        assert len(result['cases']) == 4
        # Truly flat FIELD: exact original material weight/UV and normal size,
        # independent of arbitrary AO triangle normals. This does not exempt a
        # flat triangle embedded in a sloping bilinear field.
        for wander in (-.07, 0., .07):
            assert (1.-smooth(.7,.92,1.+wander)) * (1.-smooth(.97,.9995,1.)) == 0.
        assert field([6.6]*4, .203, .797, [4.,4.]) == (6.6,0.,0.)
        for magnitude in (0., .25, 1., 2.):
            ny = .50767
            nx = math.sqrt(1.-ny*ny)
            assert abs(math.hypot(nx*magnitude, ny*magnitude)-magnitude) < 1e-12
        result.update(result='passed', samplesChecked=sample_count, previousMaximumGap=largest_prior_gap)
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
