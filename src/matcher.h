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
  }
  void popStack(int const n)
  {
    stackIdx -= n;
  }
  int stackSize()
  {
    return stackIdx + 1;
  }
  int stateSize()
  {
    int f = first[stackIdx];
    int l = last[stackIdx];
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
  int pushStack(int sym)
  {
    first[stackIdx+1] = 0;
    last[stackIdx+1] = 0;
    if(stackIdx >= 0)
    {
      step(stackIdx, stackIdx+1, sym);
    }
    stackIdx++;
    applySymbol(initial, sym, stackIdx);
  }
  void matchBlank()
  {
    pushStack(L' ');
  }
  void matchChunk(wstring ch)
  {
    pushStack(L'^');
    for(unsigned int i = 0; i < ch.size(); i++)
    {
      switch(ch[i])
      {
        case L'\\':
          step(stackIdx, stackIdx, towlower(ch[++i]), any_char);
          break;
        case L'<':
          for(unsigned int j = i+1; j < ch.size(); j++)
          {
            if(ch[j] == L'>')
            {
              int symbol = (*alpha)(ch.substr(i, j-i+1));
              if(symbol)
              {
                step(stackIdx, stackIdx, symbol, any_tag);
              }
              else
              {
                step(stackIdx, stackIdx, any_tag);
              }
              i = j;
              break;
            }
          }
          break;
        default:
          step(stackIdx, stackIdx, towlower(ch[i]), any_char);
          break;
      }
    }
    step(stackIdx, stackIdx, L'$');
  }
  bool shouldShift(wstring chunk)
  {
    pushStack(check_sym);
    matchChunk(chunk);
    bool ret = (first[stackIdx] != last[stackIdx]);
    popStack(2);
    return ret;
  }
  int getRule(set<int> skip)
  {
    int rule = -1;
    double weight;
    for(int i = first[stackIdx]; i != last[stackIdx]; i = (i+1)%RTXStateSize)
    {
      int node = stack[stackIdx][i];
      if(nodes[node].rule != -1)
      {
        if(rule == -1 || nodes[node].weight > weight ||
            (nodes[node].weight == weight && nodes[node].rule < rule))
        {
          rule = nodes[node].rule;
          weight = nodes[node].weight;
        }
      }
    }
    return rule;
  }
  int getRule()
  {
    set<int> empty;
    return getRule(empty);
  }
};

#endif
