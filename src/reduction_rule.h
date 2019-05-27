#ifndef _RTXREDUCTIONRULE_
#define _RTXREDUCTIONRULE_

#include <vector>
#include <string>

using namespace std;

class ReductionRule
{
public:
  float weight;
  vector<vector<wstring>> pattern;
  vector<wstring> resultNodes;
  vector<vector<wstring>> resultVars;
  vector<vector<pair<int, wstring>>> variableGrabs;
  vector<pair<pair<int, wstring>, pair<int, wstring>>> variableUpdates;
  //@TODO: conditions
};

#endif
