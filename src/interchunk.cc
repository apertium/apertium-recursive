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

  Transducer t;
  t.read(in, alphabet.size());

  map<int, int> finals;

  map<int, double> finalWeights = t.getFinals();

  // finals
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    int key = Compression::multibyte_read(in);
    finals[key] = Compression::multibyte_read(in);
    ruleWeights[finals[key]] = finalWeights[key];
  }

  me = new MatchExe(t, finals);

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
  wstring cur;
  for(int i = 0; i < count; i++)
  {
    cur.clear();
    len = fgetwc(in);
    for(int j = 0; j < len; j++)
    {
      cur.append(1, fgetwc(in));
    }
    rule_map.push_back(cur);
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

StackElement
Interchunk::popStack()
{
  StackElement ret = theStack.top();
  theStack.pop();
  return ret;
}

bool
Interchunk::popBool()
{
  StackElement ret = popStack();
  if(ret.mode == 0)
  {
    return ret.b;
  }
  else
  {
    wcerr << "tried to pop bool but mode is " << ret.mode << endl;
    exit(1);
  }
}

int
Interchunk::popInt()
{
  StackElement ret = popStack();
  if(ret.mode == 1)
  {
    return ret.i;
  }
  else
  {
    wcerr << "tried to pop int but mode is " << ret.mode << endl;
    exit(1);
  }
}

wstring
Interchunk::popString()
{
  StackElement ret = popStack();
  if(ret.mode == 2)
  {
    return ret.s;
  }
  else
  {
    wcerr << "tried to pop wstring but mode is " << ret.mode << endl;
    exit(1);
  }
}

Chunk*
Interchunk::popChunk()
{
  StackElement ret = popStack();
  if(ret.mode == 3)
  {
    return ret.c;
  }
  else
  {
    wcerr << "tried to pop Chunk but mode is " << ret.mode << endl;
    exit(1);
  }
}

bool
Interchunk::applyRule(wstring rule)
{
  bool in_let_setup = false;
  for(unsigned int i = 0; i < rule.size(); i++)
  {
    switch(rule[i])
    {
      case DROP:
        if(printingSteps) { wcerr << "drop" << endl; }
        popStack();
        break;
      case DUP:
        if(printingSteps) { wcerr << "dup" << endl; }
        theStack.push(theStack.top());
        break;
      case OVER:
        if(printingSteps) { wcerr << "over" << endl; }
      {
        StackElement a = popStack();
        StackElement b = theStack.top();
        theStack.push(a);
        theStack.push(b);
      }
        break;
      case SWAP:
        if(printingSteps) { wcerr << "swap" << endl; }
      {
        StackElement a = popStack();
        StackElement b = popStack();
        theStack.push(a);
        theStack.push(b);
      }
        break;
      case STRING:
      {
        if(printingSteps) { wcerr << "string" << endl; }
        int ct = rule[++i];
        pushStack(rule.substr(i+1, ct));
        i += ct;
        if(printingSteps) { wcerr << " -> " << theStack.top().s << endl; }
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
        theStack.top().b = !theStack.top().b;
        break;
      case EQUAL:
      case EQUALCL:
        if(printingSteps) { wcerr << "equal" << endl; }
      {
        StackElement _a = popStack();
        wstring a;
        if(_a.mode == 2)
        {
          a = _a.s;
        }
        else if(_a.mode == 3)
        {
          a = _a.c->target;
        }
        else
        {
          wcerr << "not sure how to do equality on mode " << _a.mode << endl;
          exit(1);
        }
        StackElement _b = popStack();
        wstring b;
        if(_b.mode == 2)
        {
          b = _b.s;
        }
        else if(_b.mode == 3)
        {
          b = _b.c->target;
        }
        else
        {
          wcerr << "not sure how to do equality on mode " << _b.mode << endl;
          exit(1);
        }
        if(rule[i] == EQUALCL)
        {
          a = StringUtils::tolower(a);
          b = StringUtils::tolower(b);
        }
        pushStack(a == b);
        if(printingSteps) { wcerr << " -> " << theStack.top().b << endl; }
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

        if(rule[i] == HASPREFIXCL)
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

        if(rule[i] == HASSUFFIXCL)
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
        break;
      case SOURCECLIP:
        if(printingSteps) { wcerr << "sourceclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        pushStack(currentInput[pos]->chunkPart(attr_items[part], L"sl"));
      }
        break;
      case TARGETCLIP:
        if(printingSteps) { wcerr << "targetclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        if(part == L"whole")
        {
          pushStack(currentInput[pos]);
        }
        else if(part == L"chcontent")
        {
          Chunk* ch = new Chunk(L"", currentInput[pos]->contents);
          pushStack(ch);
        }
        else
        {
          pushStack(currentInput[pos]->chunkPart(attr_items[part], L"tl"));
        }
      }
        break;
      case REFERENCECLIP:
        if(printingSteps) { wcerr << "referenceclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        pushStack(currentInput[pos]->chunkPart(attr_items[part], L"ref"));
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
          theStack.top().c->setChunkPart(attr_items[part], popString());
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
        theStack.top().c->contents.push_back(kid);
      }
        break;
      case APPENDSURFACE:
        if(printingSteps) { wcerr << "appendsurface" << endl; }
      {
        wstring s = popString();
        theStack.top().c->target += s;
      }
        break;
      case APPENDALLCHILDREN:
        if(printingSteps) { wcerr << "appendallchildren" << endl; }
      {
        Chunk* ch = popChunk();
        for(unsigned int k = 0; k < ch->contents.size(); k++)
        {
          theStack.top().c->contents.push_back(ch->contents[k]);
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
        src = cur;
      }
      else if(pos == 1)
      {
        dest = cur;
      }
      else if(pos == 2)
      {
        coref = cur;
      }
      pos++;
      cur.clear();
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
Interchunk::applyWord(Chunk& word)
{
  if(word.isBlank)
  {
    if(printingMatch) { wcerr << "stepping blank, size " << ms.size(); }
    ms.step(L' ');
    if(printingMatch) { wcerr << " -> " << ms.size() << endl; }
    return;
  }
  wstring word_str;
  if(word.source.size() > 0)
  {
    word_str = word.source;
  }
  else
  {
    word_str = word.target;
  }
  if(printingMatch) { wcerr << "stepping ^, size " << ms.size(); }
  ms.step(L'^');
  if(printingMatch) { wcerr << " -> " << ms.size() << endl; }
  for(unsigned int i = 0, limit = word_str.size(); i < limit; i++)
  {
    switch(word_str[i])
    {
      case L'\\':
        i++;
        ms.step(towlower(word_str[i]), any_char);
        break;

      case L'<':
        if(printingMatch) { wcerr << "stepping tag, size " << ms.size(); }
        for(unsigned int j = i+1; j != limit; j++)
        {
          if(word_str[j] == L'>')
          {
            int symbol = alphabet(word_str.substr(i, j-i+1));
            if(symbol)
            {
              ms.step(symbol, any_tag);
            }
            else
            {
              ms.step(any_tag);
            }
            i = j;
            break;
          }
        }
        if(printingMatch) { wcerr << " -> " << ms.size() << endl; }
        break;

      case L'{':  // ignore the unmodifiable part of the chunk
        if(printingMatch) { wcerr << "stepping $, size " << ms.size(); }
        ms.step(L'$');
        if(printingMatch) { wcerr << " -> " << ms.size() << endl; }
        return;

      default:
        if(printingMatch) { wcerr << "stepping char, size " << ms.size(); }
        ms.step(towlower(word_str[i]), any_char);
        if(printingMatch) { wcerr << " -> " << ms.size() << endl; }
        break;
    }
  }
  if(printingMatch) { wcerr << "stepping $, size " << ms.size(); }
  ms.step(L'$');
  if(printingMatch) { wcerr << " -> " << ms.size() << endl; }
}

int
Interchunk::getRule()
{
  set<int> skip = rejectedRules;
  int rule = ms.classifyFinals(me->getFinals(), skip);
  if(rule == -1)
  {
    return -1;
  }
  double weight = ruleWeights[rule-1];
  int temp = rule;
  while(true)
  {
    skip.insert(temp);
    temp = ms.classifyFinals(me->getFinals(), skip);
    if(temp == -1)
    {
      break;
    }
    else if(ruleWeights[temp] > weight)
    {
      weight = ruleWeights[temp];
      rule = temp;
    }
  }
  return rule;
}

void
Interchunk::interchunk_do_pass()
{
  int layer = -1;
  int minLayer = furtherInput ? longestPattern : 0;
  for(unsigned int l = 0; l < parseTower.size(); l++)
  {
    if(parseTower[l].size() > minLayer)
    {
      layer = l;
      break;
    }
  }
  if(layer == -1)
  {
    allDone = !furtherInput;
    return;
  }
  if(layer+1 == parseTower.size())
  {
    parseTower.push_back(vector<Chunk*>());
  }
  if(printingRules) { wcerr << "layer is " << layer << endl; }
  ms.init(me->getInitial());
  int rule = -1;
  int len = 0;
  for(size_t i = 0; i < parseTower[layer].size(); i++)
  {
    if(ms.size() == 0)
    {
      break;
    }
    if(i != parseTower[layer].size())
    {
      applyWord(*parseTower[layer][i]);
    }
    int val = getRule();
    if(val != -1)
    {
      rule = val-1;
      len = i+1;
    }
  }
  if(rule != -1)
  {
    if(printingRules)
    {
      wcerr << endl << "applying rule " << rule+1 << endl;
    }
    currentInput.clear();
    currentOutput.clear();
    currentInput.assign(parseTower[layer].begin(), parseTower[layer].begin()+len);
    if(applyRule(rule_map[rule]))
    {
      parseTower[layer].erase(parseTower[layer].begin(), parseTower[layer].begin()+len);
      parseTower[layer+1].insert(parseTower[layer+1].end(), currentOutput.begin(), currentOutput.end());
      rejectedRules.clear();
      if(layer+2 == parseTower.size())
      {
        shiftCount = 0;
      }
    }
    else
    {
      rejectedRules.insert(rule+1);
    }
    currentInput.clear();
    currentOutput.clear();
  }
  else
  {
    if(printingRules) { wcerr << "shifting" << endl; }
    parseTower[layer+1].push_back(parseTower[layer][0]);
    parseTower[layer].erase(parseTower[layer].begin());
    if(layer+2 == parseTower.size() && parseTower[layer+1].size() == 1)
    {
      shiftCount++;
    }
    rejectedRules.clear();
  }
}

void
Interchunk::interchunk(FILE *in, FILE *out)
{
  /*if(getNullFlush())
  {
    interchunk_wrapper_null_flush(in, out);
  }*/
  parseTower.push_back(vector<Chunk*>());
  while(!allDone)
  {
    if(furtherInput)
    {
      Chunk* ch = readToken(in);
      parseTower[0].push_back(ch);
    }
    int shift_was = shiftCount;
    interchunk_do_pass();
    vector<vector<Chunk*>> newTower;
    newTower.push_back(parseTower[0]);
    for(unsigned int i = 1; i < parseTower.size(); i++)
    {
      if(parseTower[i].size() > 0)
      {
        newTower.push_back(parseTower[i]);
      }
    }
    parseTower = newTower;
    int top = parseTower.size()-1;
    if(shiftCount == 1 && parseTower[top][0]->isBlank)
    {
      parseTower[top][0]->output(out);
      parseTower.pop_back();
      shiftCount = 0;
    }
    else if(top == 0 && parseTower[0].size() == 0)
    {
      allDone = !furtherInput;
    }
    else if(maxLayers > 0 && top >= maxLayers)
    {
      for(unsigned int i = 0; i < parseTower[top].size(); i++)
      {
        parseTower[top][i]->output(out);
      }
      parseTower[top].clear();
      shiftCount = 0;
    }
    else if(shiftCount > longestPattern ||
            (shiftCount > shift_was && top == 1 && parseTower[0].size() == 0 && !furtherInput))
    {
      parseTower[top][0]->output(out);
      parseTower[top].erase(parseTower[top].begin());
      shiftCount--;
    }
  }
}
