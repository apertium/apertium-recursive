#include <interchunk.h>
#include <bytecode.h>
#include <apertium/trx_reader.h>
#include <apertium/utf_converter.h>
#include <lttoolbox/compression.h>
#include <lttoolbox/xml_parse_util.h>

#include <cctype>
#include <cerrno>
#include <iostream>
#include <stack>
#include <deque>
#include <apertium/string_utils.h>
//#include <apertium/unlocked_cstdio.h>

using namespace Apertium;
using namespace std;

Interchunk::Interchunk()
{
  furtherInput = true;
  inword = false;
  allDone = false;
  maxLayers = -1;
  shiftCount = 0;
  printingSteps = false;
  printingRules = false;
  printingMatch = false;
}

Interchunk::~Interchunk()
{
}

void
Interchunk::readData(FILE *in)
{
  alphabet.read(in);
  any_char = alphabet(TRXReader::ANY_CHAR);
  any_tag = alphabet(TRXReader::ANY_TAG);

  //Transducer t;
  Transducer* t = new Transducer();
  t->read(in, alphabet.size());

  map<int, int> finals;

  map<int, double> finalWeights = t->getFinals();

  // finals
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    int key = Compression::multibyte_read(in);
    finals[key] = Compression::multibyte_read(in);
    ruleWeights[finals[key]] = finalWeights[key];
  }

  me = new MatchExe(*t, finals);
  mx = new MatchExe2(*t, &alphabet, finals);

  // attr_items
  bool recompile_attrs = Compression::string_read(in) != string(pcre_version());
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    wstring const cad_k = Compression::wstring_read(in);
    attr_items[cad_k].read(in);
    wstring fallback = Compression::wstring_read(in);
    if(recompile_attrs) {
      attr_items[cad_k].compile(UtfConverter::toUtf8(fallback));
    }
  }

  // variables
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    wstring const cad_k = Compression::wstring_read(in);
    variables[cad_k] = Compression::wstring_read(in);
  }

  // macros
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    wstring const cad_k = Compression::wstring_read(in);
    macros[cad_k] = Compression::multibyte_read(in);
  }

  // lists
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    wstring const cad_k = Compression::wstring_read(in);

    for(int j = 0, limit2 = Compression::multibyte_read(in); j != limit2; j++)
    {
      wstring const cad_v = Compression::wstring_read(in);
      lists[cad_k].insert(cad_v);
      listslow[cad_k].insert(StringUtils::tolower(cad_v));
    }
  }
}

void
Interchunk::read(string const &transferfile, string const &datafile)
{
  // rules
  FILE *in = fopen(transferfile.c_str(), "rb");
  longestPattern = 2*fgetwc(in)-1;
  int count = fgetwc(in);
  int len;
  int patlen;
  wstring cur;
  for(int i = 0; i < count; i++)
  {
    cur.clear();
    len = fgetwc(in);
    patlen = fgetwc(in);
    for(int j = 0; j < len; j++)
    {
      cur.append(1, fgetwc(in));
    }
    rule_map.push_back(cur);
    pat_size.push_back(patlen);
  }
  fclose(in);

  // datafile
  FILE *datain = fopen(datafile.c_str(), "rb");
  if(!datain)
  {
    wcerr << "Error: Could not open file '" << datafile << "'." << endl;
    exit(EXIT_FAILURE);
  }
  readData(datain);
  fclose(datain);
}

bool
Interchunk::beginsWith(wstring const &s1, wstring const &s2) const
{
  int const limit = s2.size(), constraint = s1.size();

  if(constraint < limit)
  {
    return false;
  }
  for(int i = 0; i != limit; i++)
  {
    if(s1[i] != s2[i])
    {
      return false;
    }
  }

  return true;
}

bool
Interchunk::endsWith(wstring const &s1, wstring const &s2) const
{
  int const limit = s2.size(), constraint = s1.size();

  if(constraint < limit)
  {
    return false;
  }
  for(int i = limit-1, j = constraint - 1; i >= 0; i--, j--)
  {
    if(s1[j] != s2[i])
    {
      return false;
    }
  }

  return true;
}

wstring
Interchunk::copycase(wstring const &source_word, wstring const &target_word)
{
  wstring result;

  bool firstupper = iswupper(source_word[0]);
  bool uppercase = firstupper && iswupper(source_word[source_word.size()-1]);
  bool sizeone = source_word.size() == 1;

  if(!uppercase || (sizeone && uppercase))
  {
    result = StringUtils::tolower(target_word);
  }
  else
  {
    result = StringUtils::toupper(target_word);
  }

  if(firstupper)
  {
    result[0] = towupper(result[0]);
  }

  return result;
}

