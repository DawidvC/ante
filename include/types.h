#ifndef AN_TYPES_H
#define AN_TYPES_H

#include "compiler.h"

#ifndef AN_USZ_SIZE
#define AN_USZ_SIZE (8*sizeof(void*))
#endif

namespace ante {

    TypedValue typeCheckWithImplicitCasts(Compiler *c, TypedValue &arg, TypeNode *ty);

    TypeNode* deepCopyTypeNode(const TypeNode *n);
    string typeNodeToStr(const TypeNode *t);
    lazy_str typeNodeToColoredStr(const TypeNode *t);
    lazy_str typeNodeToColoredStr(const unique_ptr<TypeNode>& tn);
    lazy_str typeNodeToColoredStr(const shared_ptr<TypeNode>& tn);

    //Typevar creation with no yy::location
    TypeNode* mkAnonTypeNode(TypeTag);
    TypeNode* mkTypeNodeWithExt(TypeTag tt, TypeNode *ext);
    TypeNode* mkDataTypeNode(string tyname);
    TypeNode* createFnTyNode(NamedValNode *params, TypeNode *retTy);

    //conversions
    Type* typeTagToLlvmType(TypeTag tagTy, LLVMContext &c, string typeName = "");
    TypeTag llvmTypeToTypeTag(Type *t);
    string llvmTypeToStr(Type *ty);
    string typeTagToStr(TypeTag ty);
    bool llvmTypeEq(Type *l, Type *r);

    //typevar utility functions
    void validateType(Compiler *c, const TypeNode* tn, const DataDeclNode* rootTy);
    void validateType(Compiler *c, const TypeNode *tn, const DataType *dt);
    TypeNode* extractTypeValue(const TypedValue &tv);
    TypeNode* extractTypeValue(const unique_ptr<TypedValue> &tv);
    void bindGenericToType(TypeNode *tn, const vector<pair<string, unique_ptr<TypeNode>>> &bindings);
    void bindGenericToType(TypeNode *tn, const vector<unique_ptr<TypeNode>> &bindings, DataType *dt);

    string getCastFnBaseName(TypeNode *t);

    TypeNode* getLargestExt(Compiler *c, TypeNode *tn);
    char getBitWidthOfTypeTag(const TypeTag tagTy);
    bool isPrimitiveTypeTag(TypeTag ty);
    bool isNumericTypeTag(const TypeTag ty);
    bool isIntTypeTag(const TypeTag ty);
    bool isFPTypeTag(const TypeTag tt);
    bool isUnsignedTypeTag(const TypeTag tagTy);

}

#endif
