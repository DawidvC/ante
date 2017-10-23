#include "repl.h"
#include "target.h"

#ifdef unix
#  include <unistd.h>
#  include <termios.h>
#  include <sys/ioctl.h>
#endif

using namespace std;
using namespace ante;
using namespace ante::parser;

extern char* lextxt;

namespace ante {

#ifdef unix
    winsize termSize;
#endif

#ifdef WIN32
#  include <windows.h>
#  define getchar getchar_windows

	HANDLE h_in, h_out;
	DWORD cc, normal_mode, getch_mode;

	TCHAR getchar_windows() {
		TCHAR c = 0;
		SetConsoleMode(h_in, getch_mode);
		ReadConsole(h_in, &c, 1, &cc, NULL);
		SetConsoleMode(h_in, normal_mode);
		return c;
	}

	void clearline_windows() {
		DWORD numCharsWritten;
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		// Get the number of character cells in the current buffer.
		if (!GetConsoleScreenBufferInfo(h_out, &csbi)){
			cerr << "Cannot get screen buffer info" << endl;
			return;
		}

		COORD homeCoords = { 0, csbi.dwCursorPosition.Y };
		DWORD cellsToWrite = csbi.dwSize.X;

		// Fill the entire screen with blanks.
		if (!FillConsoleOutputCharacter(h_out, (TCHAR) ' ', cellsToWrite, homeCoords, &numCharsWritten)) {
			cerr << "Error when attempting to clear screen" << endl;
			return;
		}

		// Get the current text attribute.
		if (!GetConsoleScreenBufferInfo(h_out, &csbi)) {
			cerr << "Error when getting screen buffer info" << endl;
			return;
		}

		// Set the buffer's attributes accordingly.
		if (!FillConsoleOutputAttribute(h_out, csbi.wAttributes, cellsToWrite, homeCoords, &numCharsWritten)) {
			cerr << "Error when attempting to fill attributes" << endl;
			return;
		}

		SetConsoleCursorPosition(h_out, homeCoords);
	}
#endif

    string getInputColorized(){
        string line = "";

        cout << ": " << flush;
        char inp = getchar();

        while(inp and inp != '\n' and inp != '\r'){
            if(inp == '\b' or inp == 127){
                if(!line.empty())
                    line = line.substr(0, line.length() - 1);
            }else if(inp == '\033'){
                getchar();
                inp = getchar();
                continue;
            }else{
                line += inp;
            }

#ifdef unix
			printf("\033[2K\r: ");
#elif defined(WIN32)
			clearline_windows();
			cout << ": ";
#endif

			LOC_TY loc;
			auto *l = new Lexer(nullptr, line, 1, 1, true);
			while (l->next(&loc));

            inp = getchar();
        }
        puts("");
        return line;
    }

    void setupTerm(){
#ifdef unix
        termios newt;
        tcgetattr(STDIN_FILENO, &newt);
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        ioctl(0, TIOCGWINSZ, &termSize);
#elif defined WIN32
		h_in = GetStdHandle(STD_INPUT_HANDLE);
		h_out = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!h_in or !h_out) {
			fputs("Error when attempting to access windows terminal\n", stderr);
			exit(1);
		}
		GetConsoleMode(h_in, &normal_mode);
		getch_mode = normal_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
#endif
    }


    void startRepl(Compiler *c){
        cout << "Ante REPL v0.0.6\nType 'exit' to exit.\n";
        setupTerm();

        auto cmd = getInputColorized();

        while(cmd != "exit"){
            int flag;
            //Catch any lexing errors
            try{
                //lex and parse the new string
                setLexer(new Lexer(nullptr, cmd, /*line*/1, /*col*/1));
                yy::parser p{};
                flag = p.parse();
            }catch(CtError *e){
                delete e;
                continue;
            }

            if(flag == PE_OK){
                RootNode *expr = parser::getRootNode();

                //Compile each expression and hold onto the last value
                TypedValue val = c->ast ? mergeAndCompile(c, expr)
                                        : (c->ast.reset(expr), expr->compile(c));

                //print val if it's not an error
                if(!!val and val.type->typeTag != TT_Void)
                    val.dump();
            }

            cmd = getInputColorized();
        }
    }

    TypedValue mergeAndCompile(Compiler *c, RootNode *rn){
        scanImports(c, rn);
        move(rn->imports.begin(),
            next(rn->imports.begin(), rn->imports.size()),
            back_inserter(c->ast->imports));

        for(auto &t : rn->types){
            safeCompile(c, t);
            c->ast->types.emplace_back(move(t));
        }

        for(auto &t : rn->traits){
            safeCompile(c, t);
            c->ast->traits.emplace_back(move(t));
        }

        for(auto &t : rn->extensions){
            safeCompile(c, t);
            c->ast->extensions.emplace_back(move(t));
        }

        for(auto &t : rn->funcs){
            safeCompile(c, t);
            c->ast->funcs.emplace_back(move(t));
        }

        TypedValue ret;
        for(auto &e : rn->main){
            ret = safeCompile(c, e);
            c->ast->main.emplace_back(move(e));
        }
        return ret;
    }
}
