%{
#include "types.h"
#include "parser.eng-kaz.tab.h"

#define YY_DECL extern "C" int yylex()
#define ok yylval.term=strdup(yytext)

%}
%%
[ \t\n]         	;

PYTHON_REPLACEMENT_BLOCK

%%
