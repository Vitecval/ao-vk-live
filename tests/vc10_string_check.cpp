#include "native/vc10.h"
#include <cstdio>
#include <cstring>
#include <string>
namespace rnative {void ReportAllocationFailure(size_t){}}
int main(){
 using rnative::vc10::String;bool okay=true;
 for(const char* text:{"hello","a longer character or material name"}){
  String s{};s.init();const size_t n=std::strlen(text);s.assign(text,n);s.assign(s.c_str(),n);
  const bool pass=s.size==n&&std::memcmp(s.c_str(),text,n)==0&&s.c_str()[n]==0;std::printf("self string assignment (%zu bytes): %s\n",n,pass?"PASS":"FAIL");okay &= pass;s.release();
 }
 return okay?0:1;
}
