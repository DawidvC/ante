%option noyywrap
%option yylineno
%option bison-bridge
%option bison-locations

%{
#include "tokens.h"
#include "lexer.h"
#include "error.h"
#include <stack>
%}

%x UNINDENT

%{

#define YY_DECL int yylex(YYSTYPE* yylval_param, yy::location* yylloc_param, ante::LexerCtxt* ctxt)

#define YY_USER_ACTION yylloc->begin.line = yylineno;

#define YY_INPUT(buf,result,max_size) \
{                                     \
    char c;                           \
    (*lexerCtxt->is) >> c;            \
    if(lexerCtxt->is->eof()){         \
        result = YY_NULL;             \
    }else{                            \
        buf[0] = c;                   \
        result = 1;                   \
    }                                 \
}

#define YYLTYPE yy::location

using namespace ante;

LexerCtxt *lexerCtxt;

bool ante::colored_output = true;

std::stack<int> scopes;

void flex_init();
void flex_error(const char *msg, yy::parser::location_type* loc);
        
map<int, const char*> tokDict = {
    {Tok_Ident, "Identifier"},
    {Tok_UserType, "UserType"},
    {Tok_TypeVar, "TypeVar"},

    //types
    {Tok_I8, "i8"},
    {Tok_I16, "i16"},
    {Tok_I32, "i32"},
    {Tok_I64, "i64"},
    {Tok_U8, "u8"},
    {Tok_U16, "u16"},
    {Tok_U32, "u32"},
    {Tok_U64, "u64"},
    {Tok_Isz, "isz"},
    {Tok_Usz, "usz"},
    {Tok_F16, "f16"},
    {Tok_F32, "f32"},
    {Tok_F64, "f64"},
    {Tok_C8, "c8"},
    {Tok_C32, "c32"},
    {Tok_Bool, "bool"},
    {Tok_Void, "Void"},

    {Tok_Eq, "=="},
    {Tok_NotEq, "!="},
    {Tok_AddEq, "+="},
    {Tok_SubEq, "-="},
    {Tok_MulEq, "*="},
    {Tok_DivEq, "/="},
    {Tok_GrtrEq, ">="},
    {Tok_LesrEq, "<="},
    {Tok_Or, "or"},
    {Tok_And, "and"},
    {Tok_Range, ".."},
    {Tok_RArrow, "->"},
    {Tok_ApplyL, "<|"},
    {Tok_ApplyR, "|>"},
    {Tok_Append, "++"},
    {Tok_New, "new"},
    {Tok_Not, "not"},

    //literals
    {Tok_True, "true"},
    {Tok_False, "false"},
    {Tok_IntLit, "IntLit"},
    {Tok_FltLit, "FltLit"},
    {Tok_StrLit, "StrLit"},
    {Tok_CharLit, "CharLit"},

    //keywords
    {Tok_Return, "return"},
    {Tok_If, "if"},
    {Tok_Then, "then"},
    {Tok_Elif, "elif"},
    {Tok_Else, "else"},
    {Tok_For, "for"},
    {Tok_While, "while"},
    {Tok_Do, "do"},
    {Tok_In, "in"},
    {Tok_Continue, "continue"},
    {Tok_Break, "break"},
    {Tok_Import, "import"},
    {Tok_Let, "let"},
    {Tok_Var, "var"},
    {Tok_Match, "match"},
    {Tok_With, "with"},
    {Tok_Type, "type"},
    {Tok_Trait, "trait"},
    {Tok_Fun, "fun"},
    {Tok_Ext, "ext"},

    //modifiers
    {Tok_Pub, "pub"},
    {Tok_Pri, "pri"},
    {Tok_Pro, "pro"},
    {Tok_Raw, "raw"},
    {Tok_Const, "const"},
    {Tok_Noinit, "noinit"},
    {Tok_Mut, "mut"},
    {Tok_Global, "global"},

    //other
    {Tok_Where, "where"},
    
    {Tok_Newline, "Newline"},
    {Tok_Indent, "Indent"},
    {Tok_Unindent, "Unindent"},
};

//#define YY_USER_ACTION yylloc.first_line = yyloc.last_line = yylineno; \
//                       yylloc.first_column = yycolumn; \
//                       yylloc.last_column = yycolumn+yyleng-1; \
//                       yycolumn += yyleng;
%}

typevar '[a-z]\w*

usertype [A-Z]\w*

ident [a-z]\w*

strlit \".*\"

intlit [1-9][0-9]*

fltlit {intlit}.[0-9]+

