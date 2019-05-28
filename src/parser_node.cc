#include <parser_node.h>
#include <string>
#include <map>
#include <vector>

using namespace std;

void
ParserNode::setVar(wstring name, wstring val, int pos=-1)
{
  if(variables.find(name) == variables.end())
  {
    variables[name] = vector<wstring>(3, L"");
  }
  if(pos < 0 || pos > 2)
  {
    pos = 0;
  }
  variables[name][pos] = val;
}
