%option noyywrap
%option yylineno

%{
#include "tokens.h"
#include "lexer.h"
#include <stack>
%}

%{
using namespace ante;
size_t yycolumn  = 1;

std::stack<unsigned int> scopes;

void flex_init();
void flex_error(const char *msg, yy::parser::location_type* loc);

//#define YY_USER_ACTION yylloc.first_line = yyloc.last_line = yylineno; \
//                       yylloc.first_column = yycolumn; \
//                       yylloc.last_column = yycolumn+yyleng-1; \
//                       yycolumn += yyleng;
%}

typevar '[a-z]\w*

usertype [A-Z]\w*

ident [a-z]\w*

strlit \".*\"

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


{strlit}  {return(Tok_StrLit);}

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
            perror("Changes in significant whitespace cannot be less than 2 spaces in size");
        }
        
        if(yyleng > scopes.top()){
            scopes.push(yyleng);
            return(Tok_Indent);
        }else{
            scopes.pop();
            //TODO: enter unindent state for multiple unindents
            return(Tok_Unindent);
        }
      }
\n    {yycolumn = 1;}

%%
//User code

void flex_init(){
    while(!scopes.empty())
        scopes.pop();
    scopes.push(0);
}

void flex_error(const char *msg, yy::parser::location_type* loc){
    error(msg, *loc);
    exit(EXIT_FAILURE);//lexing errors are always fatal
}
