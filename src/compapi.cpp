#include "compiler.h"
#include "types.h"

/* Provide a callable C API from ante */
extern "C" {

    ante::Node* Ante_getAST(){
        return ante::parser::getRootNode();
    }

    void Ante_debug(ante::TypedValue &tv){
        tv.dump();
    }

    size_t Ante_sizeof(ante::Compiler *c, ante::TypedValue &tv){
        if(tv.type->type == ante::TT_Type){
            return extractTypeValue(tv)->getSizeInBits(c) / 8;
        }else{
            return tv.type->getSizeInBits(c) / 8;
        }
    }

}

namespace ante {
    map<string, CtFunc*> compapi = {
        {"Ante_getAST", new CtFunc((void*)Ante_getAST, mkTypeNodeWithExt(TT_Ptr, mkDataTypeNode("Node")))},
        {"Ante_debug",  new CtFunc((void*)Ante_debug,  mkAnonTypeNode(TT_Void), {mkAnonTypeNode(TT_TypeVar)})},
        {"Ante_sizeof", new CtFunc((void*)Ante_sizeof, mkAnonTypeNode(TT_U32), {mkAnonTypeNode(TT_TypeVar)})}
    };


    CtFunc::CtFunc(void* f) : fn(f), params(), retty(mkAnonTypeNode(TT_Void)){}
    CtFunc::CtFunc(void* f, TypeNode *retTy) : fn(f), params(), retty(retTy){}
    CtFunc::CtFunc(void* f, TypeNode *retTy, vector<TypeNode*> p) : fn(f), params(p), retty(retTy){}

    //convert void* to void*() and call it
    void* CtFunc::operator()(){
        void* (*resfn)() = 0;
        *reinterpret_cast<void**>(&resfn) = fn;
        return resfn();
    }

    void* CtFunc::operator()(TypedValue &tv){
        void* (*resfn)(TypedValue&) = 0;
        *reinterpret_cast<void**>(&resfn) = fn;
        return resfn(tv);
    }

    void* CtFunc::operator()(Compiler *c, TypedValue &tv){
        void* (*resfn)(Compiler*, TypedValue&) = 0;
        *reinterpret_cast<void**>(&resfn) = fn;
        return resfn(c, tv);
    }

    void* CtFunc::operator()(TypedValue &tv1, TypedValue &tv2){
        void* (*resfn)(TypedValue&, TypedValue&) = 0;
        *reinterpret_cast<void**>(&resfn) = fn;
        return resfn(tv1, tv2);
    }
}
