#include "native/diagnostic_walk.h"
#include <cstdio>
#include <vector>

struct Node { Node* child=nullptr; Node* sibling=nullptr; };
int main()
{
    using namespace rnative::diagnostics;
    auto first=[](const void* p)->const void*{return static_cast<const Node*>(p)->child;};
    auto next=[](const void* p)->const void*{return static_cast<const Node*>(p)->sibling;};
    size_t visits=0;
    auto visit=[&](const void*,int){++visits;};
    Node root,a,b,c;
    root.child=&a; a.sibling=&b; b.sibling=&c; c.sibling=&a;
    WalkBudget cycle;
    if (WalkChildren(&root,0,cycle,first,next,visit) || visits!=3 || !cycle.truncated) return 1;
    a.sibling=&a; visits=0;
    WalkBudget self;
    if (WalkChildren(&root,0,self,first,next,visit) || visits!=1) return 2;
    std::vector<Node> wide(2000);
    for (size_t i=1;i<wide.size();++i) wide[i-1].sibling=&wide[i];
    root.child=wide.data(); visits=0;
    WalkBudget capped;
    if (WalkChildren(&root,0,capped,first,next,visit) || visits!=512 || !capped.truncated) return 3;
    std::vector<Node> deep(1000);
    for (size_t i=1;i<deep.size();++i) deep[i-1].child=&deep[i];
    root.child=deep.data(); visits=0;
    WalkBudget depth;
    if (!WalkChildren(&root,0,depth,first,next,visit) || visits!=3) return 4;
    // Valid sibling/child trees retain the complete intended three-level output.
    a={&c,&b}; b={}; c={}; root.child=&a; visits=0;
    WalkBudget valid;
    if (!WalkChildren(&root,0,valid,first,next,visit) || visits!=3 || valid.truncated) return 5;
    std::puts("PASS: three-node cycle, self-cycle, 2000 siblings, depth bound and valid frame tree");
}
