%{
#include <vector>
#include <string>
#include <map>
#include <iostream>

using namespace std;

class ParserNode
{
public:
  int outputMode;
  wstring defaultOutput;
  wstring nodeType;
  vector<ParserNode*> children;
  map<wstring, vector<wstring>> variables; //0 = tl, 1 = an, 2 = sl
                                           //take first non-empty or L""
  ParserNode(vector<ParserNode*> children, int outId)
  {
    this->children = children;
    outputMode = outId;
  }
  ParserNode()
  {
  }
  wstring getVar(wstring var)
  {
    if(variables.find(var) == variables.end())
    {
      return L"";
    }
    for(int i = 0; i < 3; i++)
    {
      if(variables[var][i] != L"")
      {
        return variables[var][i];
      }
    }
    return L"";
  }
  void setVar(wstring var, wstring val, int loc = -1)
  {
    if(variables.find(var) == variables.end())
    {
      variables.insert(pair<wstring, vector<wstring>>(var, vector<wstring>(3, val)));
    }
    else if(0 <= loc && loc <= 2)
    {
      variables[var][loc] = val;
    }
    else
    {
      variables[var][0] = val;
    }
  }
  void outputLU()
  {
    @@LEXOUTPUT@@
    else
    {
      wcout << defaultOutput;
    }
  }
  void output()
  {
    switch(outputMode)
    {
      case -2: wcout << defaultOutput; break;
      case -1: outputLU(); break;
      @@NODEOUTPUT@@
      default: wcout << L" "; break;
      // that's an error of some kind
    }
  }
};

int yylex(void);
void yyerror(const char *s);

%}

%union {
  ParserNode* node;
}

%glr-parser

//%language "C++"

%start GLUE

@@TOKENS@@

%%

@@TREES@@

%%

map<wstring, vector<wstring>> AllAttributes;

map<wstring, int> AllTokens;

void
readLU(int pos, ParserNode* node)
{
  wstring wholeString = L"";
  wstring lemma = L"";
  while(wcin.peek() != L'/' && wcin.peek() != L'<')
  {
    // @TODO: gets stuck in infinite loop here
    // upon encountering non-Latin character (e.g. n~)
    if(wcin.peek() == L'\\')
    {
      lemma.append(1, wcin.get());
    }
    lemma.append(1, wcin.get());
  }
  wholeString = lemma;
  node->setVar(L" lemma", lemma, pos);
  wstring tag;
  bool isFirst = true;
  while(wcin.peek() == L'<')
  {
    wholeString.append(1, wcin.get());
    tag = L"";
    while(wcin.peek() != L'>')
    {
      tag.append(1, wcin.get());
    }
    wholeString += tag;
    wholeString.append(1, wcin.get());
    if(AllAttributes.find(tag) != AllAttributes.end())
    {
      for(int i = 0; i < AllAttributes[tag].size(); i++)
      {
         node->setVar(AllAttributes[tag][i], tag, pos);
      }
    }
    if(pos == 0 && isFirst)
    {
      node->nodeType = L"@" + tag;
      isFirst = false;
    }
  }
  if(pos == 0)
  {
    node->defaultOutput = wholeString;
    if(isFirst)
    {
      node->nodeType = L"*";
    }
  }
}

int
yylex()
{
  //@TODO: check for eof
  yylval.node = new ParserNode();
  if(wcin.peek() != L'^')
  {
    yylval.node->outputMode = -2;
    yylval.node->nodeType = L"_";
    yylval.node->defaultOutput = L"";
    while(wcin.peek() != L'^')
    {
      yylval.node->defaultOutput.append(1, wcin.get());
    }
    return _;
  }
  else
  {
    yylval.node->outputMode = -1;
    wcin.get(); // L'^'
    readLU(2, yylval.node);
    wcin.get(); // should be L'/' @TODO: check
    readLU(0, yylval.node);
    if(wcin.peek() == L'/')
    {
      wcin.get();
      readLU(1, yylval.node);
    }
    wcin.get(); // should be L'$' @TODO: check
  }
  if(AllTokens.find(yylval.node->nodeType) != AllTokens.end())
  {
    return AllTokens[yylval.node->nodeType];
  }
  return UNKNOWN;
}

int
main(int argc, char* argv[])
{
  @@TOKENS2@@
  yyparse();
  yylval.node->output();
  return 0;
}

void yyerror(const char *s) {
    cout << "EEK, parse error!  Message: " << s << endl;
    // might as well halt now:
    exit(-1);
}
