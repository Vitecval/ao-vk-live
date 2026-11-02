#include "internal.h"
#include <cstdio>
#include <algorithm>
#include <string>
namespace rvk {
void Device::ResetWeatherStats(){
 m_wearDraws=m_wearMatched=m_wearEligible=m_wearActive=m_wearOverflow=0;
 if(m_buildWear>0 || m_wearDebug>0)m_wearStats.fill({});
}
void Device::NoteWeatherDraw(const Texture* t,bool match,uint32_t blocked,bool active,bool pbr,bool hd,bool stage1){
 ++m_wearDraws;m_wearMatched+=match;m_wearEligible+=blocked==0;m_wearActive+=active;
 uint64_t key=(uint64_t(t->m_rdbType)<<32)|t->m_rdbId;if(!key){++m_wearOverflow;return;}
 for(uint32_t n=0;n<256;++n){auto& e=m_wearStats[(t->m_rdbId*2654435761u+n)&255];
  if(e.key && e.key!=key)continue;e.key=key;++e.draws;e.matched+=match;e.eligible+=blocked==0;e.active+=active;e.pbr+=active&&pbr;e.hd+=active&&hd;e.stage1+=active&&stage1;e.reasons|=blocked;return;}
 ++m_wearOverflow;
}
void Device::ReportWeatherStats(FILE* f){
 if(m_buildWear<=0 && m_wearDebug<=0){if(f)std::fprintf(f,"# weathering: Off; no optional draw inspection\n");return;}
 auto line=[&](const std::string& s){if(f)std::fprintf(f,"# %s\n",s.c_str());else Log("%s",s.c_str());};
 char b[600];std::snprintf(b,sizeof(b),"weathering frame %llu: level %.0f debug %.0f zone %u inspected %u matched %u eligible %u weathered-submitted %u overflow %u; counts are draw submissions, not pixel visibility",(unsigned long long)m_frameNumber,m_buildWear,m_wearDebug,m_skyPlayfield,m_wearDraws,m_wearMatched,m_wearEligible,m_wearActive,m_wearOverflow);line(b);
 std::vector<const WearStat*> rows;for(const auto& e:m_wearStats)if(e.key)rows.push_back(&e);
 std::sort(rows.begin(),rows.end(),[](auto a,auto b){if(bool(a->matched)!=bool(b->matched))return a->matched>b->matched;return a->draws>b->draws;});
 size_t count=f?rows.size():std::min<size_t>(rows.size(),24);
 for(size_t i=0;i<count;++i){const auto& e=*rows[i];std::string why;
  const char* names[]={"unlisted","no-zone","dynamic","protected-visual","blend","alpha-test","geometry-or-lighting","self-lit","emission-mask","stage-discard"};
  for(int j=0;j<10;++j)if(e.reasons&(1u<<j)){if(!why.empty())why+=",";why+=names[j];}
  std::snprintf(b,sizeof(b),"weathering material %u:%u: draws %u matched %u eligible %u weathered %u PBR %u HD-albedo %u RGB-stage1 %u blocked=%s",uint32_t(e.key>>32),uint32_t(e.key),e.draws,e.matched,e.eligible,e.active,e.pbr,e.hd,e.stage1,why.empty()?"none":why.c_str());line(b);
 }
}
}
