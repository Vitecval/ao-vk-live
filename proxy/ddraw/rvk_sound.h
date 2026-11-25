#pragma once
#include <cstdint>
namespace rvkproxy {
bool ReadSoundContext(uint32_t& zone,bool& outdoor);
void SoundFrame(bool live);
void SoundShutdown();
}
