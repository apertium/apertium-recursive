#ifndef __RTXMATCHER__
#define __RTXMATCHER__

#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>
#include <chunk.h>
#include <list>

using namespace std;

#define RTXStateSize 128
#define RTXStackSize 4096

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
  Alphabet* alpha;
  int initial;
  int rejected[RTXStackSize];
  int rejectedCount;

  void applySymbol(int const srcNode, int const symbol, int* state, int& last)
  {
    int res = nodes[srcNode].search(symbol);
    if(res != -1)
    {
      state[last] = res;
      last = (last+1) % RTXStateSize;
    }
  }
  void step(int* state, int& first, int& last, int const symbol)
  {
    int loclast = last;
    for(int i = first; i != loclast; i = (i+1)%RTXStateSize)
    {
      applySymbol(state[i], symbol, state, last);
    }
    first = loclast;
  }
  void step(int* state, int& first, int& last, int const symbol, int const alt)
  {
    int loclast = last;
    for(int i = first; i != loclast; i = (i+1)%RTXStateSize)
    {
      applySymbol(state[i], symbol, state, last);
      applySymbol(state[i], alt, state, last);
    }
    first = loclast;
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
  }
  void matchBlank(int* state, int& first, int& last)
  {
    step(state, first, last, L' ');
  }
  void matchChunk(int* state, int& first, int& last, wstring ch)
  {
    step(state, first, last, L'^');
    applySymbol(initial, L'^', state, last);
    for(unsigned int i = 0, limit = ch.size(); i < limit; i++)
    {
      switch(ch[i])
      {
        case L'\\':
          step(state, first, last, towlower(ch[++i]), any_char);
          break;
        case L'<':
          for(unsigned int j = i+1; j < ch.size(); j++)
          {
            if(ch[j] == L'>')
            {
              int symbol = (*alpha)(ch.substr(i, j-i+1));
              if(symbol)
              {
                step(state, first, last, symbol, any_tag);
              }
              else
              {
                step(state, first, last, any_tag);
              }
              i = j;
              break;
            }
          }
          break;
        default:
          step(state, first, last, towlower(ch[i]), any_char);
          break;
      }
    }
    step(state, first, last, L'$');
  }
  bool shouldShift(int* state, int first, int last)
  {
    for(int i = first; i != last; i = (i+1)%RTXStateSize)
    {
      if(nodes[state[i]].search(L' ') != -1)
      {
        return true;
      }
    }
    return false;
  }
  int getRule(int* state, int first, int last)
  {
    int rule = -1;
    double weight;
    int len = 0;
    for(int i = first; i != last; i = (i+1)%RTXStateSize)
    {
      MatchNode2& node = nodes[state[i]];
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
  int state[RTXStateSize];
  int first;
  int last;
  Chunk* chunk;
  int length;
  ParseNode* prev;
  int refcount;
  MatchExe2* mx;
  double weight;
  ParseNode(MatchExe2* m, Chunk* ch, double w = 0.0)
  : first(0), last(0), chunk(ch), length(1), prev(NULL), refcount(1), mx(m), weight(w)
  {
    ch->refcount++;
    if(chunk->isBlank)
    {
      mx->matchBlank(state, first, last);
    }
    else
    {
      mx->matchChunk(state, first, last, chunk->matchSurface());
    }
  }
  ParseNode(ParseNode* prevNode, Chunk* next, double w = 0.0)
  : first(0), last(0), chunk(next), prev(prevNode), refcount(1)
  {
    for(int i = prevNode->first; i != prevNode->last; i = (i+1)%RTXStateSize)
    {
      state[last++] = prevNode->state[i];
    }
    mx = prevNode->mx;
    prev->refcount++;
    length = prev->length+1;
    chunk->refcount++;
    weight = (w == 0) ? prev->weight : w;
    if(next->isBlank)
    {
      mx->matchBlank(state, first, this->last);
    }
    else
    {
      mx->matchChunk(state, first, this->last, chunk->matchSurface());
    }
  }
  ParseNode(ParseNode* other)
  {
    first = 0;
    last = 0;
    for(int i = other->first; i != other->last; i = (i+1)%RTXStateSize)
    {
      state[last++] = other->state[i];
    }
    chunk = other->chunk->copy();
    chunk->refcount++;
    length = other->length;
    prev = other->prev;
    weight = other->weight;
    if(prev != NULL)
    {
      prev->refcount++;
    }
    refcount = 1;
    mx = other->mx;
  }
  ~ParseNode()
  {
    if(prev != NULL)
    {
      prev->release();
    }
    chunk->release();
  }
  void release()
  {
    refcount--;
    if(refcount == 0)
    {
      delete this;
    }
  }
  void getChunks(list<Chunk*>& chls, int count)
  {
    chls.push_front(chunk);
    if(count == 0) return;
    prev->getChunks(chls, count-1);
  }
  void getChunks(vector<Chunk*>& chls, int count)
  {
    chls[count] = chunk;
    if(count == 0) return;
    prev->getChunks(chls, count-1);
  }
  ParseNode* popNodes(int n)
  {
    if(n == 1 && prev == NULL) return NULL;
    if(n == 0) return this;
    return prev->popNodes(n-1);
  }
  int getRule()
  {
    return mx->getRule(state, first, last);
  }
  bool shouldShift()
  {
    return mx->shouldShift(state, first, last);
  }
  bool isDone()
  {
    return (first == last);
  }
};

#endif
