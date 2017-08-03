#include <types.h>

namespace ante {

char getBitWidthOfTypeTag(const TypeTag ty){
    switch(ty){
        case TT_I8:  case TT_U8: case TT_C8:  return 8;
        case TT_I16: case TT_U16: case TT_F16: return 16;
        case TT_I32: case TT_U32: case TT_F32: case TT_C32: return 32;
        case TT_I64: case TT_U64: case TT_F64: return 64;
        case TT_Isz: case TT_Usz: return AN_USZ_SIZE;
        case TT_Bool: return 1;
   
        case TT_Ptr:
        case TT_Function:
        case TT_MetaFunction:
        case TT_FunctionList: return AN_USZ_SIZE;
  
        default: return 0;
    }
}

/*
 *  Returns the TypeNode* value of a TypedValue* of type TT_Type
 */
TypeNode* extractTypeValue(const TypedValue* tv){
    auto zext = dyn_cast<ConstantInt>(tv->val)->getZExtValue();
    return (TypeNode*) zext;
}

TypeNode* extractTypeValue(const unique_ptr<TypedValue> &tv){
    return extractTypeValue(tv.get());
}


/*
 *  Checks to see if a type is valid to be used.
 *  To be valid the type must:
 *      - Not be recursive (contain no references to
 *        itself that are not behind a pointer)
 *      - Contain no typevars that are not declared
 *        within the rootTy's params
 *      - Contain only data types that have been declared
 */
void validateType(Compiler *c, const TypeNode *tn, const DataDeclNode *rootTy){
    if(!tn) return;

    if(tn->type == TT_Data){
        auto *dataTy = c->lookupType(tn->typeName);
        if(!dataTy){
            if(tn->typeName == rootTy->name){
                c->compErr("Recursive types are disallowed, wrap the type in a pointer instead", tn->loc);
            }

            c->compErr("Type "+tn->typeName+" has not been declared", tn->loc);
        }

        if(dataTy->generics.size() != tn->params.size())
            c->compErr("Unbound type params for type "+typeNodeToColoredStr(dataTy->tyn.get()), tn->loc);

        TypeNode *dtyn = dataTy->tyn.get();
        if(!tn->params.empty()){
            dtyn = copy(dtyn);
            bindGenericToType(dtyn, tn->params, dataTy);
        }

        validateType(c, dtyn, rootTy);
    
    }else if(tn->type == TT_Tuple or tn->type == TT_TaggedUnion){
        TypeNode *ext = tn->extTy.get();
        while(ext){
            validateType(c, ext, rootTy);
            ext = (TypeNode*)ext->next.get();
        }
    }else if(tn->type == TT_Array){
        TypeNode *ext = tn->extTy.get();
        validateType(c, ext, rootTy);
    }else if(tn->type == TT_Ptr or tn->type == TT_Function or tn->type == TT_MetaFunction){
        return;

    }else if(tn->type == TT_TypeVar){
        auto *var = c->lookup(tn->typeName);
        if(var){
            return validateType(c, extractTypeValue(var->tval), rootTy);
        }

        //Typevar not found, if its not in the rootTy's params, then it is unbound
        for(auto &p : rootTy->generics){
            if(p->typeName == tn->typeName) return;
        }

        c->compErr("Lookup for "+tn->typeName+" not found", tn->loc);
    }
}

void validateType(Compiler *c, const TypeNode *tn, const DataType *dt){
    auto fakeLoc = mkLoc(mkPos(0, 0, 0), mkPos(0, 0, 0));
    auto *ddn = new DataDeclNode(fakeLoc, dt->name, 0, 0);

    for(auto &g : dt->generics)
        ddn->generics.emplace_back(g.get());

    validateType(c, tn, ddn);
    ddn->generics.clear();
    delete ddn;
}


unsigned int TypeNode::getSizeInBits(Compiler *c, string *incompleteType){
    int total = 0;
    TypeNode *ext = this->extTy.get();

    if(isPrimitiveTypeTag(this->type))
        return getBitWidthOfTypeTag(this->type);
   
    if(type == TT_Data and not extTy.get()){
        auto *dataTy = c->lookupType(typeName);
        if(!dataTy){
            if(incompleteType and typeName == *incompleteType){
                c->compErr("Incomplete Type", loc);
                throw new IncompleteTypeError();
            }

            c->compErr("Type "+typeName+" has not been declared", loc);
            return 0;
        }

        TypeNode *tyn = dataTy->tyn.get();
        if(!dataTy->generics.empty()){
            tyn = copy(tyn);
            bindGenericToType(tyn, params, dataTy);
        }

        return tyn->getSizeInBits(c, incompleteType);
    }

    if(type == TT_Tuple or type == TT_TaggedUnion or type == TT_Data){
        while(ext){
            total += ext->getSizeInBits(c, incompleteType);
            ext = (TypeNode*)ext->next.get();
        }
    }else if(type == TT_Array){
        auto *len = (IntLitNode*)ext->next.get();
        return stoi(len->val) * ext->getSizeInBits(c, incompleteType);
    }else if(type == TT_Ptr or type == TT_Function or type == TT_MetaFunction){
        return 64;

    }else if(type == TT_TypeVar){
        auto *var = c->lookup(typeName);
        if(var){
            return extractTypeValue(var->tval)->getSizeInBits(c);
        }

        c->compErr("Lookup for typevar "+typeName+" not found", loc);
        throw new TypeVarError();
    }

    return total;
}


void bind(TypeNode *type_var, const unique_ptr<TypeNode> &concrete_ty){
    auto *cpy = copy(concrete_ty);
    type_var->type = cpy->type;
    type_var->typeName = cpy->typeName;
    type_var->params = move(cpy->params);
    type_var->extTy.reset(cpy->extTy.release());
    delete cpy;
}


vector<pair<string, unique_ptr<TypeNode>>>
mapBindingsToDataType(const vector<unique_ptr<TypeNode>> &bindings, DataType *dt){
    vector<pair<string, unique_ptr<TypeNode>>> map;

    size_t i = 0;
    for(auto &typevar : dt->generics){
        pair<string,unique_ptr<TypeNode>> p{typevar->typeName, copy(bindings[i])};
        map.push_back(move(p));
        i++;
    }

    return map;
}

/*
 *  Generics can be stored and bound in two ways
 *    1. Stored as a map from name of typevar -> type bound to
 *       - Handled by this function
 *       - This is the format returned by a typeEq type check if it
 *         indicates the check would only be a success if those typevars
 *         are bound to the given types.  This is TypeCheckResult::SuccessWithTypeVars
 *    2. Stored as a vector of ordered bound types.
 *       - The ordering of type vars in this format is matched to the order of the
 *         declaration of generics when the datatype was first declared.
 *         Eg. with type Map<'k,'v> = ... and bindings {Str, i32}, 'k is bound to
 *         Str and 'v is bound to i32.
 *       - This is the format used internally by TypeNodes and DataTypes.
 *       - Before being bound in bindGenericToType this representation must be
 *         converted to the first beforehand, and to do that it needs the DataType
 *         to match the typevar ordering with.  The second function below handles
 *         this conversion
 */
void bindGenericToType(TypeNode *tn, const vector<pair<string, unique_ptr<TypeNode>>> &bindings){
    if(bindings.empty())
        return;

    if(tn->params.empty()){
        if(tn->type == TT_Data or tn->type == TT_TaggedUnion)
            //TODO: this could cause problems with excess bindings
            for(auto& p : bindings)
                tn->params.push_back(unique_ptr<TypeNode>(copy(p.second)));
    }else{
        for(auto& p : tn->params){
            bindGenericToType(p.get(), bindings);
        }
    }

    if(tn->type == TT_TypeVar){
        for(auto& pair : bindings){
            if(tn->typeName == pair.first){
                bind(tn, pair.second);
                return;
            }
        }
    }

    auto *ext = tn->extTy.get();
    while(ext){
        bindGenericToType(ext, bindings);
        ext = (TypeNode*)ext->next.get();
    }
}


void bindGenericToType(TypeNode *tn, const vector<unique_ptr<TypeNode>> &bindings, DataType *dt){
    if(bindings.empty())
        return;

    auto bindings_map = mapBindingsToDataType(bindings, dt);

    return bindGenericToType(tn, bindings_map);
}


/*
 *  Expands a typenode by replacing every Data or TaggedUnion instance with the
 *  types that it contains.  Does not expand pointer types
 */
void Compiler::expand(TypeNode *tn){
    TypeNode *ext = tn->extTy.get();
    if(tn->type == TT_Tuple or tn->type == TT_TaggedUnion){
        while(ext){
            expand(ext);
            ext = (TypeNode*)ext->next.get();
        }
    }else if(!tn->typeName.empty()){
        auto *dt = lookupType(tn->typeName);
        if(!dt) return;

        auto *cpy = copy(dt->tyn);
        tn->extTy.reset(cpy->extTy.release());
        ext = tn->extTy.get();

        while(ext){
            expand(ext);
            ext = (TypeNode*)ext->next.get();
        }
        delete cpy;
    }
}


/*
 *  Replaces already bound typevars with their declared value
 */
void Compiler::searchAndReplaceBoundTypeVars(TypeNode* tn) const{
    TypeNode *ext = tn->extTy.get();
    while(ext){
        //size of an array is stored in the type, skip it
        if(dynamic_cast<IntLitNode*>(ext))
            ext = (TypeNode*)ext->next.get();

        searchAndReplaceBoundTypeVars(ext);
        ext = (TypeNode*)ext->next.get();
    }

    if(tn->type == TT_TypeVar){
        auto *var = lookup(tn->typeName);
        if(!var){
            cerr << "Lookup for "+tn->typeName+" not found\n";
            return;
        }

        TypeNode* val = extractTypeValue(var->tval);
        tn->type = val->type;
        tn->typeName = val->typeName;
        tn->extTy.reset(copy(val->extTy));
        
        for(auto &p : val->params){
            auto cpy = unique_ptr<TypeNode>(copy(p.get()));
            tn->params.push_back(move(cpy));
        }
    }
}


/*
 *  Checks for, and implicitly widens an integer or float type.
 *  The original value of num is returned if no widening can be performed.
 */
TypedValue Compiler::implicitlyWidenNum(TypedValue *num, TypeTag castTy){
    bool lIsInt = isIntTypeTag(num->type->type);
    bool lIsFlt = isFPTypeTag(num->type->type);

    if(lIsInt or lIsFlt){
        bool rIsInt = isIntTypeTag(castTy);
        bool rIsFlt = isFPTypeTag(castTy);
        if(!rIsInt and !rIsFlt){
            cerr << "castTy argument of implicitlyWidenNum must be a numeric primitive type\n";
            exit(1);
        }

        int lbw = getBitWidthOfTypeTag(num->type->type);
        int rbw = getBitWidthOfTypeTag(castTy);
        Type *ty = typeTagToLlvmType(castTy, *ctxt);

        //integer widening
        if(lIsInt and rIsInt){
            if(lbw <= rbw){
                return TypedValue(
                    builder.CreateIntCast(num->val, ty, !isUnsignedTypeTag(num->type->type)),
                    mkAnonTypeNode(castTy)
                );
            }

        //int -> flt, (flt -> int is never implicit)
        }else if(lIsInt and rIsFlt){
            return TypedValue(
                isUnsignedTypeTag(num->type->type)
                    ? builder.CreateUIToFP(num->val, ty)
                    : builder.CreateSIToFP(num->val, ty),

                mkAnonTypeNode(castTy)
            );

        //float widening
        }else if(lIsFlt and rIsFlt){
            if(lbw < rbw){
                return TypedValue(
                    builder.CreateFPExt(num->val, ty),
                    mkAnonTypeNode(castTy)
                );
            }
        }
    }

    return num;
}


/*
 *  Assures two IntegerType'd Values have the same bitwidth.
 *  If not, one is extended to the larger bitwidth and mutated appropriately.
 *  If the extended integer value is unsigned, it is zero extended, otherwise
 *  it is sign extended.
 *  Assumes the llvm::Type of both values to be an instance of IntegerType.
 */
void Compiler::implicitlyCastIntToInt(TypedValue *lhs, TypedValue *rhs){
    int lbw = getBitWidthOfTypeTag((*lhs)->type->type);
    int rbw = getBitWidthOfTypeTag((*rhs)->type->type);

    if(lbw != rbw){
        //Cast the value with the smaller bitwidth to the type with the larger bitwidth
        if(lbw < rbw){
            auto ret = TypedValue(
                builder.CreateIntCast((*lhs)->val, (*rhs)->getType(), !isUnsignedTypeTag((*lhs)->type->type)),
                (*rhs)->type.get());
            
            *lhs = ret;

        }else{//lbw > rbw
            auto ret = TypedValue(
                builder.CreateIntCast((*rhs)->val, (*lhs)->getType(), !isUnsignedTypeTag((*rhs)->type->type)),
                (*lhs)->type.get());

            *rhs = ret;
        }
    }
}

bool isIntTypeTag(const TypeTag ty){
    return ty==TT_I8 or ty==TT_I16 or ty==TT_I32 or ty==TT_I64 or 
           ty==TT_U8 or ty==TT_U16 or ty==TT_U32 or ty==TT_U64 or 
           ty==TT_Isz or ty==TT_Usz or ty==TT_C8;
}

bool isFPTypeTag(const TypeTag tt){
    return tt==TT_F16 or tt==TT_F32 or tt==TT_F64;
}

bool isNumericTypeTag(const TypeTag ty){
    return isIntTypeTag(ty) or isFPTypeTag(ty);
}

/*
 *  Performs an implicit cast from a float to int.  Called in any operation
 *  involving an integer, a float, and a binop.  No matter the ints size,
 *  it is always casted to the (possibly smaller) float value.
 */
void Compiler::implicitlyCastIntToFlt(TypedValue **lhs, Type *ty){
    auto *ret = new TypedValue(
        isUnsignedTypeTag((*lhs)->type->type)
            ? builder.CreateUIToFP((*lhs)->val, ty)
            : builder.CreateSIToFP((*lhs)->val, ty),

        mkAnonTypeNode(llvmTypeToTypeTag(ty))
    );
    *lhs = ret;
}


/*
 *  Performs an implicit cast from a float to float.
 */
void Compiler::implicitlyCastFltToFlt(TypedValue **lhs, TypedValue **rhs){
    int lbw = getBitWidthOfTypeTag((*lhs)->type->type);
    int rbw = getBitWidthOfTypeTag((*rhs)->type->type);

    if(lbw != rbw){
        if(lbw < rbw){
            auto *ret = new TypedValue(
                builder.CreateFPExt((*lhs)->val, (*rhs)->getType()),
                (*rhs)->type.get());
            *lhs = ret;
        }else{//lbw > rbw
            auto *ret = new TypedValue(
                builder.CreateFPExt((*rhs)->val, (*lhs)->getType()),
                (*lhs)->type.get());
            *rhs = ret;
        }
    }
}


/*
 *  Detects, and creates an implicit type conversion when necessary.
 */
void Compiler::handleImplicitConversion(TypedValue **lhs, TypedValue **rhs){
    bool lIsInt = isIntTypeTag((*lhs)->type->type);
    bool lIsFlt = isFPTypeTag((*lhs)->type->type);
    if(!lIsInt and !lIsFlt) return;

    bool rIsInt = isIntTypeTag((*rhs)->type->type);
    bool rIsFlt = isFPTypeTag((*rhs)->type->type);
    if(!rIsInt and !rIsFlt) return;

    //both values are numeric, so forward them to the relevant casting method
    if(lIsInt and rIsInt){
        implicitlyCastIntToInt(lhs, rhs);  //implicit int -> int (widening)
    }else if(lIsInt and rIsFlt){
        implicitlyCastIntToFlt(lhs, (*rhs)->getType()); //implicit int -> flt
    }else if(lIsFlt and rIsInt){
        implicitlyCastIntToFlt(rhs, (*lhs)->getType()); //implicit int -> flt
    }else if(lIsFlt and rIsFlt){
        implicitlyCastFltToFlt(lhs, rhs); //implicit int -> flt
    }
}


bool containsTypeVar(const TypeNode *tn){
    auto tt = tn->type;
    if(tt == TT_Array or tt == TT_Ptr){
        return tn->extTy->type == tt;
    }else if(tt == TT_Tuple or tt == TT_Data or tt == TT_TaggedUnion or
             tt == TT_Function or tt == TT_MetaFunction){
        TypeNode *ext = tn->extTy.get();
        while(ext){
            if(containsTypeVar(ext))
                return true;
        }
    }
    return tt == TT_TypeVar;
}


/*
 *  Translates an individual TypeTag to an llvm::Type.
 *  Only intended for primitive types, as there is not enough
 *  information stored in a TypeTag to convert to array, tuple,
 *  or function types.
 */
Type* typeTagToLlvmType(TypeTag ty, LLVMContext &ctxt, string typeName){
    switch(ty){
        case TT_I8:  case TT_U8:  return Type::getInt8Ty(ctxt);
        case TT_I16: case TT_U16: return Type::getInt16Ty(ctxt);
        case TT_I32: case TT_U32: return Type::getInt32Ty(ctxt);
        case TT_I64: case TT_U64: return Type::getInt64Ty(ctxt);
        case TT_Isz:    return Type::getIntNTy(ctxt, AN_USZ_SIZE); //TODO: implement
        case TT_Usz:    return Type::getIntNTy(ctxt, AN_USZ_SIZE); //TODO: implement
        case TT_F16:    return Type::getHalfTy(ctxt);
        case TT_F32:    return Type::getFloatTy(ctxt);
        case TT_F64:    return Type::getDoubleTy(ctxt);
        case TT_C8:     return Type::getInt8Ty(ctxt);
        case TT_C32:    return Type::getInt32Ty(ctxt);
        case TT_Bool:   return Type::getInt1Ty(ctxt);
        case TT_Void:   return Type::getVoidTy(ctxt);
        case TT_TypeVar:
            throw new TypeVarError();
        default:
            cerr << "typeTagToLlvmType: Unknown/Unsupported TypeTag " << ty << ", returning nullptr.\n";
            return nullptr;
    }
}

TypeNode* getLargestExt(Compiler *c, TypeNode *tn){
    TypeNode *largest = 0;
    size_t largest_size = 0;

    TypeNode *cur = tn->extTy.get();
    while(cur){
        size_t size = cur->getSizeInBits(c);
        if(size > largest_size){
            largest = cur;
            largest_size = size;
        }

        cur = (TypeNode*)cur->next.get();
    }
    return largest;
}


Type* updateLlvmTypeBinding(Compiler *c, DataType *dt, const vector<unique_ptr<TypeNode>> &bindings, string &name){
    auto *cpy = copy(dt->tyn);
    bindGenericToType(cpy, bindings, dt);

    //create an empty type first so we dont end up with infinite recursion
    auto* structTy = StructType::create(*c->ctxt, {}, name, dt->tyn->type == TT_TaggedUnion);
    dt->llvmTypes[name] = structTy;

    if(dt->tyn->type == TT_TaggedUnion)
        cpy = getLargestExt(c, cpy);

    Type *llvmTy = c->typeNodeToLlvmType(cpy);

    if(StructType *st = dyn_cast<StructType>(llvmTy)){
        structTy->setBody(st->elements());
    }else{
		array<Type*, 1> body;
        body[0] = llvmTy;
        structTy->setBody(body);
    }

    dt->llvmTypes[name] = structTy;
    return structTy;
}

/*
 *  Translates a llvm::Type to a TypeTag. Not intended for in-depth analysis
 *  as it loses data about the type and name of UserTypes, and cannot distinguish 
 *  between signed and unsigned integer types.  As such, this should mainly be 
 *  used for comparing primitive datatypes, or just to detect if something is a
 *  primitive.
 */
TypeTag llvmTypeToTypeTag(Type *t){
    if(t->isIntegerTy(1)) return TT_Bool;

    if(t->isIntegerTy(8)) return TT_I8;
    if(t->isIntegerTy(16)) return TT_I16;
    if(t->isIntegerTy(32)) return TT_I32;
    if(t->isIntegerTy(64)) return TT_I64;
    if(t->isHalfTy()) return TT_F16;
    if(t->isFloatTy()) return TT_F32;
    if(t->isDoubleTy()) return TT_F64;
    
    if(t->isArrayTy()) return TT_Array;
    if(t->isStructTy() and !t->isEmptyTy()) return TT_Tuple; /* Could also be a TT_Data! */
    if(t->isPointerTy()) return TT_Ptr;
    if(t->isFunctionTy()) return TT_Function;

    return TT_Void;
}

/*
 *  Converts a TypeNode to an llvm::Type.  While much less information is lost than
 *  llvmTypeToTokType, information on signedness of integers is still lost, causing the
 *  unfortunate necessity for the use of a TypedValue for the storage of this information.
 */
Type* Compiler::typeNodeToLlvmType(const TypeNode *tyNode){
    vector<Type*> tys;
    TypeNode *tyn = tyNode->extTy.get();

    switch(tyNode->type){
        case TT_Ptr:
            return tyn->type != TT_Void ?
                typeNodeToLlvmType(tyn)->getPointerTo()
                : Type::getInt8Ty(*ctxt)->getPointerTo();
        case TT_Array:{
            auto *intlit = (IntLitNode*)tyn->next.get();
            return ArrayType::get(typeNodeToLlvmType(tyn), stoi(intlit->val));
        }
        case TT_Tuple:
            while(tyn){
                tys.push_back(typeNodeToLlvmType(tyn));
                tyn = (TypeNode*)tyn->next.get();
            }
            return StructType::get(*ctxt, tys);
        case TT_Data: case TT_TaggedUnion: {
            auto *dt = lookupType(tyNode->typeName);
            if(!dt)
                compErr("Use of undeclared type " + tyNode->typeName, tyNode->loc);

            string name = dt->name;
            for(auto &p : tyNode->params)
                name += "_" + typeNodeToStr(p.get());

            try{
                return dt->llvmTypes.at(name);
            }catch(out_of_range r){
                return updateLlvmTypeBinding(this, dt, tyNode->params, name);
            }
        }
        case TT_Function: case TT_MetaFunction: {
            //ret ty is tyn from above
            //
            //get param tys
            TypeNode *cur = (TypeNode*)tyn->next.get();
            while(cur){
                tys.push_back(typeNodeToLlvmType(cur));
                cur = (TypeNode*)cur->next.get();
            }

            return FunctionType::get(typeNodeToLlvmType(tyn), tys, false)->getPointerTo();
        } /*
        case TT_TaggedUnion:
            if(!tyn){
                userType = lookupType(tyNode->typeName);
                if(!userType){
                    compErr("Use of undeclared type " + tyNode->typeName, tyNode->loc);
                    return Type::getVoidTy(*ctxt);
                }

                tyn = userType->tyn.get();
                if(tyn->type != TT_U8)
                    tyn = tyn->extTy.get();
            }
            
            while(tyn){
                tys.push_back(typeNodeToLlvmType(tyn));
                tyn = (TypeNode*)tyn->next.get();
            }
            return StructType::get(*ctxt, tys); */
        case TT_TypeVar: {
            Variable *typeVar = lookup(tyNode->typeName);
            if(!typeVar){
                //compErr("Use of undeclared type variable " + tyNode->typeName, tyNode->loc);
                //compErr("tn2llvmt: TypeVarError; lookup for "+tyNode->typeName+" not found", tyNode->loc);
                //throw new TypeVarError();
                return Type::getInt32Ty(*ctxt);
            }
            
            return typeNodeToLlvmType(extractTypeValue(typeVar->tval));
        }
        default:
            return typeTagToLlvmType(tyNode->type, *ctxt);
    }
}


/*
 *  Returns true if two given types are approximately equal.  This will return
 *  true if they are the same primitive datatype, or are both pointers pointing
 *  to the same elementtype, or are both arrays of the same element type, even
 *  if the arrays differ in size.  If two types are needed to be exactly equal, 
 *  pointer comparison can be used instead since llvm::Types are uniqued.
 */
bool llvmTypeEq(Type *l, Type *r){
    TypeTag ltt = llvmTypeToTypeTag(l);
    TypeTag rtt = llvmTypeToTypeTag(r);

    if(ltt != rtt) return false;

    if(ltt == TT_Ptr){
        Type *lty = l->getPointerElementType();
        Type *rty = r->getPointerElementType();

        if(lty->isVoidTy() or rty->isVoidTy()) return true;

        return llvmTypeEq(lty, rty);
    }else if(ltt == TT_Array){
        return l->getArrayElementType() == r->getArrayElementType() and
               l->getArrayNumElements() == r->getArrayNumElements();
    }else if(ltt == TT_Function or ltt == TT_MetaFunction){
        int lParamCount = l->getFunctionNumParams();
        int rParamCount = r->getFunctionNumParams();
        
        if(lParamCount != rParamCount)
            return false;

        for(int i = 0; i < lParamCount; i++){
            if(!llvmTypeEq(l->getFunctionParamType(i), r->getFunctionParamType(i)))
                return false;
        } 
        return true;
    }else if(ltt == TT_Tuple or ltt == TT_Data){
        int lElemCount = l->getStructNumElements();
        int rElemCount = r->getStructNumElements();
        
        if(lElemCount != rElemCount)
            return false;

        for(int i = 0; i < lElemCount; i++){
            if(!llvmTypeEq(l->getStructElementType(i), r->getStructElementType(i)))
                return false;
        } 

        return true;
    }else{ //primitive type
        return true; /* true since ltt != rtt check above is false */
    }
}


TypeCheckResult TypeCheckResult::success(){
    if(box->res != Failure){
        box->matches++;
    }
    return *this;
}

TypeCheckResult TypeCheckResult::successWithTypeVars(){
    if(box->res != Failure){
        box->res = SuccessWithTypeVars;
    }
    return *this;
}

TypeCheckResult TypeCheckResult::failure(){
    box->res = Failure;
    return *this;
}

TypeCheckResult TypeCheckResult::successIf(bool b){
    if(b) return success();
    else  return failure();
}

TypeCheckResult TypeCheckResult::successIf(Result r){
    if(box->res == Success)
        return success();
    else if(box->res == SuccessWithTypeVars)
        return successWithTypeVars();
    else
        return failure();
}

bool TypeCheckResult::failed(){
    return box->res == Failure;
}


//forward decl of typeEqHelper for extTysEq fn
TypeCheckResult typeEqHelper(const Compiler *c, const TypeNode *l, const TypeNode *r, TypeCheckResult tcr);

/*
 *  Helper function to check if each type's list of extension
 *  types are all approximately equal.  Used when checking the
 *  equality of TypeNodes of type Tuple, Data, Function, or any
 *  type with multiple extTys.
 */
TypeCheckResult extTysEq(const TypeNode *l, const TypeNode *r, TypeCheckResult &tcr, const Compiler *c = 0){
    TypeNode *lExt = l->extTy.get();
    TypeNode *rExt = r->extTy.get();

    while(lExt and rExt){
        if(c){
            if(!typeEqHelper(c, lExt, rExt, tcr)) return tcr.failure();
        }else{
            if(!typeEqBase(lExt, rExt, tcr)) return tcr.failure();
        }

        lExt = (TypeNode*)lExt->next.get();
        rExt = (TypeNode*)rExt->next.get();
        if((lExt and !rExt) or (rExt and !lExt)) return tcr.failure();
    }
    return tcr.success();
}

/*
 *  Returns 1 if two types are approx eq
 *  Returns 2 if two types are approx eq and one is a typevar
 *
 *  Does not check for trait implementation unless c is set.
 *
 *  This function is used as a base for typeEq, if a typeEq function
 *  is needed that does not require a Compiler parameter, this can be
 *  used, although it does not check for trait impls.  The optional
 *  Compiler parameter here is only used by the typeEq function.  If
 *  this function is used as a typeEq function with the Compiler ptr
 *  the outermost type will not be checked for traits.
 */
TypeCheckResult typeEqBase(const TypeNode *l, const TypeNode *r, TypeCheckResult tcr, const Compiler *c){
    if(!l) return tcr.successIf(!r);
 
    if(l->type == TT_TaggedUnion and r->type == TT_Data) return tcr.successIf(l->typeName == r->typeName);
    if(l->type == TT_Data and r->type == TT_TaggedUnion) return tcr.successIf(l->typeName == r->typeName);

    if(l->type == TT_TypeVar or r->type == TT_TypeVar)
        return tcr.successWithTypeVars();


    if(l->type != r->type)
        return tcr.failure();

    if(r->type == TT_Ptr){
        if(l->extTy->type == TT_Void or r->extTy->type == TT_Void)
            return tcr.success();

        return extTysEq(l, r, tcr, c);
    }else if(r->type == TT_Array){
        //size of an array is part of its type and stored in 2nd extTy
        auto lsz = ((IntLitNode*)l->extTy->next.get())->val;
        auto rsz = ((IntLitNode*)r->extTy->next.get())->val;

        auto lext = (TypeNode*)l->extTy.get();
        auto rext = (TypeNode*)r->extTy.get();

        if(lsz != rsz) return tcr.failure();

        return c ? typeEqHelper(c, lext, rext, tcr) : typeEqBase(lext, rext, tcr, c);

    }else if(r->type == TT_Data or r->type == TT_TaggedUnion){
        return tcr.successIf(l->typeName == r->typeName);

    }else if(r->type == TT_Function or r->type == TT_MetaFunction or r->type == TT_Tuple){
        return extTysEq(l, r, tcr, c);
    }
    //primitive type, we already know l->type == r->type
    return tcr.success();
}

bool dataTypeImplementsTrait(DataType *dt, string trait){
    for(auto traitImpl : dt->traitImpls){
        if(traitImpl->name == trait)
            return true;
    }
    return false;
}
    
TypeNode* TypeCheckResult::getBindingFor(const string &name){
    for(auto &pair : box->bindings){
        if(pair.second->typeName == name)
            return pair.second.get();
    }
    return 0;
}


/*
 *  Return true if both typenodes are approximately equal
 *
 *  Compiler instance required to check for trait implementation
 */
TypeCheckResult typeEqHelper(const Compiler *c, const TypeNode *l, const TypeNode *r, TypeCheckResult tcr){
    if(!l) return tcr.successIf(!r);
    if(!r) return tcr.failure();

    if((l->type == TT_Data or l->type == TT_TaggedUnion) and (r->type == TT_Data or r->type == TT_TaggedUnion)){
        if(l->typeName == r->typeName){
            if(l->params.empty() and r->params.empty()){
                return tcr.success();
            }

            if(l->params.size() != r->params.size()){
                DataType *dt = nullptr;
                TypeNode *lc = (TypeNode*)l;
                TypeNode *rc = (TypeNode*)r;

                if(!l->extTy.get()){
                    dt = c->lookupType(l);
                    lc = copy(dt->tyn);
                    bindGenericToType(lc, l->params, dt);
                }
                if(!r->extTy.get()){
                    if(!dt)
                        dt = c->lookupType(r);
                    rc = copy(dt->tyn);
                    bindGenericToType(rc, r->params, dt);
                }
                //Types not equal by differing amount of params, see if it is just a lack of a binding issue
                return extTysEq(lc, rc, tcr, c);
            }

            //check each type param of generic tys
            for(unsigned int i = 0, len = l->params.size(); i < len; i++){
                if(!typeEqHelper(c, l->params[i].get(), r->params[i].get(), tcr))
                    return tcr.failure();
            }

            return tcr.success();
        }

        //typeName's are different, check if one is a trait and the other
        //is an implementor of the trait
        Trait *t;
        DataType *dt;
        if((t = c->lookupTrait(l->typeName))){
            //Assume r is a datatype
            //
            //NOTE: r is never checked if it is a trait because two
            //      separate traits are never equal anyway
            dt = c->lookupType(r->typeName);
            if(!dt) return tcr.failure();
            
        }else if((t = c->lookupTrait(r->typeName))){
            dt = c->lookupType(l->typeName);
            if(!dt) return tcr.failure();
        }else{
            return tcr.failure();
        }

        return tcr.successIf(dataTypeImplementsTrait(dt, t->name));

    }else if(l->type == TT_TypeVar or r->type == TT_TypeVar){
      
        //reassign l and r into typeVar and nonTypeVar so code does not have to be repeated in
        //one if branch for l and another for r
        const TypeNode *typeVar, *nonTypeVar;

        if(l->type == TT_TypeVar and r->type != TT_TypeVar){
            typeVar = l;
            nonTypeVar = r;
        }else if(l->type != TT_TypeVar and r->type == TT_TypeVar){
            typeVar = r;
            nonTypeVar = l;
        }else{ //both type vars
            Variable *lv = c->lookup(l->typeName);
            Variable *rv = c->lookup(r->typeName);

            if(lv and rv){ //both are already bound
                auto lty = extractTypeValue(lv->tval);
                auto rty = extractTypeValue(rv->tval);
                return typeEqHelper(c, lty, rty, tcr);
            }else if(lv and not rv){
                typeVar = r;
                nonTypeVar = extractTypeValue(lv->tval);
            }else if(rv and not lv){
                typeVar = l;
                nonTypeVar = extractTypeValue(rv->tval);
            }else{ //neither are bound
                return tcr.successIf(l->typeName == r->typeName);
            }
            return tcr.failure();
        }


        //FIXME: the if statement below was commented out as it does not account
        //       for function calls that should not have access to the current scope.
        //
        //Variable *tv = c->lookup(typeVar->typeName);
        //if(!tv){
            auto *tv = tcr.getBindingFor(typeVar->typeName);
            if(!tv){
                //make binding for type var to type of nonTypeVar
                auto nontvcpy = unique_ptr<TypeNode>(copy(nonTypeVar));
                tcr->bindings.push_back({typeVar->typeName, move(nontvcpy)});

                return tcr.successWithTypeVars();
            }else{ //tv is bound in same typechecking run
                return typeEqHelper(c, tv, nonTypeVar, tcr);
            }

        //}else{ //tv already bound
        //    return typeEqHelper(c, extractTypeValue(tv->tval), nonTypeVar, tcr);
        //}
    }
    return typeEqBase(l, r, tcr, c);
}

TypeCheckResult Compiler::typeEq(const TypeNode *l, const TypeNode *r) const{
    auto tcr = TypeCheckResult();
    typeEqHelper(this, l, r, tcr);
    return tcr;
}


TypeCheckResult Compiler::typeEq(vector<TypeNode*> l, vector<TypeNode*> r) const{
    auto tcr = TypeCheckResult();
    if(l.size() != r.size()){
        tcr.failure();
        return tcr;
    }

    for(size_t i = 0; i < l.size(); i++){
        typeEqHelper(this, l[i], r[i], tcr);
        if(tcr.failed()) return tcr;
    }
    return tcr;
}


/*
 *  Returns true if the given typetag is a primitive type, and thus
 *  accurately represents the entire type without information loss.
 *  NOTE: this function relies on the fact all primitive types are
 *        declared before non-primitive types in the TypeTag definition.
 */
bool isPrimitiveTypeTag(TypeTag ty){
    return ty >= TT_I8 and ty <= TT_Bool;
}


/*
 *  Converts a TypeTag to its string equivalent for
 *  helpful error messages.  For most cases, llvmTypeToStr
 *  should be used instead to provide the full type.
 */
string typeTagToStr(TypeTag ty){
    
    switch(ty){
        case TT_I8:    return "i8" ;
        case TT_I16:   return "i16";
        case TT_I32:   return "i32";
        case TT_I64:   return "i64";
        case TT_U8:    return "u8" ;
        case TT_U16:   return "u16";
        case TT_U32:   return "u32";
        case TT_U64:   return "u64";
        case TT_F16:   return "f16";
        case TT_F32:   return "f32";
        case TT_F64:   return "f64";
        case TT_Isz:   return "isz";
        case TT_Usz:   return "usz";
        case TT_C8:    return "c8" ;
        case TT_C32:   return "c32";
        case TT_Bool:  return "bool";
        case TT_Void:  return "void";

        /* 
         * Because of the loss of specificity for these last four types, 
         * these strings are most likely insufficient.  The llvm::Type
         * should instead be printed for these types
         */
        case TT_Tuple:        return "Tuple";
        case TT_Array:        return "Array";
        case TT_Ptr:          return "Ptr"  ;
        case TT_Data:         return "Data" ;
        case TT_TypeVar:      return "'t";
        case TT_Function:     return "Function";
        case TT_MetaFunction: return "Meta Function";
        case TT_FunctionList: return "Function List";
        case TT_TaggedUnion:  return "|";
        case TT_Type:         return "type";
        default:              return "(Unknown TypeTag " + to_string(ty) + ")";
    }
}

/*
 *  Converts a typeNode directly to a string with no information loss.
 *  Used in ExtNode::compile
 */
string typeNodeToStr(const TypeNode *t){
    if(!t) return "null";

    if(t->type == TT_Tuple){
        string ret = "(";
        TypeNode *elem = t->extTy.get();
        while(elem){
            if(elem->next.get())
                ret += typeNodeToStr(elem) + ", ";
            else
                ret += typeNodeToStr(elem) + ")";
            elem = (TypeNode*)elem->next.get();
        }
        return ret;
    }else if(t->type == TT_Data or t->type == TT_TaggedUnion or t->type == TT_TypeVar){
        string name = t->typeName;
        if(!t->params.empty()){
            name += "<";
            name += typeNodeToStr(t->params[0].get());
            for(unsigned i = 1; i < t->params.size(); i++){
                name += ", ";
                name += typeNodeToStr(t->params[i].get());
            }
            name += ">";
        }
        return name;
    }else if(t->type == TT_Array){
        auto *len = (IntLitNode*)t->extTy->next.get();
        return '[' + len->val + " " + typeNodeToStr(t->extTy.get()) + ']';
    }else if(t->type == TT_Ptr){
        return typeNodeToStr(t->extTy.get()) + "*";
    }else if(t->type == TT_Function or t->type == TT_MetaFunction){
        string ret = "(";
        string retTy = typeNodeToStr(t->extTy.get());
        TypeNode *cur = (TypeNode*)t->extTy->next.get();
        while(cur){
            ret += typeNodeToStr(cur);
            cur = (TypeNode*)cur->next.get();
            if(cur) ret += ",";
        }
        return ret + ")->" + retTy;
    }else{
        return typeTagToStr(t->type);
    }
}

/*
 *  Returns a string representing the full type of ty.  Since it is converting
 *  from a llvm::Type, this will never return an unsigned integer type.
 *
 *  Gives output in a different terminal color intended for printing, use typeNodeToStr
 *  to get a type without print color.
 */
string llvmTypeToStr(Type *ty){
    TypeTag tt = llvmTypeToTypeTag(ty);
    if(isPrimitiveTypeTag(tt)){
        return typeTagToStr(tt);
    }else if(tt == TT_Tuple){
        //if(!ty->getStructName().empty())
        //    return string(ty->getStructName());

        string ret = "(";
        const unsigned size = ty->getStructNumElements();

        for(unsigned i = 0; i < size; i++){
            if(i == size-1){
                ret += llvmTypeToStr(ty->getStructElementType(i)) + ")";
            }else{
                ret += llvmTypeToStr(ty->getStructElementType(i)) + ", ";
            }
        }
        return ret;
    }else if(tt == TT_Array){
        return "[" + to_string(ty->getArrayNumElements()) + " " + llvmTypeToStr(ty->getArrayElementType()) + "]";
    }else if(tt == TT_Ptr){
        return llvmTypeToStr(ty->getPointerElementType()) + "*";
    }else if(tt == TT_Function){
        string ret = "func("; //TODO: get function return type
        const unsigned paramCount = ty->getFunctionNumParams();

        for(unsigned i = 0; i < paramCount; i++){
            if(i == paramCount-1)
                ret += llvmTypeToStr(ty->getFunctionParamType(i)) + ")";
            else
                ret += llvmTypeToStr(ty->getFunctionParamType(i)) + ", ";
        }
        return ret;
    }else if(tt == TT_TypeVar){
        return "(typevar)";
    }else if(tt == TT_Void){
        return "void";
    }
    return "(Unknown type)";
}

} //end of namespace ante
