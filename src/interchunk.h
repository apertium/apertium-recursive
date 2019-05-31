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
  wstring surface;
  wstring chunkData;
  vector<Chunk*> contents;
  Chunk()
  {
  }
  Chunk(wstring blankContent)
  {
    surface = blankContent;
  }
  Chunk(wstring chunkTags, wstring chunkContent)
  {
    surface = chunkTags;
    chunkData = chunkContent;
  }
  Chunk(wstring chunkTags, vector<Chunk*> children)
  {
    surface = chunkTags;
    contents = children;
  }
  void
  setChunkPart(ApertiumRE const &part, string const &value)
  {
    string surf = UtfConverter::toUtf8(surface);
    part.replace(surf, value);
    surface = UtfConverter::fromUtf8(surf);
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
  void updateTags(vector<wstring> parentTags)
  {
    wstring result;
    wstring cur;
    bool indigittag = false;
    for(int i = 0; i < surface.size(); i++)
    {
      if(!indigittag)
      {
        result += surface[i];
        if(surface[i] == L'\\')
        {
          result += surface[++i];
        }
        else if(surface[i] == L'<')
        {
          indigittag = true;
        }
      }
      else if(surface[i] == L'>')
      {
        result += parentTags[stoi(cur)-1];
        result += L'>';
        cur.clear();
        indigittag = false;
      }
      else if(!isdigit(surface[i]))
      {
        result += cur;
        cur.clear();
        result += surface[i];
        indigittag = false;
      }
      else
      {
        cur += surface[i];
      }
    }
    surface = result;
  }
  vector<wstring> getTags()
  {
    vector<wstring> result;
    wstring cur;
    bool intag = false;
    for(int i; i < surface.size(); i++)
    {
      if(intag)
      {
        if(surface[i] == L'\\')
        {
          cur += surface[i];
          cur += surface[++i];
        }
        else if(surface[i] == L'>')
        {
          if(cur.size() > 0)
          {
            result.push_back(cur);
            cur.clear();
          }
        }
        else
        {
          cur += surface[i];
        }
      }
      else if(surface[i] == L'<')
      {
        intag = true;
      }
      else if(surface[i] == L'\\')
      {
        i++;
      }
    }
    return result;
  }
  void output(vector<wstring> parentTags, FILE* out = NULL)
  {
    updateTags(parentTags);
    if(contents.size() > 0)
    {
      vector<wstring> tags = getTags();
      for(int i = 0; i < contents.size(); i++)
      {
        contents[i]->output(tags, out);
      }
    }
    else if(chunkData.size() == 0)
    {
      if(out == NULL)
      {
        cout << UtfConverter::toUtf8(surface);
      }
      else
      {
        fputws_unlocked(surface.c_str(), out);
      }
    }
    else
    {
      if(out == NULL)
      {
        cout << "^" << UtfConverter::toUtf8(surface);
        cout << "{" << UtfConverter::toUtf8(chunkData) << "}$";
      }
      else
      {
        fputwc_unlocked(L'^', out);
        fputws_unlocked(surface.c_str(), out);
        fputwc_unlocked(L'{', out);
        fputws_unlocked(chunkData.c_str(), out);
        fputwc_unlocked(L'}', out);
        fputwc_unlocked(L'$', out);
      }
    }
  }
};

class StackElement
{
public:
  int mode;
  bool b;
  int i;
  wstring s;
  Chunk* c;
  pair<int, wstring> clip;
  
  StackElement(bool _b) : mode(0), b(_b) {};
  StackElement(int _i) : mode(1), i(_i) {};
  StackElement(wstring _s) : mode(2), s(_s) {};
  StackElement(Chunk* _c) : mode(3), c(_c) {};
  StackElement(pair<int, wstring> _clip) : mode(4), clip(_clip) {};
};

class Interchunk
{
private:

  Alphabet alphabet;
  MatchExe *me;
  MatchState ms;
  map<wstring, ApertiumRE, Ltstr> attr_items;
  map<wstring, wstring, Ltstr> variables;
  map<wstring, int, Ltstr> macros;
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;
  map<wstring, set<wstring, Ltstr>, Ltstr> listslow;
  vector<wstring> rule_map;
  int longestPattern;
  bool furtherInput;
  bool recursing;
  stack<StackElement> theStack;
  vector<Chunk*> currentOutput;
  vector<vector<Chunk*>> parseTower;
  InterchunkWord **word;
  string **blank;
  int lword, lblank;
  Buffer<Chunk> input_buffer;
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
  void processInstruction(wstring rule);
  string processChunk(xmlNode *localroot);

  bool beginsWith(wstring const &str1, wstring const &str2) const;
  bool endsWith(wstring const &str1, wstring const &str2) const;
  wstring tolower(wstring const &str) const;
  string tags(string const &str) const;
  string readWord(FILE *in);
  string readBlank(FILE *in);
  string readUntil(FILE *in, int const symbol) const;
  void applyWord(Chunk* chunk);
  void applyRule();
  Chunk & readToken(FILE *in);
  bool checkIndex(xmlNode *element, int index, int limit);
  void interchunk_wrapper_null_flush(FILE *in, FILE *out);
  void interchunk_linear(FILE *in, FILE *out);
  void interchunk_recursive(FILE *in, FILE *out);
  
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
  void pushStack(wstring s)
  {
    StackElement el(s);
    theStack.push(el);
  }
  void pushStack(Chunk* c)
  {
    StackElement el(c);
    theStack.push(el);
  }
  void pushStack(pair<int, wstring> clip)
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
