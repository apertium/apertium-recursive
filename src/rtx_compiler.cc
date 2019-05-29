#include <rtx_compiler.h>

#include <cstdlib>
#include <iostream>
#include <algorithm>

using namespace std;

void
printItem(pair<ProductionRule*, wstring> item)
{
  wcout << item.first->result << L"\t->";
  for(int k = 0; k < item.first->pieces.size(); k++)
  {
    if(k == item.first->dot)
    {
      wcout << L" .";
    }
    wcout << L" " << item.first->pieces[k];
  }
  if(item.first->dot == item.first->pieces.size())
  {
    wcout << L" .";
  }
  wcout << L" | " << item.second << endl;
}

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
  getLR1items();
  for(int i = 0; i < LR1items.size(); i++)
  {
    wcout << L"I" << i << L":" << endl;
    for(int j = 0; j < LR1items[i].size(); j++)
    {
      wcout << L"\t";
      printItem(LR1items[i][j]);
    }
  }
}

Compiler::~Compiler()
{
}

void
Compiler::gatherSymbols(const vector<vector<wstring>>& symbols)
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
Compiler::closure(const vector<pair<ProductionRule*, wstring>>& inset)
{
  vector<pair<ProductionRule*, wstring>> ret;
  for(int i = 0; i < inset.size(); i++)
  {
    ret.push_back(inset[i]);
  }
  bool addedAny = true;
  ProductionRule* cur;
  ProductionRule* comp;
  wstring sym;
  vector<wstring> first;
  bool inThere;
  while(addedAny)
  {
    addedAny = false;
    for(int i = 0; i < ret.size(); i++)
    {
      cur = ret[i].first;
      if(cur->dot == cur->pieces.size())
      {
        continue;
      }
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
        // closure() should only add productions with dot=0
        p += comp->pieces.size();
      }
    }
  }
  return ret;
}

vector<pair<ProductionRule*, wstring>>
Compiler::findGoto(const vector<pair<ProductionRule*, wstring>>& I, wstring X)
{
  vector<pair<ProductionRule*, wstring>> J;
  ProductionRule* cur;
  ProductionRule* next;
  for(int n = 0; n < I.size(); n++)
  {
    cur = I[n].first;
    if(cur->dot < cur->pieces.size() && cur->pieces[cur->dot] == X)
    {
      next = *(find(allProductions.begin(), allProductions.end(), cur)+1);
      J.push_back(pair<ProductionRule*, wstring>(next, I[n].second));
    }
  }
  if(J.size() == 0)
  {
    return J;
  }
  return closure(J);
}

bool
setEQ(const vector<pair<ProductionRule*, wstring>>& a, const vector<pair<ProductionRule*, wstring>>& b)
{
  if(a.size() != b.size())
  {
    return false;
  }
  bool any;
  for(int ai = 0; ai < a.size(); ai++)
  {
    any = false;
    for(int bi = 0; bi < b.size(); bi++)
    {
      if(a[ai].first == b[bi].first && a[ai].second == b[bi].second)
      {
        any = true;
        break;
      }
    }
    if(!any)
    {
      return false;
    }
  }
  // all a are in b and same size
  // closure() doesn't produce duplicates, so we're done
  return true;
}

void
Compiler::getLR1items()
{
  vector<pair<ProductionRule*, wstring>> cur;
  for(int i = 0; i < allProductions.size(); i++)
  {
    if(allProductions[i]->result == L"START" && allProductions[i]->dot == 0)
    {
      cur.push_back(pair<ProductionRule*, wstring>(allProductions[i], L"$"));
      LR1items.push_back(closure(cur));
      break;
    }
  }
  bool found;
  for(int i = 0; i < LR1items.size(); i++)
  {
    for(int s = 0; s < allSymbols.size(); s++)
    {
      cur = findGoto(LR1items[i], allSymbols[s]);
      if(cur.size() == 0)
      {
        continue;
      }
      found = false;
      for(int l = 0; l < LR1items.size(); l++)
      {
        if(setEQ(cur, LR1items[l]))
        {
          found = true;
          break;
        }
      }
      if(!found)
      {
        LR1items.push_back(cur);
      }
    }
  }
}
