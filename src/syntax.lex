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
%x COMMENT
%x ML_COMMENT
%x STRLIT
%x CHARLIT

%{

#define YY_DECL int yylex(YYSTYPE* yylval_param, yy::location* yylloc_param, ante::LexerCtxt* ctxt)

#define YY_USER_ACTION ante::updateLoc(yylloc);

//#define YY_USER_ACTION yylloc.first_line = yyloc.last_line = yylineno; \
//                       yylloc.first_column = yycolumn; \
//                       yylloc.last_column = yycolumn+yyleng-1; \
//                       yycolumn += yyleng;

#define YY_INPUT(buf,result,max_size) \
{                                     \
    char c;                           \
    lexerCtxt->is->get(c);            \
    if(lexerCtxt->is->eof()){         \
        result = YY_NULL;             \
    }else{                            \
        buf[0] = c;                   \
        result = 1;                   \
    }                                 \
}

#define YYLTYPE yy::location

using namespace ante;

namespace ante {
    LexerCtxt *lexerCtxt;
    char *lextext;
    string lextext_str;

    size_t yycolumn = 1;

    int bracket_matches = 0;
    int paren_matches = 0;

    bool colored_output = true;

    void updateLoc(yy::parser::location_type* loc);

    void flex_error(const char *msg, yy::parser::location_type* loc);

    char* numdup(const char *str, size_t len);
}

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
%}

ident [a-z_][A-Za-z_0-9]*

typevar '{ident}

usertype [A-Z][A-Za-z0-9]*

intlit [0-9][0-9_]*

