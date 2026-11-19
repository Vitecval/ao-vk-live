#pragma once
namespace rvk::presentation {
// A fresh device starts in the login hall before the engine query is ready.
// Unknown is not a transition: retain a confirmed context on the same device.
constexpr int LightingContext(int reported,int previous,bool sameDevice){
    return reported ? reported : sameDevice && previous==2 ? 2:1;
}
// Windows can report the minimized icon's 154x10 client rectangle while AO is
// changing styles. Neither that rectangle nor SIZE_MINIMIZED is a restore.
constexpr bool UsableClient(unsigned width,unsigned height){return width>=32&&height>=32;}
// Login/character selection must keep presenting even with the default-off
// background preference. Unknown session state follows that same live policy.
constexpr bool SuspendForFocus(bool drawable,bool focused,bool renderInBackground,bool hasLocalCharacter){
    return drawable&&!focused&&!renderInBackground&&hasLocalCharacter;
}
}
