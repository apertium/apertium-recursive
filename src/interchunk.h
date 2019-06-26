#ifndef __RTXINTERCHUNK__
#define __RTXINTERCHUNK__

#include <apertium/transfer_instr.h>
#include <apertium/transfer_token.h>
#include <apertium/interchunk_word.h>
#include <apertium/apertium_re.h>
#include <apertium/utf_converter.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/buffer.h>
#include <lttoolbox/ltstr.h>
#include <lttoolbox/match_exe.h>
#include <lttoolbox/match_state.h>
#include <parse_table.h>
#include <matcher.h>

#include <cstring>
#include <cstdio>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <set>
#include <vector>
#include <stack>
#include <list>
#include <deque>

using namespace std;

enum ClipType
{
  SourceClip,
  TargetClip,
  ReferenceClip
};

class Chunk
{
public:
  wstring source;
  wstring target;
  wstring coref;
  bool isBlank;
  vector<Chunk*> contents;
  Chunk()
  {
  }
  Chunk(wstring blankContent)
  {
    target = blankContent;
    isBlank = true;
  }
  Chunk(wstring src, wstring dest, wstring cor)
  {
    source = src;
    target = dest;
    coref = cor;
    isBlank = false;
  }
  Chunk(wstring dest, vector<Chunk*> children)
  {
    target = dest;
    contents = children;
    isBlank = false;
  }
  ~Chunk()
  {
    for(int i = 0; i < contents.size(); i++)
    {
      delete contents[i];
    }
  }
  wstring chunkPart(ApertiumRE const &part, const ClipType side)
  {
    string chunk;
    switch(side)
    {
      case SourceClip:
        chunk = UtfConverter::toUtf8(source);
        break;
      case TargetClip:
        chunk = UtfConverter::toUtf8(target);
        break;
      case ReferenceClip:
        chunk = UtfConverter::toUtf8(coref);
        break;
    }
    string result = part.match(chunk);
    if(result.size() == 0)
    {
      return wstring(L"");
    }
    else
    {
      return UtfConverter::fromUtf8(result);
    }
  }
  void setChunkPart(ApertiumRE const &part, wstring const &value)
  {
    string surf = UtfConverter::toUtf8(target);
    if(part.match(surf).size() == 0)
    {
      target += value;
    }
    else
    {
      string val = UtfConverter::toUtf8(value);
      part.replace(surf, val);
      target = UtfConverter::fromUtf8(surf);
    }
  }
  vector<wstring> getTags(const vector<wstring>& parentTags)
  {
    int last = 0;
    vector<wstring> ret;
    for(unsigned int i = 0, limit = target.size(); i < limit; i++)
    {
      if(target[i] == L'<')
      {
        last = i;
        bool isNum = true;
        for(unsigned int j = i+1; j < limit; j++)
        {
          if(target[j] == L'>')
          {
            if(isNum)
            {
              int n = stoi(target.substr(last+1, j-last-1))-1;
              if(n >= 0 && n < parentTags.size())
              {
                ret.push_back(parentTags[n]);
                last = j+1;
                break;
              }
            }
            wstring tag = target.substr(last, j-last+1);
            ret.push_back(tag);
            last = j+1;
            break;
          }
          if(!isdigit(target[j]))
          {
            isNum = false;
          }
        }
      }
      else if(target[i] == L'\\')
      {
        i++;
      }
    }
    return ret;
  }
  void updateTags(const vector<wstring>& parentTags)
  {
    int last = 0;
    wstring result;
    result.reserve(target.size() + (2*parentTags.size()));
    // a rough estimate - works if most number tags are 1 digit and most new tags are 3 chars or less
    for(unsigned int i = 0, limit = target.size(); i < limit; i++)
    {
      if(target[i] == L'<')
      {
        result += target.substr(last, i-last);
        last = i;
        bool isNum = true;
        for(unsigned int j = i+1; j < limit; j++)
        {
          if(target[j] == L'>')
          {
            if(isNum)
            {
              int n = stoi(target.substr(last+1, j-last-1))-1;
              if(n >= 0 && n < parentTags.size())
              {
                result += parentTags[n];
              }
            }
            else
            {
              result += target.substr(last, j-last+1);
            }
            last = j+1;
            break;
          }
          if(!isdigit(target[j]))
          {
            isNum = false;
          }
        }
      }
      else if(target[i] == L'\\')
      {
        i++;
      }
    }
    if(last != target.size()-1)
    {
      result += target.substr(last);
    }
    target = result;
  }
  void output(const vector<wstring>& parentTags, FILE* out = NULL)
  {
    if(contents.size() > 0)
    {
      vector<wstring> tags = getTags(parentTags);
      for(int i = 0; i < contents.size(); i++)
      {
        contents[i]->output(tags, out);
      }
    }
    else if(isBlank)
    {
      if(out == NULL)
      {
        cout << UtfConverter::toUtf8(target);
      }
      else
      {
        fputs_unlocked(UtfConverter::toUtf8(target).c_str(), out);
      }
    }
    else
    {
      updateTags(parentTags);
      if(out == NULL)
      {
        cout << "^" << UtfConverter::toUtf8(target) << "$";
      }
      else
      {
        fputc_unlocked('^', out);
        fputs_unlocked(UtfConverter::toUtf8(target).c_str(), out);
        fputc_unlocked('$', out);
      }
    }
  }
  void output(FILE* out)
  {
    vector<wstring> tags;
    output(tags, out);
  }
  wstring matchSurface()
  {
    if(source.size() == 0)
    {
      return target;
    }
    return source;
  }
};

