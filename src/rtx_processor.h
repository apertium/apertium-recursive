#ifndef __RTXPROCESSOR__
#define __RTXPROCESSOR__

#include <apertium/apertium_re.h>
#include <apertium/utf_converter.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/buffer.h>
#include <lttoolbox/ltstr.h>
#include <matcher.h>
#include <chunk.h>
#include <pool.h>

#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

using namespace std;

struct StackElement
{
  int mode;
  bool b;
  int i;
  wstring s;
  Chunk* c;
};

class RTXProcessor
{
private:

  Alphabet alphabet;
  MatchExe2 *mx;
  map<wstring, ApertiumRE, Ltstr> attr_items;
  map<wstring, wstring, Ltstr> variables;
  map<wstring, int, Ltstr> macros;
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;
  map<wstring, set<wstring, Ltstr>, Ltstr> listslow;
  vector<wstring> rule_map;
  vector<wstring> output_rules;
  vector<int> pat_size;
  int longestPattern;
  bool furtherInput;
  bool allDone;
  map<int, double> ruleWeights;
  StackElement theStack[32];
  int stackIdx;
  vector<Chunk*> currentInput;
  vector<Chunk*> currentOutput;
  Chunk* parentChunk;
  list<Chunk*> outputQueue;
  vector<ParseNode*> parseGraph;
  Pool<Chunk> chunkPool;
  Pool<ParseNode> parsePool;

  FILE *output;

  bool inword;
  bool null_flush;
  bool internal_null_flush;
  bool trace;
  bool printingSteps;
  bool printingRules;
  bool printingMatch;
  bool noCoref;
  bool isLinear;

  void destroy();
  void readData(FILE *input);
  void readRTXProcessor(string const &input);
  wstring caseOf(wstring const &str);
  wstring copycase(wstring const &source_word, wstring const &target_word);

  bool beginsWith(wstring const &str1, wstring const &str2) const;
  bool endsWith(wstring const &str1, wstring const &str2) const;
  bool applyRule(const wstring& rule);
  Chunk* readToken(FILE *in);
  
  void checkForReduce(vector<ParseNode*>& result, ParseNode* node);
  void outputAll(FILE* out);
  void processGLR(FILE* in, FILE* out);

  void processTRXLayer(list<Chunk*>& t1x, list<Chunk*>& t2x);
  void processTRX(FILE* in, FILE* out);
  
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
  RTXProcessor();
  ~RTXProcessor();

  void read(string const &transferfile, string const &datafile);
  void process(FILE *in, FILE *out);
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
  void withoutCoref(bool val)
  {
    noCoref = val;
  }
  void mimicChunker(bool val)
  {
    isLinear = val;
  }
};

#endif
