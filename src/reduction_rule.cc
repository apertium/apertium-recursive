#include <reduction_rule.h>

using namespace std;

void
ReductionRule::process()
{
  vector<wstring> pieces;
  bool found;
  for(int i = 0; i < pattern.size(); i++)
  {
    found = false;
    for(int j = 0; j < pattern[i].size(); j++)
    {
      if(pattern[i][j] == L"@")
      {
        found = true;
        pieces.push_back(L"@" + pattern[i][j+1]);
        break;
      }
    }
    if(!found)
    {
      pieces.push_back(pattern[i][0]);
    }
  }
  ProductionRule* cur;
  for(int i = 0; i <= pieces.size(); i++)
  {
    cur = new ProductionRule();
    cur->pieces = pieces;
    cur->dot = i;
    cur->result = resultNodes[0];
    prodRules.push_back(cur);
  }
}
