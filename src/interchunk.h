#ifndef __RTXINTERCHUNK__
#define __RTXINTERCHUNK__

#include <apertium/transfer_instr.h>
#include <apertium/transfer_token.h>
#include <apertium/interchunk_word.h>
#include <apertium/apertium_re.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/buffer.h>
#include <lttoolbox/ltstr.h>
#include <lttoolbox/match_exe.h>
#include <lttoolbox/match_state.h>

#include <cstring>
#include <cstdio>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <set>
#include <vector>
#include <stack>

using namespace std;

class Chunk
{
public:
  string surface;
  vector<Chunk*> contents;
  void
  setChunkPart(ApertiumRE const &part, string const &value)
  {
    part.replace(surface, value);
  }
  void
  addPiece(Chunk* piece)
  {
    surface += piece->surface;
    for(int i = 0; i < piece->contents.size(); i++)
    {
      contents.push_back(piece->contents[i]);
    }
  }
};

class StackElement
{
public:
  int mode;
  bool b;
  int i;
  string s;
  Chunk* c;
  pair<int, string> clip;
  
  StackElement(bool _b) : mode(0), b(_b) {};
  StackElement(int _i) : mode(1), i(_i) {};
  StackElement(string _s) : mode(2), s(_s) {};
  StackElement(Chunk* _c) : mode(3), c(_c) {};
  StackElement(pair<int, string> _clip) : mode(4), clip(_clip) {};
};

class Interchunk
{
private:

  Alphabet alphabet;
  MatchExe *me;
  MatchState ms;
  map<string, ApertiumRE, Ltstr> attr_items;
  map<string, string, Ltstr> variables;
  map<string, int, Ltstr> macros;
  map<string, set<string, Ltstr>, Ltstr> lists;
  map<string, set<string, Ltstr>, Ltstr> listslow;
  vector<string> rules;
  int longestPattern;
  stack<StackElement> theStack;
  vector<Chunk*> currentOutput;
  InterchunkWord **word;
  string **blank;
  int lword, lblank;
  Buffer<TransferToken> input_buffer;
  vector<wstring *> tmpword;
  vector<wstring *> tmpblank;

  FILE *output;
  int any_char;
  int any_tag;

  xmlNode *lastrule;
  unsigned int nwords;

  map<xmlNode *, TransferInstr> evalStringCache;
  bool inword;
  bool null_flush;
  bool internal_null_flush;
  bool trace;
  string emptyblank;

  void destroy();
  void readData(FILE *input);
  void readInterchunk(string const &input);
  void collectMacros(xmlNode *localroot);
  void collectRules(xmlNode *localroot);
  string caseOf(string const &str);
  string copycase(string const &source_word, string const &target_word);

  void processAppend(xmlNode *localroot);
  void processModifyCase(xmlNode *localroot);
  void processInstruction(string rule);
  string processChunk(xmlNode *localroot);

  bool beginsWith(string const &str1, string const &str2) const;
  bool endsWith(string const &str1, string const &str2) const;
  string tolower(string const &str) const;
  string tags(string const &str) const;
  string readWord(FILE *in);
  string readBlank(FILE *in);
  string readUntil(FILE *in, int const symbol) const;
  void applyWord(wstring const &word_str);
  void applyRule();
  TransferToken & readToken(FILE *in);
  bool checkIndex(xmlNode *element, int index, int limit);
  void interchunk_wrapper_null_flush(FILE *in, FILE *out);
  
  StackElement popStack();
  void pushStack(bool b)
  {
    StackElement el(b);
    theStack.push(el);
  }
  void pushStack(int i)
  {
    StackElement el(i);
    theStack.push(el);
  }
  void pushStack(string s)
  {
    StackElement el(s);
    theStack.push(el);
  }
  void pushStack(Chunk* c)
  {
    StackElement el(c);
    theStack.push(el);
  }
  void pushStack(pair<int, string> clip)
  {
    StackElement el(clip);
    theStack.push(el);
  }

public:
  Interchunk();
  ~Interchunk();

  void read(string const &transferfile, string const &datafile);
  void interchunk(FILE *in, FILE *out);
  bool getNullFlush(void);
  void setNullFlush(bool null_flush);
  void setTrace(bool trace);
};

#endif
