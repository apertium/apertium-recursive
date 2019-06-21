#ifndef __RTXPARSETABLE__
#define __RTXPARSETABLE__

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>
#include <vector>
#include <set>
#include <map>

class ParseTable
{
private:
  int checkSymbol;
  int any_char;
  int any_tag;
  Transducer* t;
  Alphabet* a;
  map<int, map<int, set<int>>> trans;
  // final states -> rules
  map<int, int> rules;
  set<int> stepSingle(int state, int sym)
  {
    /*set<int> ret;
    pair<multimap<int, pair<int, double>>::iterator, multimap<int, pair<int, double>>::iterator> iter;
    iter = t->getTransitions()[state].equal_range(sym);
    for(multimap<int, pair<int, double>>::iterator it = iter.first; it != iter.second; it++)
    {
      set<int> temp = t->closure(it->second.first);
      ret.insert(temp.begin(), temp.end());
    }
    return ret;*/
    return trans[state][sym];
  }
public:
  ParseTable(Transducer* tr, Alphabet* al, map<int, int> rl) :
      t(tr), a(al), rules(rl)
  {
    any_char = (*a)(L"<ANY_CHAR>");
    any_tag = (*a)(L"<ANY_TAG>");
    checkSymbol = (*a)(L"<LOOK:AHEAD>");
    set<int> temp;
    map<int, multimap<int, pair<int, double>>> ts = tr->getTransitions();
    for(map<int, multimap<int, pair<int, double>>>::iterator state = ts.begin(), limit1 = ts.end();
            state != limit1; state++)
    {
      for(multimap<int, pair<int, double>>::iterator it = state->second.begin(), limit2 = state->second.end();
            it != limit2; it++)
      {
        temp = tr->closure(it->second.first);
        trans[state->first][it->first].insert(temp.begin(), temp.end());
      }
    }
  }
  set<int> step(set<int> const & start, int sym, int alt = 0)
  {
    set<int> ret;
    //set<int> temp;
    for(set<int>::iterator it = start.begin(), limit = start.end();
            it != limit; it++)
    {
      /*temp = stepSingle(*it, sym);
      ret.insert(temp.begin(), temp.end());
      if(alt != 0)
      {
        temp = stepSingle(*it, alt);
        ret.insert(temp.begin(), temp.end());
      }*/
      ret.insert(trans[*it][sym].begin(), trans[*it][sym].end());
      if(alt != 0)
      {
        ret.insert(trans[*it][alt].begin(), trans[*it][alt].end());
      }
    }
    return ret;
  }
  bool anyFinal(const set<int>& states)
  {
    for(set<int>::iterator it = states.begin(), limit = states.end();
            it != limit; it++)
    {
      if(t->isFinal(*it))
      {
        return true;
      }
    }
    return false;
  }
  set<int> getFinals(const set<int>& states)
  {
    set<int> ret;
    for(set<int>::iterator it = states.begin(); it != states.end(); it++)
    {
      if(t->isFinal(*it))
      {
        ret.insert(*it);
      }
    }
    return ret;
  }
  int getRule(set<int> states, const set<int>& skip)
  {
    map<int, double> tempfin = t->getFinals();
    double weight;
    int rule = -1;
    int r;
    for(set<int>::iterator it = states.begin(), limit = states.end();
            it != limit; it++)
    {
      if(tempfin.find(*it) != tempfin.end())
      {
        r = rules[*it];
        if(skip.find(r) != skip.end())
        {
          continue;
        }
        if(rule == -1 || tempfin[*it] > weight)
        {
          rule = r;
          weight = tempfin[*it];
        }
      }
    }
    return rule;
  }
  int getRule(set<int> states)
  {
    set<int> empty;
    return getRule(states, empty);
  }
  int getRule(vector<pair<int, int>> states)
  {
    set<int> s;
    for(unsigned int i = 0; i < states.size(); i++)
    {
      s.insert(states[i].first);
    }
    return getRule(s);
  }
  set<int> match(set<int> states, const wstring& form)
  {
    set<int> ret = states;
    for(unsigned int i = 0; i < form.size(); i++)
    {
      if(form[i] == L'^' || form[i] == L'$' || form[i] == L' ')
      {
        ret = step(ret, form[i]);
      }
      else if(form[i] == L'\\')
      {
        ret = step(ret, towlower(form[++i]), any_char);
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
              ret = step(ret, tag, any_tag);
            }
            else
            {
              ret = step(ret, any_tag);
            }
            i = j;
          }
        }
      }
      else
      {
        ret = step(ret, towlower(form[i]), any_char);
      }
    }
    return ret;
  }
  vector<pair<int, int>> matchChunk(vector<pair<int, int>> states, wstring chunk)
  {
    map<int, set<int>> state_map;
    int max = 0;
    for(unsigned int i = 0; i < states.size(); i++)
    {
      state_map[states[i].second].insert(states[i].first);
      max = states[i].second > max ? states[i].second : max;
    }
    vector<pair<int, int>> ret;
    for(int i = max; i >= 0; i--)
    {
      if(state_map.find(i) != state_map.end())
      {
        set<int> dest = match(state_map[i], chunk);
        for(set<int>::iterator it = dest.begin(); it != dest.end(); it++)
        {
          ret.push_back(make_pair(*it, i+1));
        }
        if(anyFinal(dest))
        {
          break;
        }
      }
    }
    return ret;
  }
  bool shouldKeepShifting(vector<pair<int, int>> states, wstring chunk)
  {
    set<int> s;
    for(unsigned int i = 0; i < states.size(); i++)
    {
      s.insert(states[i].first);
    }
    s = step(s, checkSymbol);
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