operator [-`~!@#$%^&*+=\{\}\\|:;/?.,<>]

%%

%{
//Rules
%}

i8    {return Tok_I8;}
i16   {return Tok_I16;}
i32   {return Tok_I32;}
i64   {return Tok_I64;}
isz   {return Tok_Isz;}
u8    {return Tok_U8;}
u16   {return Tok_U16;}
u32   {return Tok_U32;}
u64   {return Tok_U64;}
usz   {return Tok_Usz;}
c8    {return Tok_C8;}
f16   {return Tok_F16;}
f32   {return Tok_F32;}
f64   {return Tok_F64;}
bool  {return Tok_Bool;}
void  {return Tok_Void;}


"/*"       {BEGIN(ML_COMMENT);}
"//"       {BEGIN(COMMENT);}

<COMMENT>\n {yycolumn = 1; yyless(0); BEGIN(INITIAL);}
<COMMENT>.  {}

<ML_COMMENT>"*/"  {BEGIN(INITIAL);}
<ML_COMMENT>\n    {yycolumn = 1;}
<ML_COMMENT>.     {}

{operator}  {return yytext[0];}
"("         {paren_matches++; return '(';}
"["         {bracket_matches++; return '[';}
")"         {if(paren_matches) paren_matches--; return ')';}
"]"         {if(bracket_matches) bracket_matches--; return ']';}


"=="        {return Tok_Eq;}
"!="        {return Tok_NotEq;}
"+="        {return Tok_AddEq;}
"-="        {return Tok_SubEq;}
"*="        {return Tok_MulEq;}
"/="        {return Tok_DivEq;}
">="        {return Tok_GrtrEq;}
"<="        {return Tok_LesrEq;}
".."        {return Tok_Range;}
"->"        {return Tok_RArrow;}
"<|"        {return Tok_ApplyL;}
"|>"        {return Tok_ApplyR;}
"++"        {return Tok_Append;}

or        {return Tok_Or;}
and       {return Tok_And;}
new       {return Tok_New;}
not       {return Tok_Not;}
true      {return Tok_True;}
false     {return Tok_False;}
return    {return Tok_Return;}
if        {return Tok_If;}
then      {return Tok_Then;}
elif      {return Tok_Elif;}
else      {return Tok_Else;}
for       {return Tok_For;}
while     {return Tok_While;}
do        {return Tok_Do;}
in        {return Tok_In;}
continue  {return Tok_Continue;}
break     {return Tok_Break;}
import    {return Tok_Import;}
let       {return Tok_Let;}
var       {return Tok_Var;}
match     {return Tok_Match;}
with      {return Tok_With;}
type      {return Tok_Type;}
trait     {return Tok_Trait;}
fun       {return Tok_Fun;}
ext       {return Tok_Ext;}
pub       {return Tok_Pub;}
pri       {return Tok_Pri;}
pro       {return Tok_Pro;}
raw       {return Tok_Raw;}
const     {return Tok_Const;}
noinit    {return Tok_Noinit;}
mut       {return Tok_Mut;}
global    {return Tok_Global;}


{intlit}                           {lextext = numdup(yytext, yyleng); return Tok_IntLit;}
{intlit}([ui](8|16|32|64|sz)?)?    {lextext = numdup(yytext, yyleng); return Tok_IntLit;}

{intlit}\.{intlit}(f(16|32|64))?   {lextext = numdup(yytext, yyleng); return Tok_FltLit;}

\"         {lextext_str = ""; BEGIN(STRLIT);}

<STRLIT>\"        {lextext = strdup(lextext_str.c_str()); BEGIN(INITIAL); return Tok_StrLit;}
<STRLIT>\\\"      {lextext_str += '"'; }
<STRLIT>\\a       {lextext_str += '\a';}
<STRLIT>\\b       {lextext_str += '\b';}
<STRLIT>\\f       {lextext_str += '\f';}
<STRLIT>\\n       {lextext_str += '\n';}
<STRLIT>\\r       {lextext_str += '\r';}
<STRLIT>\\t       {lextext_str += '\t';}
<STRLIT>\\v       {lextext_str += '\v';}
<STRLIT>\\0       {lextext_str += '\0';}
<STRLIT>\\[0-9]+  {lextext_str += '\a';}
<STRLIT>\\\\      {lextext_str += '\\';}
<STRLIT>\\.       {YY_FATAL_ERROR("Unknown escape sequence");}
<STRLIT>\n        {yycolumn = 1; printf("Line %d:\n",yylineno); YY_FATAL_ERROR("Unterminated string");}
<STRLIT>.         {lextext_str += yytext[0];}

'   {BEGIN(CHARLIT);}

<CHARLIT>\\''       {lextext = strdup("'");  BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\a'       {lextext = strdup("\a"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\b'       {lextext = strdup("\b"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\f'       {lextext = strdup("\f"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\n'       {lextext = strdup("\n"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\r'       {lextext = strdup("\r"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\t'       {lextext = strdup("\t"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\v'       {lextext = strdup("\v"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\0'       {lextext = strdup("\0"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>'          {lextext = strdup("\0"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\[0-9]+'  {lextext = strdup("\0"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\\\'      {lextext = strdup("\\"); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>.'         {yytext[1] = '\0'; lextext = strdup(yytext); BEGIN(INITIAL); return Tok_CharLit;}
<CHARLIT>\\.'       {YY_FATAL_ERROR("Unknown escape sequence");}
<CHARLIT>\n         {yycolumn = 1; printf("Line %d:\n",yylineno); YY_FATAL_ERROR("Unterminated char literal");}

<CHARLIT>{ident}    {lextext = strdup(yytext); BEGIN(INITIAL); return Tok_TypeVar;}
<CHARLIT>{ident}'   {printf("Line %d:\n",yylineno); YY_FATAL_ERROR("Invalid char literal (too long)");}

{ident}    {lextext = strdup(yytext); return Tok_Ident;}
{usertype} {lextext = strdup(yytext); return Tok_UserType;}

\n[ ]*"//"  {yycolumn = 1; BEGIN(COMMENT);}
\n[ ]*"/*"  {yycolumn = yyleng; BEGIN(ML_COMMENT);}
\n[ ]*$     {yycolumn = 1; }

\n[ ]*[^ ]    {
                  yyless(yyleng-1);
                  yycolumn = yyleng;
                  if(!paren_matches && !bracket_matches){
                      if(yyleng-1 == lexerCtxt->scopes.top()){
                          return Tok_Newline;
                      }

                      long dif = abs((long)yyleng-1 - (long)lexerCtxt->scopes.top());
                      if(dif < 2){
                          YY_FATAL_ERROR("Changes in significant whitespace cannot be less than 2 spaces in size");
                      }
                      
                      if(yyleng-1 > lexerCtxt->scopes.top()){
                          lexerCtxt->scopes.push(yyleng-1);
                          return Tok_Indent;
                      }else{
                          lexerCtxt->scopes.pop();
                          lexerCtxt->ws_size = yyleng-1;
                          BEGIN(UNINDENT);
                          return Tok_Unindent;
                      }
                  }
              }


<UNINDENT>.|\n  {
                    yycolumn = yycolumn == 0 ? 0 : yycolumn-1;
                    if(lexerCtxt->ws_size == lexerCtxt->scopes.top()){
                        yyless(0);
                        BEGIN(INITIAL);
                        return Tok_Newline;
                    }else{
                        lexerCtxt->scopes.pop();
                        yyless(0);
                        return Tok_Unindent;
                    }
                }

[ ]* {}

\n   {yycolumn = 1;}

<<EOF>>  {
            if(0 < lexerCtxt->scopes.top()){
                lexerCtxt->scopes.pop();
                lexerCtxt->ws_size = 0;
                BEGIN(INITIAL);
                return Tok_Unindent;
            }else{
                return 0;
            }
         }

.    {
        string errstr = "Unrecognized character: '";
        errstr += yytext[0];
        errstr += "', with ascii value " + to_string((int)yytext[0]);
        YY_FATAL_ERROR(errstr.c_str());
     }


%%
//User code

void ante::LexerCtxt::init_scanner(){
    lexerCtxt = this;
    yylineno = 1;
    yycolumn = 1;
    paren_matches = 0;
    bracket_matches = 0;
    //yylex_init(is);
    //yyset_extra(this, scanner);
}

void ante::LexerCtxt::destroy_scanner(){
    //yylex_destroy(scanner);
}

void flex_error(const char *msg, yy::parser::location_type* loc){
    error(msg, *loc);
    exit(EXIT_FAILURE);//lexing errors are always fatal
    //silence yyunput unused warning
    yyunput(0,0);
    yyinput();
}

//copy a numerical string and remove _ instances
char* ante::numdup(const char *str, size_t len){
    char *buf = (char*)malloc(len+1);
    size_t buf_idx = 0;
    for(size_t i = 0; i < len; i++){
        char c = str[i];
        if(c != '_')
            buf[buf_idx++] = c;
    }
    buf[buf_idx] = '\0';
    return buf;
}

void ante::updateLoc(yy::parser::location_type* loc){
    loc->begin.filename = lexerCtxt->filename;
    loc->begin.line = yylineno;
    loc->begin.column = yycolumn;
    loc->end.filename = lexerCtxt->filename;
    loc->end.line = yylineno;
    yycolumn += yyleng;
    loc->end.column = yycolumn-1;
}

namespace ante {
    namespace lexer {
        void printTok(int t){
            std::cout << getTokStr(t);
        }

        std::string getTokStr(int t){
            return IS_LITERAL(t) ? string("")+(char)t : tokDict[t];
        }
    }
}

