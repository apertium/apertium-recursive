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
  void gatherSymbols(vector<vector<wstring>> symbols);
  void computeFirsts();
  void computeFollows();
  vector<pair<ProductionRule*, wstring>> closure(vector<pair<ProductionRule*, wstring>> I);
public:
  Compiler(string fname);
  ~Compiler();
};

#endif
