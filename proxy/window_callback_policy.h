#pragma once
namespace rvk::presentation {
inline thread_local unsigned windowCallbackDepth=0;
struct WindowCallbackScope {
    WindowCallbackScope(){++windowCallbackDepth;}
    ~WindowCallbackScope(){--windowCallbackDepth;}
    WindowCallbackScope(const WindowCallbackScope&)=delete;
    WindowCallbackScope& operator=(const WindowCallbackScope&)=delete;
};
inline bool InsideWindowCallback(){return windowCallbackDepth!=0;}
}
