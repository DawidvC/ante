#ifndef LEXER_H
#define LEXER_H

#include "tokens.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <stack>
#include <map>
using namespace std;

namespace ante { struct Node; }
#ifndef YYSTYPE
#  define YYSTYPE ante::Node*
#endif

namespace ante {
    extern bool colored_output;

    struct LexerCtxt {
        std::stack<int> scopes;
        int ws_size;
        istream *is;
        string *filename;

        LexerCtxt(istream *i, string *f) : ws_size(0), is(i), filename(f){
            init_scanner();
            scopes.push(0);
        }

        ~LexerCtxt(){
            destroy_scanner();
        }

        void init_scanner();
        void destroy_scanner();
    };

    namespace lexer {
        void printTok(int t);
        string getTokStr(int t);
    }
}

#include "yyparser.h"

#endif
