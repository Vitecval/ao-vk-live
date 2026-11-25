#include "foliage_alpha.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

int main() {
    using rvk::foliage::HasCutoutEdges;
    // The captured billboard overlays are soft all over, with zero hard edges.
    assert(!HasCutoutEdges(false, 0.f));
    assert(!HasCutoutEdges(false, .001f));
    assert(!HasCutoutEdges(false, std::nextafter(.002f, 0.f)));
    // Preserve the existing canopy boundary and genuine leaf silhouettes.
    assert(HasCutoutEdges(false, .002f));
    assert(HasCutoutEdges(false, .004f));
    assert(HasCutoutEdges(false, .2f));
    assert(!HasCutoutEdges(true, .2f));
    assert(!HasCutoutEdges(false, std::numeric_limits<float>::quiet_NaN()));
    using rvk::foliage::HasFoliageEdges;
    // Sign lettering can have stronger alpha edges than leaves. Native role,
    // not a looser numeric threshold, keeps both it and its overlays stable.
    for (uint32_t id : {258411u, 258412u, 285825u}) {
        assert(!HasFoliageEdges(1010004u, id, false, 0.f));
        assert(!HasFoliageEdges(1010004u, id, false, .091f));
        assert(!HasFoliageEdges(1010004u, id, false, 1.f));
        assert(HasFoliageEdges(1010005u, id, false, .091f));
    }
    assert(HasFoliageEdges(1010004u, 42116u, false, .173f));
    assert(HasFoliageEdges(1010004u, 9653u, false, .192f));
    std::puts("Soft screen/glow cards excluded; opaque cards excluded; leaf-edge boundary preserved");
}
