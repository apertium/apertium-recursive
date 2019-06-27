#ifndef __RTXMATCHER__
#define __RTXMATCHER__

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>
#include <chunk.h>

using namespace std;

#define RTXStateSize 128
//#define RTXStackSize 4096
#define RTXStackSize 10000000

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
  int length;
  MatchNode2(int const sz)
  : size(sz), rule(-1)
  {
    trans = new Transition[sz];
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
  int available[RTXStackSize];
  int availableIdx;
  int resetFrom;
  int initial;
  int rejected[RTXStackSize];
  int rejectedCount;

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
    int ret = available[availableIdx];
    availableIdx--;
    if(availableIdx < resetFrom) resetFrom--;
    first[ret] = 0;
    last[ret] = 0;
    return ret;
  }
  int pushStack(int src, int sym)
  {
    int state = newState();
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
    if(src != -1)
    {
      step(src, state, sym);
    }
    return state;
  }

public:
  MatchExe2(Transducer& t, Alphabet* a, map<int, int> const& rules, vector<int> pattern_size)
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
      nodes[it->first].rule = it->second;
      nodes[it->first].weight = t.getFinals().at(it->first);
      nodes[it->first].length = pattern_size[it->second-1];
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

    resetFrom = 0;
    returnAllStates();
  }
  void returnAllStates()
  {
    for(int i = resetFrom; i < RTXStackSize; i++)
    {
      available[i] = RTXStackSize - i - 1;
    }
    availableIdx = RTXStackSize - 1;
    resetFrom = RTXStackSize - 1;
  }
  void returnState(int n)
  {
    availableIdx++;
    if(availableIdx == RTXStackSize) availableIdx--;
    available[availableIdx] = n;
  }
  int stateSize(int state)
  {
    int f = first[state];
    int l = last[state];
    return (l >= f) ? (l - f) : (l + RTXStateSize - f);
  }
  int matchBlank(int src)
  {
    return pushStack(src, L' ');
  }
  int matchChunk(int src, wstring ch)
  {
    int state = pushStack(src, L'^');
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
  bool shouldShift(int src)
  {
    int s = pushStackNoInit(src, L' ');
    bool ret = (first[s] != last[s]);
    returnState(s);
    return ret;
  }
  int getRule(int state)
  {
    int rule = -1;
    double weight;
    int len = 0;
    for(int i = first[state], end = last[state]; i != end; i = (i+1)%RTXStateSize)
    {
      int n = stack[state][i];
      MatchNode2& node = nodes[n];
      if(node.rule != -1)
      {
        bool rej = false;
        for(int rj = 0; rj < rejectedCount; rj++)
        {
          if(rejected[rj] == node.rule)
          {
            rej = true;
            break;
          }
        }
        if(rej) continue;
        if(node.length > len)
        {
          rule = node.rule;
          weight = node.weight;
          len = node.length;
          continue;
        }
        if(rule != -1 && node.length < len) continue;
        if(rule != -1 && node.weight < weight) continue;
        if(rule != -1 && node.rule > rule) continue;

        rule = node.rule;
        weight = node.weight;
      }
    }
    return rule;
  }
  void resetRejected()
  {
    rejectedCount = 0;
  }
  void rejectRule(int rule)
  {
    rejected[rejectedCount++] = rule;
  }
};

class ParseNode
{
public:
  int state;
  Chunk* chunk;
  int length;
  ParseNode* prev;
  int refcount;
  MatchExe2* mx;
  double weight;
  ParseNode(MatchExe2* m, Chunk* ch, double w = 0.0)
  : chunk(ch), length(1), prev(NULL), refcount(0), mx(m), weight(w)
  {
    if(chunk->isBlank)
    {
      state = mx->matchBlank(-1);
    }
    else
    {
      state = mx->matchChunk(-1, chunk->matchSurface());
    }
  }
  ParseNode(ParseNode* last, Chunk* next, double w = 0.0)
  {
    mx = last->mx;
    prev = last;
    prev->refcount++;
    length = prev->length+1;
    refcount = 0;
    chunk = next;
    weight = (w == 0) ? prev->weight : w;
    if(next->isBlank)
    {
      state = mx->matchBlank(prev->state);
    }
    else
    {
      state = mx->matchChunk(prev->state, chunk->matchSurface());
    }
  }
  ParseNode(ParseNode* other)
  {
    state = other->state;
    chunk = other->chunk;
    length = other->length;
    prev = other->prev;
    weight = other->weight;
    if(prev != NULL)
    {
      prev->refcount++;
    }
    refcount = other->refcount;
    mx = other->mx;
  }
  ~ParseNode()
  {
    mx->returnState(state);
    if(prev != NULL)
    {
      prev->refcount--;
      if(prev->refcount == 0)
      {
        delete prev;
      }
    }
  }
  void getChunks(vector<Chunk*>& chls, int count)
  {
    if(count < 0) return;
    chls[count] = chunk;
    prev->getChunks(chls, count-1);
  }
  ParseNode* popNodes(int n)
  {
    if(n == 1 && prev == NULL) return NULL;
    if(n == 0) return this;
    return prev->popNodes(n-1);
  }
};

#endif
