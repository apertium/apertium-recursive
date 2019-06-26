#ifndef __RTXMATCHER__
#define __RTXMATCHER__

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>

using namespace std;

#define RTXStateSize 128
#define RTXStackSize 1024

class MatchNode2
{
private:
  struct Transition
  {
    int tag;
    int dest;
  };
  Transition* trans;
  int size;
public:
  int rule;
  double weight;
  MatchNode2(int const sz)
  : size(sz), rule(-1)
  {
    trans = new Transition[sz];
  }
  void setFinal(int const rl, double const wgt)
  {
    rule = rule;
    weight = wgt;
  }
  void addTransition(int const tag, int const dest, int const pos)
  {
    trans[pos].tag = tag;
    trans[pos].dest = dest;
  }
  int search(int tag)
  {
    int left = 0, right = size-1;
    while(left <= right)
    {
      int mid = (left+right)/2;
      if(trans[mid].tag == tag)
      {
        return trans[mid].dest;
      }
      if(trans[mid].tag > tag)
      {
        right = mid - 1;
      }
      else
      {
        left = mid + 1;
      }
    }

    return -1;
  }
};

class MatchExe2
{
private:
  vector<MatchNode2> nodes;
  int any_char;
  int any_tag;
  int check_sym;
  Alphabet* alpha;
  int stack[RTXStackSize][RTXStateSize];
  int first[RTXStackSize];
  int last[RTXStackSize];
  int stackIdx;
  int available[RTXStackSize];
  int availableIdx;
  int initial;
  
public:
  MatchExe2(Transducer& t, Alphabet* a, map<int, int> const& rules)
  : alpha(a)
  {
    map<int, multimap<int, pair<int, double> > >& trns = t.getTransitions();
    nodes.reserve(trns.size());
    for(map<int, multimap<int, pair<int, double> > >::const_iterator it = trns.begin(),
          limit = trns.end(); it != limit; it++)
    {
      MatchNode2 mynode(it->second.size());
      nodes.push_back(mynode);
    }

    for(map<int, int>::const_iterator it = rules.begin(), limit = rules.end();
          it != limit; it++)
    {
      //nodes[it->first].setFinal(it->second, t.getFinals().at(it->first));
      nodes[it->first].rule = it->second;
      nodes[it->first].weight = t.getFinals().at(it->first);
    }

    initial = t.getInitial();

    for(map<int, multimap<int, pair<int, double> > >::const_iterator it = trns.begin(),
          limit = trns.end(); it != limit; it++)
    {
      MatchNode2 &mynode = nodes[it->first];
      int i = 0;
      for(multimap<int, pair<int, double> >::const_iterator it2 = it->second.begin(),
            limit2 = it->second.end(); it2 != limit2; it2++)
      {
        mynode.addTransition(it2->first, it2->second.first, i++);
      }
    }

    any_char = (*a)(L"<ANY_CHAR>");
    any_tag = (*a)(L"<ANY_TAG>");
    check_sym = (*a)(L"<LOOK:AHEAD>");

    availableIdx = -1;
    for(int i = 0; i < RTXStackSize; i++)
    {
      available[++availableIdx] = RTXStackSize - i - 1;
    }
  }
  void popStack(int const n)
  {
    stackIdx -= n;
  }
  int stackSize()
  {
    return stackIdx + 1;
  }
  void returnState(int n)
  {
    available[++availableIdx] = n;
  }
  int stateSize()
  {
    int f = first[stackIdx];
    int l = last[stackIdx];
    return (l >= f) ? (l - f) : (l + RTXStateSize - f);
  }
  int stateSize(int state)
  {
    int f = first[state];
    int l = last[state];
    return (l >= f) ? (l - f) : (l + RTXStateSize - f);
  }
  // src = source MatchNode2
  // dest = stack location
  void applySymbol(int const src, int const symbol, int const dest)
  {
    int res = nodes[src].search(symbol);
    if(res != -1)
    {
      stack[dest][last[dest]] = res;
      last[dest] = (last[dest] + 1) % RTXStateSize;
    }
  }
  void step(int const src, int const dest, int const symbol)
  {
    int loclast = last[src];
    for(int i = first[src]; i != loclast; i = (i+1)%RTXStateSize)
    {
      applySymbol(stack[src][i], symbol, dest);
    }
    if(src == dest)
    {
      first[src] = loclast;
    }
  }
  void step(int const src, int const dest, int const symbol, int const alt)
  {
    int loclast = last[src];
    for(int i = first[src]; i != loclast; i = (i+1)%RTXStateSize)
    {
      applySymbol(stack[src][i], symbol, dest);
      applySymbol(stack[src][i], alt, dest);
    }
    if(src == dest)
    {
      first[src] = loclast;
    }
  }
  int newState()
  {
    int ret = available[availableIdx--];
    first[ret] = 0;
    last[ret] = 0;
    return ret;
  }
  int pushStack(int src, int sym)
  {
    int state = newState();
    first[state] = 0;
    last[state] = 0;
    if(src != -1)
    {
      step(src, state, sym);
    }
    applySymbol(initial, sym, state);
    return state;
  }
  int pushStackNoInit(int src, int sym)
  {
    int state = newState();
    first[state] = 0;
    last[state] = 0;
    if(src != -1)
    {
      step(src, state, sym);
    }
    return state;
  }
  int matchBlank(int src)
  {
    return pushStack(src, L' ');
  }
  int matchChunk(int src, wstring ch, bool isShiftCheck = false)
  {
    int state;
    if(isShiftCheck)
    {
      state = pushStackNoInit(src, check_sym);
      step(state, state, L'^');
    }
    else
    {
      state = pushStack(src, L'^');
    }
    for(unsigned int i = 0; i < ch.size(); i++)
    {
      switch(ch[i])
      {
        case L'\\':
          step(state, state, towlower(ch[++i]), any_char);
          break;
        case L'<':
          for(unsigned int j = i+1; j < ch.size(); j++)
          {
            if(ch[j] == L'>')
            {
              int symbol = (*alpha)(ch.substr(i, j-i+1));
              if(symbol)
              {
                step(state, state, symbol, any_tag);
              }
              else
              {
                step(state, state, any_tag);
              }
              i = j;
              break;
            }
          }
          break;
        default:
          step(state, state, towlower(ch[i]), any_char);
          break;
      }
    }
    step(state, state, L'$');
    return state;
  }
  bool shouldShift(int src, wstring chunk)
  {
    int s = matchChunk(src, chunk, true);
    bool ret = (first[s] != last[s]);
    returnState(s);
    return ret;
  }
  int getRule(int state, set<int> skip)
  {
    int rule = -1;
    double weight;
    for(int i = first[state], end = last[state]; i != end; i = (i+1)%RTXStateSize)
    {
      int node = stack[state][i];
      if(nodes[node].rule != -1)
      {
        if(skip.find(nodes[node].rule) != skip.end()) continue;
        if(rule != -1 && nodes[node].weight < weight) continue;
        if(rule != -1 && nodes[node].rule > rule) continue;

        rule = nodes[node].rule;
        weight = nodes[node].weight;
      }
    }
    return rule;
  }
  int getRule(int state)
  {
    set<int> empty;
    return getRule(state, empty);
  }
};

#endif
