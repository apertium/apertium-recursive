#ifndef _RTXREDUCTIONRULE_
#define _RTXREDUCTIONRULE_

#include <vector>
#include <string>

using namespace std;

// stuff needed to construct the parsing table
class ProductionRule
{
public:
  vector<wstring> pieces;
  int dot;
  wstring result;
  // it may take some doing to figure out how to work with multiple outputs
  int parentID;
};

class ReductionRule
{
public:
  int ID;
  float weight;
  int patternLength;
  vector<vector<wstring>> pattern;
  vector<wstring> resultNodes;
  vector<vector<wstring>> resultVars;
  vector<vector<wstring>> resultContents;
  vector<vector<pair<int, wstring>>> variableGrabs;
  vector<pair<pair<int, wstring>, pair<int, wstring>>> variableUpdates;
  //@TODO: conditions
  
  vector<ProductionRule*> prodRules;
  void process();
};

#endif
