#pragma once
namespace rnative::framebuffer {
// Stage and bind a replacement while the working resources remain owned.
// Rollback must restore globals/binding and destroy candidates only. Commit
// releases the working set only after every required stage has succeeded.
template<class Stage,class Build,class Bind,class Rollback,class Commit>
bool Replace(Stage&& stage,Build&& build,Bind&& bind,Rollback&& rollback,Commit&& commit) {
    bool ready=false;
    try {stage();ready=build() && bind();} catch(...) {}
    if(!ready){rollback();return false;}
    commit();
    return true;
}
} // namespace rnative::framebuffer
