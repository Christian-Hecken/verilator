#ifndef V3SELFASSIGN_H
#define V3SELFASSIGN_H

class AstNetlist;

class V3SelfAssign final {
public:
    static void deleteSelfAssigns(AstNetlist* nodep);
};
#endif