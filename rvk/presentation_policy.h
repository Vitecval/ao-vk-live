#pragma once
namespace rvk::presentation {
// Login/character selection must keep presenting even with the default-off
// background preference. Unknown session state follows that same live policy.
constexpr bool SuspendForFocus(bool drawable,bool focused,bool renderInBackground,bool hasLocalCharacter){
    return drawable&&!focused&&!renderInBackground&&hasLocalCharacter;
}
}
