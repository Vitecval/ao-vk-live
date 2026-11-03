"""Conservative whole-path clearance for future straight aerial corridors.

Reuses Living's geometry-survey output convention. It does not author routes or
open the AO database. No real route is approved by this module alone. Triangle
bounds can reject safe routes; they cannot miss a collision with those triangles.
"""
import math
from itertools import chain


def vector(value):
    if len(value) != 3 or not all(math.isfinite(x) and abs(x) < 100000 for x in value):
        raise ValueError('Invalid world coordinate')
    return tuple(value)


def interval(start, end, low, high, axes=(0, 1, 2)):
    entry, exit = 0., 1.
    for axis in axes:
        step = end[axis]-start[axis]
        if abs(step) < 1e-12:
            if not low[axis] <= start[axis] <= high[axis]:
                return None
        else:
            a, b = (low[axis]-start[axis])/step, (high[axis]-start[axis])/step
            entry, exit = max(entry, min(a, b)), min(exit, max(a, b))
            if entry > exit:
                return None
    return entry, exit


def terrain_span(start, end, points):
    def side(a, b, p):
        return (b[0]-a[0])*(p[2]-a[2])-(b[2]-a[2])*(p[0]-a[0])
    area = side(*points)
    if abs(area) < 1e-10:
        raise ValueError('Degenerate terrain triangle')
    sign = 1 if area > 0 else -1
    entry, exit = 0., 1.
    for a, b in zip(points, points[1:]+points[:1]):
        first, last = sign*side(a, b, start), sign*side(a, b, end)
        if first < 0 and last < 0:
            return None
        if first < 0:
            entry = max(entry, -first/(last-first))
        elif last < 0:
            exit = min(exit, first/(first-last))
        if entry > exit:
            return None
    return entry, exit


def validate(start, end, radius, margin, bounds, triangles, terrain, *, complete=False,
             outdoor=False, identity_matches=False, phasefront=False):
    """radius must enclose ALL orientations, animation, hover and attached rider.

    complete/identity_matches are supplied by a separately verified geometry and
    resource pipeline, not inferred from endpoint checks or a matching filename.
    Terrain must include every terrain cell crossed. Missing coverage fails closed.
    """
    if not complete or not outdoor or not identity_matches:
        return False, 'incomplete geometry, indoor playfield or mismatched source identity'
    try:
        start, end = vector(start), vector(end)
        low, high = map(vector, bounds)
        if not math.isfinite(radius) or not math.isfinite(margin) or radius <= 0 or margin < 0:
            raise ValueError('Invalid vehicle bound or safety margin')
        if math.dist(start, end) < .01 or any(a >= b for a, b in zip(low, high)):
            raise ValueError('Degenerate route or playfield bounds')
        envelope = radius+margin
        for point in (start, end):
            if any(point[i]-envelope < low[i] or point[i]+envelope > high[i] for i in range(3)):
                return False, 'swept vehicle leaves playfield bounds'
        if len(triangles)+len(terrain) > 2000000:
            raise ValueError('Geometry exceeds validation budget')
        for triangle in chain(triangles, terrain):
            if len(triangle) != 3:
                raise ValueError('Invalid triangle')
            points = [vector(p) for p in triangle]
            lo = [min(p[i] for p in points)-envelope for i in range(3)]
            hi = [max(p[i] for p in points)+envelope for i in range(3)]
            if interval(start, end, lo, hi) is not None:
                return False, 'whole swept path may intersect geometry'
        if phasefront:
            covered = []
            for triangle in terrain:
                points = [vector(p) for p in triangle]
                lo = [min(p[i] for p in points) for i in range(3)]
                hi = [max(p[i] for p in points) for i in range(3)]
                span = terrain_span(start, end, points)
                if span is None:
                    continue
                heights = [start[1]+t*(end[1]-start[1]) for t in span]
                if min(heights)-hi[1] < 20 or max(heights)-lo[1] > 50:
                    return False, '20-50 metre altitude cannot be certified over entire terrain interval'
                covered.append(span)
            reach = 0.
            for a, b in sorted(covered):
                if a > reach+1e-9:
                    return False, 'terrain coverage gap'
                reach = max(reach, b)
            if reach < 1:
                return False, 'terrain coverage incomplete'
    except (ValueError, TypeError, OverflowError) as error:
        return False, str(error)
    return True, 'conservative continuous static clearance only; live validation still required'
