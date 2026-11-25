#pragma once
#include <cstdint>

namespace rvk::environment {
// Reviewed normal/Fixer Grid destination and emergency-exit faces: retain text contrast
// instead of treating their light backgrounds as broad emissive lamps.
inline bool ReadableGridSign(uint32_t type, uint32_t id) {
    if (type!=1010004 && type!=1010016 && type!=1010017) return false;
    switch (id) {
    case 82934: case 82937: case 82939: case 82945: case 82947: case 82949:
    case 82951: case 82953: case 82957: case 82959: case 82961: case 82963:
    case 82965: case 82966: case 82968: case 82970: case 95814: case 125777:
    case 159671: case 159682: case 159684: case 159686: case 159688: case 159690:
    case 159692: case 159694: case 159696: case 159698: case 159700: case 159702:
    case 159704: case 159706: case 159708: case 159710: case 159712: case 159714:
    case 159725: case 159752: case 159754: case 159756: case 159758: case 159760:
    case 159767: case 159769: case 159771: case 159773: case 159775: case 159777:
    case 159779: case 159781: case 159783: case 159785: case 159787: case 159789:
    case 159791: case 159793: case 159795: case 159896: case 159898: case 159907:
    case 159909: case 159911: case 159913: case 159915: case 205337: case 205339:
    case 254910: case 283457: case 285603: case 285605: case 287922:
        return true;
    default: return false; // Excludes the Grid's glowpiece and other functional light fields.
    }
}
// Side-by-side RGBA review found a full armour atlas in place of this
// original 16px radial light field. Only the proven legacy provider is
// suppressed by the loader; explicit personal/private maps remain eligible.
inline bool RetainLegacyFunctionalOriginal(uint32_t type,uint32_t id) {
    // 85949 is the Grid's smooth translucent cyan pillar cap; the legacy
    // replacement incorrectly adds physical grit. Provider scoping is at the
    // loader call site, so later private/personal artwork remains eligible.
    return (type==1010004 || type==1010016 || type==1010017) && (id==292915 || id==85949);
}
// Reviewed ice identities; the low-quality world tables share these IDs.
inline bool Ice(uint32_t type, uint32_t id) {
    if(type!=1010004 && type!=1010016 && type!=1010017) return false;
    switch(id) {
    case 206037: case 206297: case 206353: case 206889:
    case 210140: case 210141: case 217790: case 226466: case 226467: return true;
    default: return false;
    }
}
}
