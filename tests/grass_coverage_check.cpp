#include "grass_coverage.h"
#include <array>
#include <cassert>
#include <cstdio>
int main() {
    // A clamped terrain atlas: green on the left, a broad tan path through the
    // right edge. The former wrapping/nearest lookup grew grass over that path.
    std::array<uint8_t,512> bits{};
    for (int y=0;y<64;++y) for (int x=0;x<24;++x) {
        int bit=y*64+x;bits[bit>>3]|=uint8_t(1u<<(bit&7));
    }
    using rvk::grass::Coverage;
    for (int i=0;i<100;++i) {
        float v=float(i)/99;
        assert(Coverage(bits.data(),.2f,v,true)==1);
        assert(Coverage(bits.data(),.8f,v,true)==0);
        assert(Coverage(bits.data(),1,v,true)==0);
        assert(Coverage(bits.data(),1.1f,v,true)==0);
    }
    assert(Coverage(bits.data(),1,.5f,false)==.5f);
    // A narrow path in a large baked atlas must still be resolved: the RGB
    // thumbnail's 16x16 samples could miss this strip altogether.
    std::array<uint8_t,2048> large;
    large.fill(255);
    for (int y=0;y<128;++y) for (int x=63;x<=64;++x) {
        int bit=y*128+x;large[bit>>3]&=uint8_t(~(1u<<(bit&7)));
    }
    assert(Coverage(large.data(),.5f,.5f,true,128)==0);
    assert(Coverage(large.data(),.4f,.5f,true,128)==1);
    float before=1;
    for (int i=0;i<=100;++i) {
        float c=Coverage(bits.data(),(23.0f+i*.02f)/64,.5f,true);
        assert(c<=before+.00001f);before=c;
    }
    uint8_t green[]={60,106,42},tan[]={150,120,90},black[]={0,0,0},grey[]={100,100,100};
    assert(rvk::grass::Green(green));
    assert(!rvk::grass::Green(tan)&&!rvk::grass::Green(black)&&!rvk::grass::Green(grey));
    std::puts("Grass coverage: paths stay bare; edges taper; clamp/wrap and dark/grey rejection passed");
}
