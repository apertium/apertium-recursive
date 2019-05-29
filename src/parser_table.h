#ifndef __PARSERTABLE__
#define __PARSERTABLE__

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <reduction_rule.h>
#include <rtx_compiler.h>

class ParserTable
{
private:
  bool fromFile;
  vector<vector<vector<pair<int, int>>>> actions;
  vector<vector<int>> gotos;
  map<wstring, int> terminalSymbolMap;
  map<wstring, int> nonterminalSymbolMap;
  vector<ReductionRule*> rules;
public:
  static int const SHIFT;
  static int const REDUCE;
  static int const OUTPUT;

  ParserTable(vector<wstring> terminals, vector<wstring> nonterminals, Compiler* comp);
  ParserTable(string fname);
  ~ParserTable();
  void serialize(string fname);
};

#endif
