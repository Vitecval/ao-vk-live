// Weathering v2: reviewed material identities, not a generic static-mesh fallback.
#pragma once
#include <algorithm>
#include <iterator>
#include <cstdint>
namespace rvk::environment {
struct WeatherMaterial {uint32_t id; uint8_t kind, family, maintenance, emissionMask;};
inline constexpr WeatherMaterial weatherMaterials[]={
 {5942,1,1,255,0}, // 128x128basewall.png
 {5945,1,1,255,0}, // 128x128basewallvents.png
 {5949,1,1,255,0}, // 128x128basewalllowborder.png
 {5958,1,1,255,0}, // basewallventendpart.png
 {6008,2,14,255,0}, // factory_bigstripedplate.png
 {6012,2,14,255,0}, // factory_platedetail.png
 {6013,2,14,255,0}, // factory_crossplate.png
 {6020,2,14,255,0}, // factory_platedetailside.png
 {6024,2,12,255,0}, // big dark plate.png
 {6030,2,12,255,0}, // factory_bigplate_cornerpart.png
 {6050,3,5,255,0}, // plateyellbars.png
 {6061,3,5,255,0}, // plateyellhorisontalpipes.png
 {6063,3,5,255,0}, // wallfront.png
 {6065,3,5,255,0}, // wallside.png
 {6066,3,5,255,0}, // plateyellbottompart.png
 {6073,3,5,255,0}, // mplate_dirty.png
 {6083,3,5,255,0}, // plateyelltopridge.png
 {6088,3,5,255,0}, // midtech_metalwall1.png
 {6098,2,5,255,0}, // greymetal.png
 {6178,2,13,255,0}, // wire_techyplates.png
 {6195,2,10,255,0}, // stripedplates.png
 {8247,2,14,255,0}, // factory_portablewall.png
 {9802,2,13,255,0}, // roundplate_dirty.png
 {9810,2,13,255,0}, // roundplate_dirty_detail.png
 {9822,2,13,255,0}, // darksimple_pillarpart.png
 {11321,2,15,166,0}, // hitech_wallitemplate.png
 {11324,2,15,166,0}, // hitech_wallitems_lowerpart.png
 {14263,2,3,166,0}, // mainbuildingplate_darkdesaturatedgreen.png
 {14735,2,3,166,0}, // hq_bigstripedwallplate.png
 {14804,2,3,166,0}, // hq_lowerwallfluidinside.png
 {15750,2,3,166,0}, // hq_smallwall_generalplate.png
 {16003,2,3,166,0}, // hq_lowerwall_ornamentalplate.png
 {16010,2,3,166,0}, // hq_lowerwallinnerpartornamental_tech.png
 {18202,1,23,255,0}, // ent_arena_generic_walltile_dirty.png
 {18203,1,23,255,0}, // ent_arena_generic_walltile.png
 {18207,2,23,255,0}, // ent_arena_wall.png
 {18515,2,13,255,0}, // dock_wall_rustybottom_trans.png
 {18553,2,15,166,0}, // slum_wall1.png
 {18566,2,15,166,0}, // slum_wall2_nailed.png
 {19927,2,23,255,0}, // ent_wall2mainplate_endfringe.png
 {19929,2,23,255,0}, // ent_wall2mainplate.png
 {20354,2,13,255,0}, // dockwall_cleanwithror.png
 {20605,2,12,255,0}, // hq_smallwall_generalplate2.png
 {20808,1,10,255,0}, // rhinohouse_bigrockwallside.png
 {21516,1,1,255,0}, // newclan_cleanwall_cracked_tileableplate.png
 {21520,1,1,255,0}, // newclan_cleanwall_cracked_upperfringe.png
 {21521,1,1,255,0}, // 2wall-clean.png
 {21522,1,1,255,0}, // 2wall-clean_tileable.png
 {21534,3,1,255,0}, // newclan_metal_tileableplate.png
 {21737,2,14,255,0}, // factory_metal_wall_broken.png
 {23128,2,23,255,0}, // entwall_maingeneralplate.png
 {25424,1,4,166,0}, // concretebridge_metal_tileableplate.png
 {26958,2,3,166,0}, // standalone_globeplate_lite1.png
 {27700,1,12,255,0}, // gatewall_ridge.png
 {27703,1,12,255,0}, // gatewall_upper_ridge.png
 {29267,2,10,255,0}, // villageruin_sandwall_brokenpipepart.png
 {29524,2,13,255,0}, // newbiezone_wall_simple.png
 {29597,1,4,166,0}, // concretebridge_generalside_veryplain.png
 {32231,2,14,255,0}, // avalon_wallsimple_front.png
 {35592,1,12,255,0}, // tlr_mine_dirty_wall.png
 {36100,1,27,255,0}, // athens_baselimestone_plaster1.png
 {36110,1,27,255,0}, // athens_baselimestone_aroundpipe.png
 {36122,2,15,166,0}, // standalone_wallpart_withtechitems_brown.png
 {36125,2,15,166,0}, // standalone_wallpart_lower_brown.png
 {36160,2,15,166,0}, // hq_wall_lowpart_upperfringe_lit_old.png
 {36267,2,27,255,0}, // athens_brown_basemetal_top1.png
 {36275,2,27,255,0}, // athens_brown_basemetal_circle.png
 {36276,2,27,255,0}, // athens_brown_basemetal_smallmetalpiece12.png
 {36332,1,27,255,0}, // athens_smallwallpart1.png
 {36333,1,27,255,0}, // athens_smallwallparttop.png
 {37219,1,12,255,0}, // wall_com_sides.png
 {38647,2,28,166,0}, // rome_pillarmat1.png
 {38649,2,28,166,0}, // rome_pillarconnector.png
 {38733,1,25,255,0}, // tir_pillarupper.png
 {40590,2,28,166,0}, // rome_pillarmat2.png
 {40600,2,28,166,0}, // rome_lowerwall_techdetailupper.png
 {40749,1,25,255,0}, // tir_pillarhorizontal.png
 {41140,2,28,166,0}, // rome_wall_zigzagbluedown.png
 {41332,2,28,166,0}, // rome_smallwall_ornamental.png
 {41333,2,28,166,0}, // rome_smallwall_ornamental_pillar.png
 {42122,1,25,255,0}, // athens_basestone_rough.png
 {99648,3,5,255,0}, // neutral_newplate_colstripeupper1.png
 {247847,1,4,166,0}, // ai_building_neutral_concrete.png
 {295543,2,4,166,0}, // wh_tile01_d.png
 {295610,2,4,166,0}, // wh_basemetal_01_d.png
 {295615,3,4,166,0}, // wh_tile04_d.png
 {295902,2,4,166,0}, // wh_basemetaltile02_d.png
 {297312,2,4,166,0}, // npe_trim03_d.png
 {297326,3,4,166,1}, // icc_panelwall02_d.png
 {297327,3,4,166,1}, // icc_panelwall01_d.png
 {297644,2,4,166,0}, // factory_bigpillar_4041_d.png
 {297645,2,4,166,0}, // factory_bigpillar_front_d.png
};
inline const WeatherMaterial* WeatherInfo(uint32_t type,uint32_t id) {
 if(type!=1010004 && type!=1010016 && type!=1010017)return nullptr;
 auto p=std::lower_bound(std::begin(weatherMaterials),std::end(weatherMaterials),id,[](const auto& e,uint32_t n){return e.id<n;});
 return p!=std::end(weatherMaterials)&&p->id==id?p:nullptr;
}
inline uint32_t WeatherProfile(uint32_t type,uint32_t id){auto p=WeatherInfo(type,id);return p?p->kind:0;}
}
