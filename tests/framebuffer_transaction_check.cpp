#include "framebuffer_transaction.h"
#include <array>
#include <cassert>
#include <stdexcept>
#include <new>
#include <set>
#include <cstdio>

int main() {
    for(int failure=-1;failure<9;++failure) {
        std::array<int,7> old{1,2,3,4,5,6,7},current=old;
        std::set<int> live{1,2,3,4,5,6,7};
        int width=1920,height=1080,bound=1;bool committed=false,rolledBack=false;
        bool ok=rnative::framebuffer::Replace(
            [&]{current.fill(0);width=3840;height=2160;},
            [&]{for(int i=0;i<7;++i){current[i]=101+i;live.insert(current[i]);
                if(failure==i)return false;if(failure==8 && i==3)throw std::bad_alloc();}return true;},
            [&]{assert(live.count(old[0]));if(failure==7)return false;bound=current[0];return true;},
            [&]{for(int id:current)if(id)live.erase(id);current=old;width=1920;height=1080;bound=old[0];rolledBack=true;},
            [&]{assert(bound==101);for(int id:old)live.erase(id);committed=true;});
        if(failure==-1) {
            assert(ok && committed && !rolledBack && width==3840 && height==2160 && bound==101);
            for(int i=0;i<7;++i)assert(live.count(101+i) && !live.count(1+i));
        } else {
            assert(!ok && !committed && rolledBack && current==old && width==1920 && height==1080 && bound==1);
            assert(live==std::set<int>({1,2,3,4,5,6,7}));
        }
    }
    std::puts("Framebuffer replacement:7-stage/bind/allocation failures retain working targets, dimensions and binding; success releases old resources only after bind");
}
