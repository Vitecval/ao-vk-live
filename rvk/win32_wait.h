#pragma once
#include <windows.h>

namespace rvk::win32 {
// Win32 Vulkan WSI may synchronously send to the HWND owner from a presenter.
// Dispatch only nonqueued sent messages; posted input, timers and WM_QUIT stay
// for the game's normal loop. Never run a nested game input/render loop here.
inline void PumpSentMessages()
{
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
}

inline DWORD WaitWithSentMessages(HANDLE handle, DWORD timeout)
{
    const DWORD began = GetTickCount();
    DWORD remaining = timeout;
    for (;;) {
        const DWORD result = MsgWaitForMultipleObjectsEx(1, &handle, remaining,
                                                       QS_SENDMESSAGE, MWMO_INPUTAVAILABLE);
        if (result != WAIT_OBJECT_0 + 1)
            return result;
        PumpSentMessages();
        if (timeout != INFINITE) {
            const DWORD elapsed = GetTickCount() - began;
            if (elapsed >= timeout)
                return WaitForSingleObject(handle, 0);
            remaining = timeout - elapsed;
        }
    }
}
} // namespace rvk::win32
