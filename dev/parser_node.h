#ifndef _RTXPARSERNODE_
#define _RTXPARSERNODE_

#include <vector>
#include <string>
#include <map>

using namespace std;

class ParserNode
{
public:
  wstring defaultOutput;
  wstring nodeType;
  vector<ParserNode*> children;
  void setVar(wstring name, wstring val, int pos);
protected:
  map<wstring, vector<wstring>> variables; //0 = tl, 1 = an, 2 = sl
                                           //take first non-empty or L""
};

#endif
