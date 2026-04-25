#ifndef VERILATOR_V3SIMPLEASSIGNGRAPH_H_
#define VERILATOR_V3SIMPLEASSIGNGRAPH_H_
class AstNetlist;

class V3SimpleAssignGraph final {
public:
    static void dumpSimpleAssigns(AstNetlist* root);
};
#endif