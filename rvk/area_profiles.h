#pragma once
#include <cstdint>

namespace rvk::areas {
enum class Biome { General, City, Forest, Desert, Coast, Marsh, Spirit, Snow, Volcanic, Alien, Ash };
// Playfield identities checked against the installed classic database's names.
// Gardens deliberately keep the gentler forest treatment, including Inferno's.
inline Biome For(uint32_t id)
{
    if(id>=4676 && id<=4699)return Biome::Forest;
    switch(id) {
    case 386:return Biome::Alien;
    case 655:case 566:case 4001:case 4314:case 4315:case 4316:
    case 4530:case 4531:case 4532:case 4533:case 4534:return Biome::City;
    case 565:case 551:case 560:case 570:case 760:case 791:case 795:
    case 670:case 615:case 620:case 6011:case 6012:case 6550:return Biome::Desert;
    case 556:case 656:case 665:case 585:case 500:case 501:case 6010:case 4894:case 4872:case 4873:return Biome::Coast;
    case 505:case 600:case 605:case 716:case 717:case 685:case 687:case 695:
    case 630:case 646:case 647:case 625:case 635:case 650:case 790:case 6551:
    case 4310:case 4311:case 4313:case 4540:case 4541:case 4542:case 4543:case 4544:return Biome::Forest;
    case 4312:return Biome::Marsh;
    case 4880:case 4881:return Biome::Spirit;
    case 4006:case 4320:case 4321:case 4322:case 4324:return Biome::Snow;
    case 4605:return Biome::Volcanic;
    case 4328:case 4329:case 4330:case 4331:case 4389:case 4390:case 4391:case 4900:return Biome::Ash;
    default:return Biome::General;
    }
}
struct Palette { float sky[3], ground[3], sun[3], sideFill; };
inline Palette Lighting(uint32_t id)
{
    if(id==655)return {{.82f,.95f,1.12f},{1.03f,.96f,.84f},{1.035f,1.01f,.96f},.025f};
    switch(For(id)) {
    case Biome::City:return {{.91f,.98f,1.06f},{1.03f,.96f,.86f},{1.04f,1.f,.94f},.010f};
    case Biome::Forest:case Biome::Marsh:return {{.86f,1.01f,.96f},{.88f,1.03f,.83f},{1.025f,1.f,.96f},.016f};
    case Biome::Desert:return {{.91f,.99f,1.10f},{1.12f,.98f,.76f},{1.055f,1.f,.92f},.012f};
    case Biome::Coast:return {{.83f,.96f,1.14f},{.92f,1.f,1.04f},{1.02f,1.f,.98f},.014f};
    case Biome::Spirit:return {{.85f,.91f,1.13f},{.91f,.87f,1.04f},{1.01f,.98f,1.04f},.012f};
    case Biome::Snow:return {{.88f,.98f,1.12f},{.95f,1.f,1.08f},{1.02f,1.f,.97f},.008f};
    case Biome::Volcanic:return {{.98f,.91f,.88f},{1.16f,.88f,.68f},{1.06f,.98f,.90f},.008f};
    case Biome::Ash:return {{.87f,.92f,1.06f},{.94f,.91f,.99f},{1.01f,.98f,1.02f},.006f};
    default:return {{1.f,1.f,1.f},{1.f,.96f,.88f},{1.025f,1.f,.94f},.008f};
    }
}
// Runtime particle style: auto=0, fireflies=1, dust=2, spores=3,
// spirit motes=4, snow=5, embers=6, ash=7.
inline int Ambient(uint32_t id)
{
    switch(For(id)) {
    case Biome::Alien:case Biome::Marsh:return 3;
    case Biome::Spirit:return 4;
    case Biome::Snow:return 5;
    case Biome::Volcanic:return 6;
    case Biome::Ash:return 7;
    case Biome::City:case Biome::Forest:case Biome::Coast:
        return id>=4000 && id<5000 ? 8:0; // SL pollen by day, glowing life at night.
    default:return 0; // Actual local vegetation/dry-ground classification.
    }
}
struct MoteLook {float colour[3],speed,flutter;};
inline MoteLook AmbientLook(uint32_t id) {
    switch(For(id)) {
    case Biome::Snow:return {{.66f,.84f,1.32f},.60f,.65f};
    case Biome::Spirit:return {{.70f,.86f,1.20f},.55f,1.35f};
    case Biome::Marsh:return {{.78f,1.13f,.71f},.70f,1.20f};
    case Biome::Alien:return {{.65f,1.22f,.78f},.86f,1.45f};
    case Biome::Volcanic:return {{1.18f,.78f,.55f},1.15f,1.25f};
    case Biome::Ash:return {{.86f,.82f,1.12f},.65f,.65f};
    case Biome::Coast:return {{.68f,.94f,1.18f},.78f,.80f};
    case Biome::Forest:
        if(id>=4000 && id<5000) {
            // Stable regional looks, with mint, pearl-blue and lilac spores.
            // Gardens use warm pollen; snow/ash retain their own explicit style.
            if(id>=4676)return {{1.08f,1.04f,.78f},.65f,.75f};
            switch(id%3) {
            case 0:return {{.55f,.90f,1.35f},.64f,1.20f};
            case 1:return {{.78f,1.13f,.86f},.78f,.90f};
            default:return {{1.08f,.76f,1.22f},.58f,1.40f};
            }
        }
        return {{1.f,1.f,.90f},.85f,.90f};
    default:return {{1.f,1.f,1.f},1.f,1.f};
    }
}
}
