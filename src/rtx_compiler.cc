#include <rtx_compiler.h>

#include <cstdlib>
#include <iostream>
#include <algorithm>

using namespace std;

Compiler::Compiler(string fname)
{
  Parser p;
  p.parse(fname);
  vector<wstring> res;
  vector<vector<wstring>> pats;
  for(int i = 0; i < p.reductionRules.size(); i++)
  {
    res.push_back(p.reductionRules[i]->prodRules[0]->result);
    pats.push_back(p.reductionRules[i]->prodRules[0]->pieces);
    allProductions.insert(allProductions.end(),
        p.reductionRules[i]->prodRules.begin(),
        p.reductionRules[i]->prodRules.end());
  }
  pats.push_back(res);
  gatherSymbols(pats);
  computeFirsts();
  computeFollows();
  for(int i = 0; i < allSymbols.size(); i++)
  {
    wcout << allSymbols[i] << endl;
    wcout << L"\tfirst:";
    for(int j = 0; j < firsts[allSymbols[i]].size(); j++)
    {
      wcout << L" " << firsts[allSymbols[i]][j];
    }
    wcout << endl << L"\tfollow:";
    for(int j = 0; j < follows[allSymbols[i]].size(); j++)
    {
      wcout << L" " << follows[allSymbols[i]][j];
    }
    wcout << endl;
  }
}

Compiler::~Compiler()
{
}

void
Compiler::gatherSymbols(vector<vector<wstring>> symbols)
{
  map<wstring, bool> added;
  for(int i = 0; i < symbols.size(); i++)
  {
    for(int j = 0; j < symbols[i].size(); j++)
    {
      if(added[symbols[i][j]] != true)
      {
        allSymbols.push_back(symbols[i][j]);
        added[symbols[i][j]] = true;
      }
    }
  }
  allSymbols.push_back(L"$");
}

vector<wstring>
appendNoDup(vector<wstring> src, vector<wstring> dest)
{
  for(int i = 0; i < src.size(); i++)
  {
    if(find(dest.begin(), dest.end(), src[i]) == dest.end())
    {
      dest.push_back(src[i]);
    }
  }
  return dest;
}

void
Compiler::computeFirsts()
{
  map<wstring, bool> done;
  int doneCount = 0;
  for(int i = 0; i < allSymbols.size(); i++)
  {
    done[allSymbols[i]] = false;
    if(allSymbols[i][0] == L'@' || allSymbols[i] == L"$")
    {
      firsts[allSymbols[i]].push_back(allSymbols[i]);
      done[allSymbols[i]] = true;
      doneCount += 1;
    }
  }
  bool alldone;
  wstring cur, comp;
  while(doneCount < allSymbols.size())
  {
    for(int i = 0; i < allSymbols.size(); i++)
    {
      alldone = true;
      cur = allSymbols[i];
      if(done[cur])
      {
        continue;
      }
      for(int j = 0; j < allProductions.size(); j++)
      {
        if(allProductions[j]->result == cur)
        {
          comp = allProductions[j]->pieces[0];
          if(!done[comp])
          {
            alldone = false;
          }
          firsts[cur] = appendNoDup(firsts[comp], firsts[cur]);
        }
        // skip duplicate rules
        j += allProductions[j]->pieces.size();
      }
      if(alldone)
      {
        done[cur] = true;
        doneCount++;
      }
    }
  }
}

void
Compiler::computeFollows()
{
  ProductionRule* curprod;
  follows[L"START"].push_back(L"$");
  for(int i = 0; i < allProductions.size(); i++)
  {
    curprod = allProductions[i];
    if(curprod->pieces.size() > 1)
    {
      for(int j = 0; j < curprod->pieces.size()-1; j++)
      {
        follows[curprod->pieces[j]] = appendNoDup(firsts[curprod->pieces[j+1]], follows[curprod->pieces[j]]);
      }
    }
    // skip duplicate rules
    i += curprod->pieces.size();
  }
  bool addedAny = true;
  int sizeWas, numPieces;
  while(addedAny)
  {
    addedAny = false;
    for(int i = 0; i < allProductions.size(); i++)
    {
      curprod = allProductions[i];
      numPieces = curprod->pieces.size();
      sizeWas = follows[curprod->pieces[numPieces-1]].size();
      follows[curprod->pieces[numPieces-1]] = appendNoDup(follows[curprod->result], follows[curprod->pieces[numPieces-1]]);
      if(follows[curprod->pieces[numPieces-1]].size() > sizeWas)
      {
        addedAny = true;
      }
      i += numPieces;
    }
  }
}

vector<pair<ProductionRule*, wstring>>
Compiler::closure(vector<pair<ProductionRule*, wstring>> inset)
{
  vector<pair<ProductionRule*, wstring>> ret = inset;
  bool addedAny = true;
  ProductionRule* cur;
  ProductionRule* comp;
  wstring sym;
  vector<wstring> first;
  bool inThere;
  while(addedAny)
  {
    for(int i = 0; i < ret.size(); i++)
    {
      cur = ret[i].first;
      for(int p = 0; p < allProductions.size(); p++)
      {
        comp = allProductions[p];
        if(comp->result != cur->pieces[cur->dot])
        {
          p += comp->pieces.size();
          continue;
        }
        if(cur->dot+1 < cur->pieces.size())
        {
          first = firsts[cur->pieces[cur->dot+1]];
        }
        else
        {
          first = firsts[ret[i].second];
        }
        for(int s = 0; s < first.size(); s++)
        {
          sym = first[s];
          inThere = false;
          for(int pr = 0; pr < ret.size(); pr++)
          {
            if(ret[pr].first == comp && ret[pr].second == sym)
            {
              inThere = true;
            }
          }
          if(!inThere)
          {
            ret.push_back(pair<ProductionRule*, wstring>(comp, sym));
            addedAny = true;
          }
        }
      }
    }
  }
  return ret;
}
