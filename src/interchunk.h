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
  wstring chunkPart(ApertiumRE const &part, wstring side = L"tl")
  {
    string chunk;
    if(side == L"tl")
    {
      chunk = UtfConverter::toUtf8(target);
    }
    else if(side == L"sl")
    {
      chunk = UtfConverter::toUtf8(source);
    }
    else
    {
      chunk = UtfConverter::toUtf8(coref);
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
  void updateTags(vector<wstring> parentTags)
  {
    wstring result;
    wstring cur;
    bool indigittag = false;
    for(size_t i = 0; i < target.size(); i++)
    {
      if(!indigittag)
      {
        result += target[i];
        if(target[i] == L'\\')
        {
          result += target[++i];
        }
        else if(target[i] == L'<')
        {
          indigittag = true;
        }
      }
      else if(target[i] == L'>')
      {
        int idx = stoi(cur)-1;
        if(idx < parentTags.size())
        {
          result = result.substr(0, result.size()-1);
          result += parentTags[idx];
          result += L'>';
        }
        cur.clear();
        indigittag = false;
      }
      else if(!isdigit(target[i]))
      {
        result += cur;
        cur.clear();
        result += target[i];
        indigittag = false;
      }
      else
      {
        cur += target[i];
      }
    }
    target = result;
  }
  vector<wstring> getTags()
  {
    vector<wstring> result;
    wstring cur;
    bool intag = false;
    for(int i = 0; i < target.size(); i++)
    {
      if(intag)
      {
        if(target[i] == L'\\')
        {
          cur += target[i];
          cur += target[++i];
        }
        else if(target[i] == L'>')
        {
          if(cur.size() > 0)
          {
            result.push_back(cur);
            cur.clear();
          }
        }
        else
        {
          cur += target[i];
        }
      }
      else if(target[i] == L'<')
      {
        intag = true;
      }
      else if(target[i] == L'\\')
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
};

class StackElement
{
public:
  int mode;
  bool b;
  int i;
  wstring s;
  Chunk* c;
  
  StackElement(bool _b) : mode(0), b(_b) {};
  StackElement(int _i) : mode(1), i(_i) {};
  StackElement(wstring _s) : mode(2), s(_s) {};
  StackElement(Chunk* _c) : mode(3), c(_c) {};
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
  bool allDone;
  bool recursing;
  stack<StackElement> theStack;
  vector<Chunk*> currentInput;
  vector<Chunk*> currentOutput;
  vector<vector<Chunk*>> parseTower;

  FILE *output;
  int any_char;
  int any_tag;

  bool inword;
  bool null_flush;
  bool internal_null_flush;
  bool trace;
  bool printingSteps;
  bool printingRules;

  void destroy();
  void readData(FILE *input);
  void readInterchunk(string const &input);
  wstring caseOf(wstring const &str);
  wstring copycase(wstring const &source_word, wstring const &target_word);

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
  void applyWord(Chunk& chunk);
  void applyRule(wstring rule);
  Chunk* readToken(FILE *in);
  bool checkIndex(xmlNode *element, int index, int limit);
  void interchunk_wrapper_null_flush(FILE *in, FILE *out);
  void interchunk_do_pass();
  void interchunk_linear(FILE *in, FILE *out);
  void interchunk_recursive(FILE *in, FILE *out);
  
  StackElement popStack();
  bool popBool();
  int popInt();
  wstring popString();
  Chunk* popChunk();
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
};

#endif
