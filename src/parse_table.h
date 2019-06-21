#ifndef __RTXPARSETABLE__
#define __RTXPARSETABLE__

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

using namespace std;

class ParseTable
{
private:
  int checkSymbol;
  int any_char;
  int any_tag;
  Transducer* t;
  Alphabet* a;
  map<int, map<int, vector<int>>> trans;
  // final states -> rules
  map<int, int> rules;
  map<int, double> ruleWeights;
  void appendUnique(vector<pair<int, int>>& dest, const vector<pair<int, int>>& src)
  {
    bool found;
    //for(unsigned int i = 0; i < src.size(); i++)
    int l2 = dest.size();
    for(unsigned int i = 0, l1 = src.size(); i < l1; i++)
    {
      found = false;
      //for(unsigned int j = 0; j < dest.size(); j++)
      for(unsigned int j = 0; j < l2; j++)
      {
        if(src[i].first == dest[j].first)
        {
          found = true;
          break;
        }
      }
      if(!found)
      {
        dest.push_back(src[i]);
      }
    }
  }
  bool pairLT(const pair<int, int>& a, const pair<int, int>& b)
  {
    return a.second > b.second;
    // we want longer paths first
  }
  static bool transSort1(const pair<int, vector<int>>& a, const pair<int, vector<int>>& b)
  {
    return a.first < b.first;
  }
  static bool transSort2(const pair<int, vector<pair<int, vector<int>>>>& a,
                         const pair<int, vector<pair<int, vector<int>>>>& b)
  {
    return a.first < b.first;
  }
  vector<pair<int, vector<pair<int, vector<int>>>>> trans_vec;
  vector<int> final_vec;
  void stepSingle(int state, int sym, vector<int>& writeTo)
  {
    int lstate = 0;
    int rstate = trans_vec.size()-1;
    int loc1 = -1;
    while(lstate <= rstate)
    {
      loc1 = (lstate + rstate) / 2;
      if(trans_vec[loc1].first == state)
      {
        break;
      }
      else if(trans_vec[loc1].first < state)
      {
        lstate = loc1 + 1;
      }
      else
      {
        rstate = loc1 - 1;
      }
    }
    if(loc1 == -1 || trans_vec[loc1].first != state)
    {
      return;
    }
    vector<pair<int, vector<int>>>& tmp = trans_vec[loc1].second;
    lstate = 0;
    rstate = tmp.size()-1;
    int loc2 = -1;
    while(lstate <= rstate)
    {
      loc2 = (lstate + rstate) / 2;
      if(tmp[loc2].first == sym)
      {
        writeTo.insert(writeTo.end(), tmp[loc2].second.begin(), tmp[loc2].second.end());
        return;
      }
      else if(tmp[loc2].first < sym)
      {
        lstate = loc2 + 1;
      }
      else
      {
        rstate = loc2 - 1;
      }
    }
  }
  inline bool isFinal(int state)
  {
    return binary_search(final_vec.begin(), final_vec.end(), state);
  }
public:
  ParseTable(Transducer* tr, Alphabet* al, map<int, int> rl, map<int, double>* rw) :
      t(tr), a(al), rules(rl), ruleWeights(*rw)
  {
    any_char = (*a)(L"<ANY_CHAR>");
    any_tag = (*a)(L"<ANY_TAG>");
    checkSymbol = (*a)(L"<LOOK:AHEAD>");
    set<int> temp;
    map<int, multimap<int, pair<int, double>>> ts = tr->getTransitions();
    for(map<int, multimap<int, pair<int, double>>>::iterator state = ts.begin(), limit1 = ts.end();
            state != limit1; state++)
    {
      vector<pair<int, vector<int>>>* tv_temp = new vector<pair<int, vector<int>>>();
      for(multimap<int, pair<int, double>>::iterator it = state->second.begin(), limit2 = state->second.end();
            it != limit2; it++)
      {
        temp = tr->closure(it->second.first);
        vector<int>* tv_temp2 = new vector<int>();
        for(set<int>::iterator ch = temp.begin(); ch != temp.end(); ch++)
        {
          if(find(trans[state->first][it->first].begin(), trans[state->first][it->first].end(), *ch)
              == trans[state->first][it->first].end())
          {
            trans[state->first][it->first].push_back(*ch);
            tv_temp2->push_back(*ch);
          }
        }
        sort(tv_temp2->begin(), tv_temp2->end());
        tv_temp->push_back(make_pair(it->first, *tv_temp2));
      }
      sort(tv_temp->begin(), tv_temp->end(), transSort1);
      trans_vec.push_back(make_pair(state->first, *tv_temp));
    }
    sort(trans_vec.begin(), trans_vec.end(), transSort2);
    for(map<int, int>::iterator it = rl.begin(), limit = rl.end(); it != limit; it++)
    {
      final_vec.push_back(it->first);
    }
    sort(final_vec.begin(), final_vec.end());
  }
  vector<pair<int, int>> step(const vector<pair<int, int>>& start, bool incr, int sym, int alt = 0)
  {
    vector<pair<int, int>> ret;
    vector<pair<int, int>> temp1;
    vector<int> temp2;
    //vector<int> temp3;
    int pl = incr ? 1 : 0;
    //for(unsigned int i = 0; i < start.size(); i++)
    for(unsigned int i = 0, limit = start.size(); i < limit; i++)
    {
      if(i > 0)
      {
        temp2.clear();
      }
      stepSingle(start[i].first, sym, temp2);
      if(alt != 0)
      {
        stepSingle(start[i].first, alt, temp2);
      }
      temp1.resize(temp2.size());
      //for(unsigned int j = 0; j < temp2.size(); j++)
      for(unsigned int j = 0, l2 = temp2.size(); j < l2; j++)
      {
        temp1[j] = make_pair(temp2[j], start[i].second+pl);
      }
      appendUnique(ret, temp1);
    }
    return ret;
  }
  bool anyFinal(const vector<pair<int, int>>& states)
  {
    for(unsigned int i = 0; i < states.size(); i++)
    {
      //if(rules.find(states[i].first) != rules.end())
      if(isFinal(states[i].first))
      {
        return true;
      }
    }
    return false;
  }
  int getRule(const vector<pair<int, int>>& states, const set<int>& skip)
  {
    int minPath = 0;
    int rule = -1;
    double weight;
    for(unsigned int i = 0; i < states.size(); i++)
    {
      if(states[i].second < minPath)
      {
        break;
      }
      //if(rules.find(states[i].first) != rules.end())
      if(isFinal(states[i].first))
      {
        int r = rules[states[i].first];
        minPath = states[i].second;
        if(rule == -1 || ruleWeights[r] < weight ||
            (ruleWeights[r] == weight && r < rule))
        {
          rule = r;
          weight = ruleWeights[r];
        }
      }
    }
    return rule;
  }
  int getRule(const vector<pair<int, int>>& states)
  {
    set<int> empty;
    return getRule(states, empty);
  }
  vector<pair<int, int>> match(const vector<pair<int, int>>& states, const wstring& form)
  {
    vector<pair<int, int>> ret = step(states, false, L'^');
    for(unsigned int i = 0; i < form.size(); i++)
    {
      if(form[i] == L'\\')
      {
        ret = step(ret, false, towlower(form[++i]), any_char);
      }
      else if(form[i] == L'<')
      {
        for(unsigned int j = i+1; j < form.size(); j++)
        {
          if(form[j] == L'\\')
          {
            j++;
          }
          else if(form[j] == L'>')
          {
            int tag = (*a)(form.substr(i, j-i+1));
            if(tag)
            {
              ret = step(ret, false, tag, any_tag);
            }
            else
            {
              ret = step(ret, false, any_tag);
            }
            i = j;
            break;
          }
        }
      }
      else
      {
        ret = step(ret, false, towlower(form[i]), any_char);
      }
    }
    ret = step(ret, true, L'$');
    return ret;
  }
  vector<pair<int, int>> matchBlank(const vector<pair<int, int>>& states)
  {
    return step(states, true, L' ');
  }
  vector<pair<int, int>> matchChunk(const vector<pair<int, int>>& states, const wstring& chunk)
  {
    vector<pair<int, int>> next = match(states, chunk);
    int lim = 0;
    for(unsigned int i = 0; i < next.size(); i++)
    {
      if(next[i].second < lim)
      {
        next.resize(i);
        break;
      }
      //if(rules.find(next[i].first) != rules.end())
      if(isFinal(next[i].first))
      {
        lim = next[i].second;
      }
    }
    return next;
  }
  bool shouldKeepShifting(vector<pair<int, int>> states, wstring chunk)
  {
    vector<pair<int, int>> s = step(s, false, checkSymbol);
    if(s.size() == 0)
    {
      return false;
    }
    else
    {
      return match(s, chunk).size() != 0;
    }
  }
};

#endif
