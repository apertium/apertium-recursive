#include <parser_table.h>

int const ParserTable::SHIFT  = 0;
int const ParserTable::REDUCE = 1;
int const ParserTable::OUTPUT = 2;

ParserTable::ParserTable(vector<wstring> terminals, vector<wstring> nonterminals, Compiler* comp)
{
  fromFile = false;
  actions.resize(terminals.size());
  gotos.resize(nonterminals.size());
  for(int i = 0; i < terminals.size(); i++)
  {
    terminalSymbolMap.insert(pair<wstring, int>(terminals[i], i));
  }
  for(int i = 0; i < nonterminals.size(); i++)
  {
    nonterminalSymbolMap.insert(pair<wstring, int>(nonterminals[i], i));
  }
  vector<pair<ProductionRule*, wstring>> curset;
  vector<vector<pair<int, int>>> curact;
  vector<int> curgoto;
  ProductionRule* curprod;
  wstring cursym;
  int curgotoval;
  for(int i = 0; i < comp->LR1items.size(); i++)
  {
    curset = comp->LR1items[i];
    curact.clear();
    curact.resize(terminals.size());
    curgoto.clear();
    curgoto.resize(nonterminals.size());
    for(int j = 0; j < curset.size(); j++)
    {
      curprod = curset[j].first;
      if(curprod->dot != curprod->pieces.size())
      {
        cursym = curprod->pieces[curprod->dot];
        curgotoval = comp->gotoMap[pair<int, wstring>(i, cursym)];
        if(terminalSymbolMap.find(cursym) != terminalSymbolMap.end() && curgotoval != -1)
        {
          curact[terminalSymbolMap[cursym]].push_back(pair<int, int>(SHIFT, curgotoval));
        }
      }
      else if(curprod->result == L"START")
      {
        if(curset[j].second == L"$")
        {
          curact[terminalSymbolMap[L"$"]].push_back(pair<int, int>(OUTPUT, 0));
        }
      }
      else
      {
        curact[terminalSymbolMap[curset[j].second]].push_back(pair<int, int>(REDUCE, curprod->parentID));
      }
    }
    for(int s = 0; s < terminals.size(); s++)
    {
      if(actions[i][s].size() == 0)
      {
        curact[s].push_back(pair<int, int>(OUTPUT, 1));
      }
    }
    actions.push_back(curact);
    for(int s = 0; s < nonterminals.size(); s++)
    {
      curgoto[s] = comp->gotoMap[pair<int, wstring>(i, nonterminals[s])];
    }
    gotos.push_back(curgoto);
  }
}

ParserTable::ParserTable(string fname)
{
  wifstream file;
  file.open(fname);
  // deserialize
  file.close();
}

ParserTable::~ParserTable()
{
  if(fromFile)
  {
    for(int i = 0; i < rules.size(); i++)
    {
      delete rules[i];
    }
  }
}

void
ParserTable::serialize(string fname)
{
  wofstream file;
  file.open(fname);
  file.close();
}