operator [-+*%/#@&=<>|!:]

%%

%{
//Rules
%}

i8    {return(Tok_I8);}
i16   {return(Tok_I16);}
i32   {return(Tok_I32);}
i64   {return(Tok_I64);}
isz   {return(Tok_Isz);}
u8    {return(Tok_U8);}
u16   {return(Tok_U16);}
u32   {return(Tok_U32);}
u64   {return(Tok_U64);}
usz   {return(Tok_Usz);}
c8    {return(Tok_C8);}
f16   {return(Tok_F16);}
f32   {return(Tok_F32);}
f64   {return(Tok_F64);}
bool  {return(Tok_Bool);}
void  {return(Tok_Void);}

{usertype} {return(Tok_UserType);}
{typevar}  {return(Tok_TypeVar);}


{operator}  {return(yytext[0]);}
"=="        {return(Tok_Eq);}
"!="        {return(Tok_NotEq);}
"+="        {return(Tok_AddEq);}
"-="        {return(Tok_SubEq);}
"*="        {return(Tok_MulEq);}
"/="        {return(Tok_DivEq);}
">="        {return(Tok_GrtrEq);}
"<="        {return(Tok_LesrEq);}
".."        {return(Tok_Range);}
"->"        {return(Tok_RArrow);}
"<|"        {return(Tok_ApplyL);}
"|>"        {return(Tok_ApplyR);}
"++"        {return(Tok_Append);}

or        {return(Tok_Or);}
and       {return(Tok_And);}
new       {return(Tok_New);}
not       {return(Tok_Not);}
true      {return(Tok_True);}
false     {return(Tok_False);}
return    {return(Tok_Return);}
if        {return(Tok_If);}
then      {return(Tok_Then);}
elif      {return(Tok_Elif);}
else      {return(Tok_Else);}
for       {return(Tok_For);}
while     {return(Tok_While);}
do        {return(Tok_Do);}
in        {return(Tok_In);}
continue  {return(Tok_Continue);}
break     {return(Tok_Break);}
import    {return(Tok_Import);}
let       {return(Tok_Let);}
var       {return(Tok_Var);}
match     {return(Tok_Match);}
with      {return(Tok_With);}
type      {return(Tok_Type);}
trait     {return(Tok_Trait);}
fun       {return(Tok_Fun);}
ext       {return(Tok_Ext);}
pub       {return(Tok_Pub);}
pri       {return(Tok_Pri);}
pro       {return(Tok_Pro);}
raw       {return(Tok_Raw);}
const     {return(Tok_Const);}
noinit    {return(Tok_Noinit);}
mut       {return(Tok_Mut);}
global    {return(Tok_Global);}


{intlit}   {return(Tok_IntLit);}
{fltlit}   {return(Tok_FltLit);}

{strlit}   {return(Tok_StrLit);}

'.'        {return(Tok_CharLit);}
'\\a'      {return(Tok_CharLit);}
'\\b'      {return(Tok_CharLit);}
'\\f'      {return(Tok_CharLit);}
'\\n'      {return(Tok_CharLit);}
'\\r'      {return(Tok_CharLit);}
'\\t'      {return(Tok_CharLit);}
'\\v'      {return(Tok_CharLit);}
'\\0'      {return(Tok_CharLit);}
'\\[0-9]+' {return(Tok_CharLit);}


{ident}   {return(Tok_Ident);}

^\ *  {
        if(yyleng == scopes.top()){
            return(Tok_Newline);
        }

        if(abs((long)yyleng - (long)scopes.top()) < 2){
            YY_FATAL_ERROR("Changes in significant whitespace cannot be less than 2 spaces in size");
        }
        
        if(yyleng > scopes.top()){
            scopes.push(yyleng);
            return(Tok_Indent);
        }else{
            scopes.pop();
            ctxt->ws_size = yyleng;
            BEGIN(UNINDENT);
            yymore();
            return(Tok_Unindent);
        }
      }

<UNINDENT>.     {
                    if(ctxt->ws_size == scopes.top()){
                        BEGIN(INITIAL);
                    }else{
                        scopes.pop();
                        yyless(0);
                        return(Tok_Unindent);
                    }
                }

\n    {}

%%
//User code

void ante::LexerCtxt::init_scanner(){
    //yylex_init(is);
    //yyset_extra(this, scanner);
}

void ante::LexerCtxt::destroy_scanner(){
    //yylex_destroy(scanner);
}

void flex_init(){
    while(!scopes.empty())
        scopes.pop();
    scopes.push(0);
}

void flex_error(const char *msg, yy::parser::location_type* loc){
    error(msg, *loc);
    exit(EXIT_FAILURE);//lexing errors are always fatal
}

namespace ante {
    namespace lexer {
        void printTok(int t){
            std::cout << getTokStr(t);
        }

        std::string getTokStr(int t){
            return IS_LITERAL(t) ? string((char)t,1) : tokDict[t];
        }
    }
}