wstring
Interchunk::caseOf(wstring const &s)
{
  return copycase(s, wstring(L"aa"));
}

inline bool
Interchunk::popBool()
{
  if(theStack[stackIdx].mode == 0)
  {
    return theStack[stackIdx--].b;
  }
  else
  {
    wcerr << "tried to pop bool but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline int
Interchunk::popInt()
{
  if(theStack[stackIdx].mode == 1)
  {
    return theStack[stackIdx--].i;
  }
  else
  {
    wcerr << "tried to pop int but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline wstring
Interchunk::popString()
{
  if(theStack[stackIdx].mode == 2)
  {
    return theStack[stackIdx--].s;
  }
  else
  {
    wcerr << "tried to pop wstring but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline Chunk*
Interchunk::popChunk()
{
  if(theStack[stackIdx].mode == 3)
  {
    return theStack[stackIdx--].c;
  }
  else
  {
    wcerr << "tried to pop Chunk but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline void
Interchunk::stackCopy(int src, int dest)
{
  theStack[dest].mode = theStack[src].mode;
  switch(theStack[src].mode)
  {
    case 0:
      theStack[dest].b = theStack[src].b;
      break;
    case 1:
      theStack[dest].i = theStack[src].i;
      break;
    case 2:
      theStack[dest].s = theStack[src].s;
      break;
    case 3:
      theStack[dest].c = theStack[src].c;
      break;
    default:
      wcerr << "Unknown StackElement mode " << theStack[src].mode;
      break;
  }
}

bool
Interchunk::applyRule(const wstring& rule)
{
  stackIdx = 0;
  for(unsigned int i = 0; i < rule.size(); i++)
  {
    switch(rule[i])
    {
      case DROP:
        if(printingSteps) { wcerr << "drop" << endl; }
        stackIdx--;
        break;
      case DUP:
        if(printingSteps) { wcerr << "dup" << endl; }
        stackCopy(stackIdx, stackIdx+1);
        stackIdx++;
        break;
      case OVER:
        if(printingSteps) { wcerr << "over" << endl; }
        stackCopy(stackIdx-1, stackIdx+1);
        stackIdx++;
        break;
      case SWAP:
        if(printingSteps) { wcerr << "swap" << endl; }
      {
        stackCopy(stackIdx, stackIdx+1);
        stackCopy(stackIdx-1, stackIdx);
        stackCopy(stackIdx+1, stackIdx-1);
      }
        break;
      case STRING:
      {
        if(printingSteps) { wcerr << "string" << endl; }
        int ct = rule[++i];
        pushStack(rule.substr(i+1, ct));
        i += ct;
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case INT:
        if(printingSteps) { wcerr << "int" << endl; }
        pushStack((int)rule[++i]);
        break;
      case PUSHFALSE:
        if(printingSteps) { wcerr << "pushfalse" << endl; }
        pushStack(false);
        break;
      case PUSHTRUE:
        if(printingSteps) { wcerr << "pushtrue" << endl; }
        pushStack(true);
        break;
      case JUMP:
        if(printingSteps) { wcerr << "jump" << endl; }
        i += rule[++i];
        break;
      case JUMPONTRUE:
        if(printingSteps) { wcerr << "jumpontrue" << endl; }
        if(!popBool())
        {
          i++;
        }
        else
        {
          i += rule[++i];
        }
        break;
      case JUMPONFALSE:
        if(printingSteps) { wcerr << "jumponfalse" << endl; }
        if(popBool())
        {
          i++;
        }
        else
        {
          i += rule[++i];
        }
        break;
      case AND:
        if(printingSteps) { wcerr << "and" << endl; }
      {
        bool a = popBool();
        bool b = popBool();
        pushStack(a && b);
      }
        break;
      case OR:
        if(printingSteps) { wcerr << "or" << endl; }
      {
        bool a = popBool();
        bool b = popBool();
        pushStack(a || b);
      }
        break;
      case NOT:
        if(printingSteps) { wcerr << "not" << endl; }
        theStack[stackIdx].b = !theStack[stackIdx].b;
        break;
      case EQUAL:
      case EQUALCL:
        if(printingSteps) { wcerr << "equal" << endl; }
      {
        wstring a;
        if(theStack[stackIdx].mode == 2)
        {
          a = theStack[stackIdx--].s;
        }
        else if(theStack[stackIdx].mode == 3)
        {
          a = theStack[stackIdx--].c->target;
        }
        else
        {
          wcerr << "not sure how to do equality on mode " << theStack[stackIdx].mode << endl;
          exit(1);
        }
        wstring b;
        if(theStack[stackIdx].mode == 2)
        {
          b = theStack[stackIdx--].s;
        }
        else if(theStack[stackIdx].mode == 3)
        {
          b = theStack[stackIdx--].c->target;
        }
        else
        {
          wcerr << "not sure how to do equality on mode " << theStack[stackIdx].mode << endl;
          exit(1);
        }
        if(rule[i] == EQUALCL)
        {
          a = StringUtils::tolower(a);
          b = StringUtils::tolower(b);
        }
        pushStack(a == b);
      }
        break;
      case ISPREFIX:
      case ISPREFIXCL:
        if(printingSteps) { wcerr << "isprefix" << endl; }
      {
        wstring substr = popString();
        wstring str = popString();
        if(rule[i] == ISPREFIXCL)
        {
          pushStack(beginsWith(StringUtils::tolower(str), StringUtils::tolower(substr)));
        }
        else
        {
          pushStack(beginsWith(str, substr));
        }
      }
        break;
      case ISSUFFIX:
      case ISSUFFIXCL:
        if(printingSteps) { wcerr << "issuffix" << endl; }
      {
        wstring substr = popString();
        wstring str = popString();
        if(rule[i] == ISSUFFIXCL)
        {
          pushStack(endsWith(StringUtils::tolower(str), StringUtils::tolower(substr)));
        }
        else
        {
          pushStack(endsWith(str, substr));
        }
      }
        break;
      case HASPREFIX:
      case HASPREFIXCL:
        if(printingSteps) { wcerr << "hasprefix" << endl; }
      {
        wstring list = popString();
        wstring needle = popString();
        set<wstring, Ltstr>::iterator it, limit;

        if(rule[i] == HASPREFIX)
        {
          it = lists[list].begin();
          limit = lists[list].end();
        }
        else
        {
          needle = StringUtils::tolower(needle);
          it = listslow[list].begin();
          limit = listslow[list].end();
        }

        bool found = false;
        for(; it != limit; it++)
        {
          if(beginsWith(needle, *it))
          {
            found = true;
            break;
          }
        }
        pushStack(found);
      }
        break;
      case HASSUFFIX:
      case HASSUFFIXCL:
        if(printingSteps) { wcerr << "hassuffix" << endl; }
      {
        wstring list = popString();
        wstring needle = popString();
        set<wstring, Ltstr>::iterator it, limit;

        if(rule[i] == HASSUFFIX)
        {
          it = lists[list].begin();
          limit = lists[list].end();
        }
        else
        {
          needle = StringUtils::tolower(needle);
          it = listslow[list].begin();
          limit = listslow[list].end();
        }

        bool found = false;
        for(; it != limit; it++)
        {
          if(endsWith(needle, *it))
          {
            found = true;
            break;
          }
        }
        pushStack(found);
      }
        break;
      case ISSUBSTRING:
      case ISSUBSTRINGCL:
        if(printingSteps) { wcerr << "issubstring" << endl; }
      {
        wstring needle = popString();
        wstring haystack = popString();
        if(rule[i] == ISSUBSTRINGCL)
        {
          needle = StringUtils::tolower(needle);
          haystack = StringUtils::tolower(haystack);
        }
        pushStack(haystack.find(needle) != wstring::npos);
      }
        break;
      case IN:
      case INCL:
        if(printingSteps) { wcerr << "in" << endl; }
      {
        wstring list = popString();
        wstring str = popString();
        if(rule[i] == INCL)
        {
          str = StringUtils::tolower(str);
          set<wstring, Ltstr> &myset = listslow[list];
          pushStack(myset.find(str) != myset.end());
        }
        else
        {
          set<wstring, Ltstr> &myset = lists[list];
          pushStack(myset.find(str) != myset.end());
        }
      }
        break;
      case SETVAR:
        if(printingSteps) { wcerr << "setvar" << endl; }
      {
        wstring var = popString();
        wstring val = popString();
        variables[var] = val;
      }
        break;
      case OUTPUT:
        if(printingSteps) { wcerr << "out" << endl; }
        currentOutput.push_back(popChunk());
        if(printingSteps) { wcerr << " -> " << currentOutput.back()->target << endl; }
        break;
      case SOURCECLIP:
        if(printingSteps) { wcerr << "sourceclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        pushStack(currentInput[pos]->chunkPart(attr_items[part], SourceClip));
      }
        break;
      case TARGETCLIP:
        if(printingSteps) { wcerr << "targetclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        if(part == L"whole")
        {
          Chunk* ch = new Chunk;
          ch->isBlank = false;
          ch->target = currentInput[pos]->target;
          ch->contents = currentInput[pos]->contents;
          pushStack(ch);
        }
        else if(part == L"chcontent")
        {
          Chunk* ch = new Chunk(L"", currentInput[pos]->contents);
          pushStack(ch);
        }
        else
        {
          pushStack(currentInput[pos]->chunkPart(attr_items[part], TargetClip));
        }
      }
        break;
      case REFERENCECLIP:
        if(printingSteps) { wcerr << "referenceclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        pushStack(currentInput[pos]->chunkPart(attr_items[part], ReferenceClip));
      }
        break;
      case SETCLIP:
        if(printingSteps) { wcerr << "setclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        if(pos >= 0)
        {
          currentInput[pos]->setChunkPart(attr_items[part], popString());
        }
        else
        {
          theStack[stackIdx].c->setChunkPart(attr_items[part], popString());
        }
      }
        break;
      case FETCHVAR:
        if(printingSteps) { wcerr << "fetchvar" << endl; }
        pushStack(variables[popString()]);
        break;
      case GETCASE:
        if(printingSteps) { wcerr << "getcase" << endl; }
        pushStack(caseOf(popString()));
        break;
      case SETCASE:
        if(printingSteps) { wcerr << "setcase" << endl; }
      {
        wstring src = popString();
        wstring dest = popString();
        pushStack(copycase(src, dest));
      }
        break;
      case CONCAT:
        if(printingSteps) { wcerr << "concat" << endl; }
      {
        wstring result = popString();
        result += popString();
        pushStack(result);
      }
        break;
      case CHUNK:
        if(printingSteps) { wcerr << "chunk" << endl; }
      {
        Chunk* ch = new Chunk();
        ch->isBlank = false;
        pushStack(ch);
      }
        break;
      case APPENDCHILD:
        if(printingSteps) { wcerr << "appendchild" << endl; }
      {
        Chunk* kid = popChunk();
        theStack[stackIdx].c->contents.push_back(kid);
      }
        break;
      case APPENDSURFACE:
        if(printingSteps) { wcerr << "appendsurface" << endl; }
      {
        wstring s = popString();
        theStack[stackIdx].c->target += s;
      }
        break;
      case APPENDALLCHILDREN:
        if(printingSteps) { wcerr << "appendallchildren" << endl; }
      {
        Chunk* ch = popChunk();
        for(unsigned int k = 0; k < ch->contents.size(); k++)
        {
          theStack[stackIdx].c->contents.push_back(ch->contents[k]);
        }
      }
        break;
      case BLANK:
        if(printingSteps) { wcerr << "blank" << endl; }
      {
        int loc = 2*(popInt()-1) + 1;
        if(loc == -1)
        {
          pushStack(new Chunk(L" "));
        }
        else
        {
          pushStack(currentInput[loc]);
        }
      }
        break;
      case REJECTRULE:
        if(printingSteps) { wcerr << "rejectrule" << endl; }
        return false;
        break;
      case DISTAG:
        if(printingSteps) { wcerr << "distag" << endl; }
      {
        wstring s = popString();
        if(s.size() > 0 && s[0] == L'<' && s[s.size()-1] == L'>')
        {
          s = s.substr(1, s.size()-2);
        }
        pushStack(s);
      }
        break;
      default:
        wcerr << "unknown instruction: " << rule[i] << endl;
        exit(1);
    }
  }
  return true;
}

Chunk *
Interchunk::readToken(FILE *in)
{
  int pos = 0;
  wstring cur;
  wstring src;
  wstring dest;
  wstring coref;
  cur.reserve(256);
  while(true)
  {
    int val = fgetwc_unlocked(in);
    if(feof(in) || (internal_null_flush && val == 0))
    {
      furtherInput = false;
      Chunk* ret = new Chunk(cur);
      return ret;
    }
    else if(val == L'\\')
    {
      cur += L'\\';
      cur += wchar_t(fgetwc_unlocked(in));
    }
    else if(val == L'[')
    {
      cur += L'[';
      while(true)
      {
        int val2 = fgetwc_unlocked(in);
        if(val2 == L'\\')
        {
          cur += L'\\';
          cur += wchar_t(fgetwc_unlocked(in));
        }
        else if(val2 == L']')
        {
          cur += L']';
          break;
        }
        else
        {
          cur += wchar_t(val2);
        }
      }
    }
    else if(inword && (val == L'$' || val == L'/'))
    {
      if(pos == 0)
      {
        src.swap(cur);
      }
      else if(pos == 1)
      {
        dest.swap(cur);
      }
      else if(pos >= 2 && !noCoref && val == L'$')
      {
        coref.swap(cur);
      }
      else
      {
        cur.clear();
      }
      pos++;
      if(val == L'$')
      {
        inword = false;
        Chunk* ret = new Chunk(src, dest, coref);
        return ret;
      }
    }
    else if(!inword && val == L'^')
    {
      inword = true;
      Chunk* ret = new Chunk(cur);
      return ret;
    }
    else
    {
      cur += wchar_t(val);
    }
  }
}

bool
Interchunk::getNullFlush(void)
{
  return null_flush;
}

void
Interchunk::setNullFlush(bool null_flush)
{
  this->null_flush = null_flush;
}

void
Interchunk::setTrace(bool trace)
{
  this->trace = trace;
}

void
Interchunk::interchunk_wrapper_null_flush(FILE *in, FILE *out)
{
  null_flush = false;
  internal_null_flush = true;

  while(!feof(in))
  {
    interchunk(in, out);
    fputwc_unlocked(L'\0', out);
    int code = fflush(out);
    if(code != 0)
    {
      wcerr << L"Could not flush output " << errno << endl;
    }
  }
  internal_null_flush = false;
  null_flush = true;
}

void
Interchunk::matchNode(Chunk* next)
{
  if(next->isBlank)
  {
    mx->matchBlank();
  }
  else if(next->source.size() == 0)
  {
    mx->matchChunk(next->target);
  }
  else
  {
    mx->matchChunk(next->source);
  }
  parseStack.push(next);
}

void
Interchunk::applyReduction(int rule, int len)
{
  currentInput.resize(len);
  for(int i = 0; i < len; i++)
  {
    currentInput[len-i-1] = parseStack.top();
    parseStack.pop();
  }
  mx->popStack(len);
  currentInput.clear();
  currentOutput.clear();
  applyRule(rule_map[rule-1]);
  vector<Chunk*> out;
  out.swap(currentOutput);
  // calling checkForReduce() can modify currentOutput, so make a copy
  for(unsigned int i = 0; i < out.size(); i++)
  {
    matchNode(out[i]);
    checkForReduce();
  }
}

void
Interchunk::checkForReduce()
{
  if(mx->stackSize() > 0)
  {
    int rule = mx->getRule();
    if(rule != -1)
    {
      if(!outputting)
      {
        for(unsigned int i = 0; i < inputBuffer.size(); i++)
        {
          if(!inputBuffer[i]->isBlank)
          {
            wstring surf = inputBuffer[i]->source;
            if(surf.size() == 0)
            {
              surf = inputBuffer[i]->target;
            }
            if(mx->shouldShift(surf))
            {
              return;
            }
          }
        }
      }
      if(printingRules) { wcerr << "Applying rule " << rule << endl; }
      applyReduction(rule, pat_size[rule-1]);
    }
  }
}

void
Interchunk::outputAll(FILE* out)
{
  outputting = true;
  stack<Chunk*> temp;
  while(parseStack.size() > 0)
  {
    checkForReduce();
    temp.push(parseStack.top());
    parseStack.pop();
    mx->popStack(1);
  }
  while(temp.size() > 0)
  {
    temp.top()->output(out);
    temp.pop();
  }
}

void
Interchunk::interchunk(FILE *in, FILE *out)
{
  Chunk* next;
  while(true)
  {
    while(furtherInput && inputBuffer.size() < 5)
    {
      inputBuffer.push_back(readToken(in));
    }
    next = inputBuffer.front();
    inputBuffer.pop_front();
    matchNode(next);
    if(mx->stateSize() == 0)
    {
      outputAll(out);
    }
    else
    {
      checkForReduce();
    }
    if(inputBuffer.size() == 0 && !furtherInput)
    {
      outputAll(out);
      break;
    }
  }
}
