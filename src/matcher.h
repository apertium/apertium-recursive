#ifndef __RTXMATCHER__
#define __RTXMATCHER__

#include <rtx_config.h>
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
  int rule_begin; // region of array in MatchExe2 which
  int rule_end;   // corresponds to this node
  MatchNode2()
  : size(0), rule_begin(-1), rule_end(-1)
  {}
  void setSize(int const sz)
  {
    size = sz;
    trans = new Transition[sz];
  }
  ~MatchNode2()
  {
    delete[] trans;
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
  MatchNode2* nodes;
  int any_char;
  int any_tag;
  int lookahead;
  Alphabet* alpha;
  int initial;
  int rejected[RTXStackSize];
  int rejectedCount;
  int prematch[RTXStackSize];
  int prematchAlt[RTXStackSize];
  int prematchIdx;

  int rule_count;
  int* rule_states;
  int* rule_numbers;
  double* rule_weights;
  int* rule_lengths;

  void applySymbol(int const srcNode, int const symbol, int* state, int& last)
  {
    int res = nodes[srcNode].search(symbol);
    if(res != -1)
    {
      state[last] = res;
      last = (last+1) % RTXStateSize;
    }
  }

public:
  MatchExe2(Transducer& t, Alphabet* a, multimap<int, pair<int, double>> const& rules, vector<int> pattern_size)
  : alpha(a)
  {
    map<int, multimap<int, pair<int, double> > >& trns = t.getTransitions();
    nodes = new MatchNode2[trns.size()];
    for(map<int, multimap<int, pair<int, double> > >::const_iterator it = trns.begin(),
          limit = trns.end(); it != limit; it++)
    {
      nodes[it->first].setSize(it->second.size());
      int i = 0;
      for(multimap<int, pair<int, double> >::const_iterator it2 = it->second.begin(),
            limit2 = it->second.end(); it2 != limit2; it2++)
      {
        nodes[it->first].addTransition(it2->first, it2->second.first, i++);
      }
    }

    rule_count = rules.size();
    rule_states = new int[rule_count];
    rule_numbers = new int[rule_count];
    rule_weights = new double[rule_count];
    rule_lengths = new int[rule_count];
    int i = 0;
    for(auto it : rules)
    {
      if(nodes[it.first].rule_begin == -1) nodes[it.first].rule_begin = i;
      nodes[it.first].rule_end = i;
      rule_states[i] = it.first;
      rule_numbers[i] = it.second.first;
      rule_weights[i] = it.second.second;
      rule_lengths[i] = pattern_size[it.second.first-1];
      i++;
    }

    initial = t.getInitial();

    any_char = (*a)(L"<ANY_CHAR>");
    any_tag = (*a)(L"<ANY_TAG>");
    lookahead = (*a)(L"<LOOK:AHEAD>");

    prematchIdx = 0;
  }
  ~MatchExe2()
  {
    delete[] nodes;
    delete[] rule_states;
    delete[] rule_numbers;
    delete[] rule_weights;
    delete[] rule_lengths;
  }
  int getInitial()
  {
    return initial;
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
  void matchBlank(int* state, int& first, int& last)
  {
    step(state, first, last, L' ');
  }
  void matchChunk(int* state, int& first, int& last, const wstring& ch, bool addInit = true)
  {
    step(state, first, last, L'^');
    if(addInit)
    {
      applySymbol(initial, L'^', state, last);
    }
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
  void prepareChunk(const wstring& chunk)
  {
    prematchIdx = 0;
    for(unsigned int i = 0, limit = chunk.size(); i < limit; i++)
    {
      switch(chunk[i])
      {
        case L'\\':
          prematchAlt[prematchIdx] = any_char;
          prematch[prematchIdx++] = towlower(chunk[++i]);
          break;
        case L'<':
          for(unsigned int j = i+1; j < chunk.size(); j++)
          {
            if(chunk[j] == L'>')
            {
              int symbol = (*alpha)(chunk.substr(i, j-i+1));
              prematchAlt[prematchIdx] = any_tag;
              prematch[prematchIdx++] = (symbol ? symbol : any_tag);
              i = j;
              break;
            }
          }
          break;
        default:
          prematchAlt[prematchIdx] = any_char;
          prematch[prematchIdx++] = towlower(chunk[i]);
          break;
      }
    }
  }
  void matchPreparedChunk(int* state, int& first, int& last)
  {
    step(state, first, last, L'^');
    applySymbol(initial, L'^', state, last);
    for(int i = 0; i < prematchIdx; i++)
    {
      if(prematch[i] == any_tag)
      {
        step(state, first, last, any_tag);
      }
      else
      {
        step(state, first, last, prematch[i], prematchAlt[i]);
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
  bool shouldShift(int* state, int first, int last, const wstring& chunk)
  {
    int local_state[RTXStateSize];
    memcpy(local_state, state, RTXStateSize*sizeof(int));
    int local_first = first;
    int local_last = last;
    step(local_state, local_first, local_last, lookahead);
    matchChunk(local_state, local_first, local_last, chunk, false);
    return local_first != local_last;
  }
  pair<int, double> getRule(int* state, int first, int last)
  {
    int rule = -1;
    double weight = 0.0;
    int len = 0;
    for(int i = first; i != last; i = (i+1)%RTXStateSize)
    {
      MatchNode2& node = nodes[state[i]];
      if(node.rule_begin == -1) continue;
      for(int rl = node.rule_begin; rl <= node.rule_end; rl++)
      {
        bool rej = false;
        for(int rj = 0; rj < rejectedCount; rj++)
        {
          if(rejected[rj] == rule_numbers[rl])
          {
            rej = true;
            break;
          }
        }
        if(rej) continue;
        if(rule_lengths[rl] > len)
        {
          rule = rule_numbers[rl];
          weight = rule_weights[rl];
          len = rule_lengths[rl];
          continue;
        }
        if(rule != -1 && rule_lengths[rl] < len) continue;
        if(rule != -1 && rule_weights[rl] < weight) continue;
        if(rule != -1 && rule_numbers[rl] > rule) continue;

        rule = rule_numbers[rl];
        weight = rule_weights[rl];
      }
    }
    return make_pair(rule, weight);
  }
  int getRuleUnweighted(int* state, int first, int last)
  {
    for(int i = first; i != last; i = (i+1)%RTXStateSize)
    {
      if(nodes[state[i]].rule_begin != -1)
      {
        return rule_numbers[nodes[state[i]].rule_begin];
      }
    }
    return -1;
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
  MatchExe2* mx;
  double weight;
  int firstWord;
  int lastWord;
  int id;
  map<wstring, wstring, Ltstr> stringVars;
  vector<Chunk*> chunkVars;
  ParseNode()
  : first(0), last(0), firstWord(0), lastWord(0), id(-1)
  {}
  void init(MatchExe2* m, Chunk* ch, double w = 0.0)
  {
    firstWord = 0;
    lastWord = 0;
    chunk = ch;
    length = 1;
    prev = NULL;
    mx = m;
    weight = w;
    if(chunk->isBlank)
    {
      mx->matchBlank(state, first, last);
    }
    else
    {
      mx->matchChunk(state, first, last, chunk->matchSurface());
    }
  }
  void init(ParseNode* prevNode, Chunk* next, double w = 0.0)
  {
    chunk = next;
    prev = prevNode;
    firstWord = prev->lastWord + 1;
    lastWord = firstWord;
    for(int i = prevNode->first; i != prevNode->last; i = (i+1)%RTXStateSize)
    {
      state[last++] = prevNode->state[i];
    }
    mx = prevNode->mx;
    length = prev->length+1;
    stringVars = prev->stringVars;
    chunkVars = prev->chunkVars;
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
  void init(ParseNode* prevNode, Chunk* next, bool prepared)
  {
    chunk = next;
    prev = prevNode;
    for(int i = prevNode->first; i != prevNode->last; i = (i+1)%RTXStateSize)
    {
      state[last++] = prevNode->state[i];
    }
    mx = prevNode->mx;
    length = prev->length+1;
    weight = prev->weight;
    firstWord = prev->lastWord+1;
    lastWord = firstWord;
    stringVars = prev->stringVars;
    chunkVars = prev->chunkVars;
    if(next->isBlank)
    {
      mx->matchBlank(state, first, last);
    }
    else if(prepared)
    {
      mx->matchPreparedChunk(state, first, last);
    }
    else
    {
      mx->matchChunk(state, first, last, chunk->matchSurface());
    }
  }
  void init(ParseNode* other)
  {
    for(int i = other->first; i != other->last; i = (i+1)%RTXStateSize)
    {
      state[last++] = other->state[i];
    }
    chunk = other->chunk->copy();
    length = other->length;
    prev = other->prev;
    weight = other->weight;
    mx = other->mx;
    firstWord = other->firstWord;
    lastWord = other->lastWord;
    stringVars = other->stringVars;
    chunkVars = other->chunkVars;
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
  pair<int, double> getRule()
  {
    return mx->getRule(state, first, last);
  }
  bool shouldShift()
  {
    return mx->shouldShift(state, first, last);
  }
  bool shouldShift(Chunk* next)
  {
    return mx->shouldShift(state, first, last, next->matchSurface());
  }
  bool isDone()
  {
    return (first == last);
  }
};

#endif
