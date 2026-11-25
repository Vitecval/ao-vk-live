#pragma once
#include <windows.h>

namespace rnative::tooltips {
// Verified classic Live GUI.dll only. The timer is shared by native item and
// interface tooltips. No client file is modified.
class DelayPatch {
public:
    bool Apply(HMODULE module,bool instant);
    bool Changed() const { return changed_; }
private:
    bool Refuse(const char* reason);
    HMODULE module_=nullptr;
    bool changed_=false;
    const char* lastReason_=nullptr;
};
void SetInstant(bool enabled);
void Poll(); // first and subsequent game frames, including a late-loaded GUI.dll
void Restore(); // render-window shutdown; a later window reapplies the saved choice
}
