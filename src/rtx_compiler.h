#ifndef _RTXCOMPILER_
#define _RTXCOMPILER_

#include <rtx_parser.h>
#include <reduction_rule.h>

using namespace std;

class Compiler
{
private:
  vector<ProductionRule*> allProductions;
  map<wstring, vector<wstring>> firsts;
  map<wstring, vector<wstring>> follows;
  vector<wstring> allSymbols;
  vector<vector<pair<ProductionRule*, wstring>>> LR1items;
  void gatherSymbols(const vector<vector<wstring>>& symbols);
  void computeFirsts();
  void computeFollows();
  vector<pair<ProductionRule*, wstring>> closure(const vector<pair<ProductionRule*, wstring>>& I);
  vector<pair<ProductionRule*, wstring>> findGoto(const vector<pair<ProductionRule*, wstring>>& I, wstring X);
  void getLR1items();
public:
  Compiler(string fname);
  ~Compiler();
};

#endif
