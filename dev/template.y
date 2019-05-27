%{
#include <iostream>
#include <cstdio>
#include "types.h"
#include <string>
#include <vector>
#include <map>
#include <apertium/apertium_re.h>
#include <sstream>
#include <cstdarg> //arbitrary number of parameters

using  namespace std;

extern "C" int yylex();
extern "C" int yyparse();
extern "C" FILE *yyin;
void yyerror(const char *s);

void doAction(node_pair *np);
void printOutput(node_pair *np, string side, bool tl_only);
void printTreeErr(node_pair *np, string side, bool tl_only);

map<string, string> attr_regexps; 
map<string, ApertiumRE> attr_items ; 

%}

%start SS

%union {
        char *term;
        node_pair *non_term;
}

//BEGIN PYTHON INSERTION
PYTHON_REPLACEMENT_SPOT1
//END PYTHON INSERTION

%%

SS:
        S sent {doAction($1); cout << endl;}
        ;

//BEGIN PYTHON INSERTION
PYTHON_REPLACEMENT_SPOT2
//END_PYTHON_INSERTION

%%

int typeOfAction = 0;

int main(int argc, char* argv[]) 
{
    // BEGIN PYTHON INSERTION
    PYTHON_REPLACEMENT_SPOT3
    // END PYTHON INSERTION
      
    for(map<string,string>::iterator it = attr_regexps.begin(); it != attr_regexps.end(); it++)
    { 
        ApertiumRE my_re;
        attr_items[it->first] = my_re;
        attr_items[it->first].compile(it->second);
    }
    for(int i=1;i<argc;i++)
    {
        string ar = argv[i];
        if(ar == "-t")
        {
            typeOfAction += 1;  // 001bin
        }
        else if(ar == "-p")
        {
            typeOfAction += 2;  // 010bin
        }
        else if(ar == "-s")
        {
            typeOfAction += 4;  // 100bin
        }
    }
    do {
            yyparse();
    } while (!feof(yyin));
    return 0;
}

void doAction(node_pair *np){
    if (typeOfAction == 5 || typeOfAction == 7)  // 101bin, 111bin
    {
        cerr << "Unimplemented set of options: -s -t" << endl; // TODO
        exit(-1);
    }

    if(typeOfAction & 1){  // typeOfAction == 1 || typeOfAction == 3  // -t; -t -p
        printOutput(np, "tl", true);
        if((typeOfAction >> 1) & 1){  // typeOfAction == 3  // -t -p
            printTreeErr(np, "tl", true);
        }
    }
    else if(typeOfAction >> 2 & 1){  // typeOfAction == 4 || typeOfAction == 6  // -s; -s -p
        printOutput(np, "sl", false);
        if((typeOfAction >> 1) & 1){  // typeOfAction == 6  // -s -p
            printTreeErr(np, "sl", false);
        }
    }
    else{  // typeOfAction == 0 || typeOfAction == 2  // <no_options>; -p
        printOutput(np, "tl", false);
        if((typeOfAction >> 1) & 1){  // typeOfAction == 2  // -p
            printTreeErr(np, "tl", false);
        }
    }
}

string targetLang(string s){
    return "^" + s.substr(s.find("/") + 1);
}

// string side : "sl", "tl"
void printOutput(node_pair *np, string side, bool tl_only){
    node *n;
    //cerr << "! " << np->sl->token << " " << endl ; 
    if(side == "sl")
    {
        n = np->sl;
    }
    else if(side == "tl")
    {
        n = np->tl;
    }

    if(n->leaf)
    {
        string s = n->token;
        if(tl_only) s = targetLang(s);

        cout << s << " ";
    }
    for(int i=0; i < n->children.size(); i++){
        if(n->children[i])
        {
          printOutput(n->children[i], side, tl_only);
        }
    }
}

// string side : "sl", "tl"
void printTreeErr(node_pair *np, string side, bool tl_only){
    node *n;
    if(side == "sl"){
        n = np->sl;
    }
    else if(side == "tl"){
        n = np->tl;
    }

    string s = n->token;
    if(tl_only && n->leaf) s = targetLang(s);

    cerr << "(" << s;
    for(int i=0; i < n->children.size(); i++){
        cerr << " ";

        printTreeErr(n->children[i], side, tl_only);
    }
    cerr << ")";
}

void yyerror(const char *s) {
    cout << "EEK, parse error!  Message: " << s << endl;
    // might as well halt now:
    exit(-1);
}