struct StackElement
{
  int mode;
  bool b;
  int i;
  wstring s;
  Chunk* c;
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
    weight = w == 0 ? prev->weight : w;
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

class Interchunk
{
private:

  Alphabet alphabet;
  MatchExe *me;
  MatchState ms;
  MatchExe2 *mx;
  map<wstring, ApertiumRE, Ltstr> attr_items;
  map<wstring, wstring, Ltstr> variables;
  map<wstring, int, Ltstr> macros;
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;
  map<wstring, set<wstring, Ltstr>, Ltstr> listslow;
  vector<wstring> rule_map;
  vector<int> pat_size;
  int longestPattern;
  bool furtherInput;
  bool allDone;
  map<int, double> ruleWeights;
  set<int> rejectedRules;
  StackElement theStack[32];
  int stackIdx;
  vector<Chunk*> currentInput;
  vector<Chunk*> currentOutput;
  stack<Chunk*> parseStack;
  deque<Chunk*> inputBuffer;
  bool outputting;
  vector<ParseNode*> parseGraph;

  FILE *output;
  int any_char;
  int any_tag;

  bool inword;
  bool null_flush;
  bool internal_null_flush;
  bool trace;
  bool printingSteps;
  bool printingRules;
  bool printingMatch;
  bool noCoref;
  int maxLayers;
  int shiftCount;

  void destroy();
  void readData(FILE *input);
  void readInterchunk(string const &input);
  wstring caseOf(wstring const &str);
  wstring copycase(wstring const &source_word, wstring const &target_word);

  bool beginsWith(wstring const &str1, wstring const &str2) const;
  bool endsWith(wstring const &str1, wstring const &str2) const;
  void applyWord(Chunk& chunk);
  int getRule();
  bool applyRule(const wstring& rule);
  Chunk* readToken(FILE *in);
  void interchunk_wrapper_null_flush(FILE *in, FILE *out);
  bool interchunk_do_pass();
  
  void matchNode(Chunk* next);
  void applyReduction(int rule, int len);
  //void checkForReduce();
  vector<ParseNode*> checkForReduce(ParseNode* node);
  void outputAll(FILE* out);
  
  bool popBool();
  int popInt();
  wstring popString();
  Chunk* popChunk();
  inline void pushStack(bool b)
  {
    theStack[++stackIdx].mode = 0;
    theStack[stackIdx].b = b;
  }
  inline void pushStack(int i)
  {
    theStack[++stackIdx].mode = 1;
    theStack[stackIdx].i = i;
  }
  inline void pushStack(wstring s)
  {
    theStack[++stackIdx].mode = 2;
    theStack[stackIdx].s = s;
  }
  inline void pushStack(Chunk* c)
  {
    theStack[++stackIdx].mode = 3;
    theStack[stackIdx].c = c;
  }
  void stackCopy(int src, int dest);
  
public:
  Interchunk();
  ~Interchunk();

  void read(string const &transferfile, string const &datafile);
  void interchunk(FILE *in, FILE *out);
  bool getNullFlush(void);
  void setNullFlush(bool null_flush);
  void setTrace(bool trace);
  void printSteps(bool val)
  {
    printingSteps = val;
  }
  void printRules(bool val)
  {
    printingRules = val;
  }
  void printMatch(bool val)
  {
    printingMatch = val;
  }
  void numLayers(int val)
  {
    maxLayers = val;
  }
  void withoutCoref(bool val)
  {
    noCoref = val;
  }
};

#endif
