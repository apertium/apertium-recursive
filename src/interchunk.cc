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

  // finals
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    int key = Compression::multibyte_read(in);
    finals[key] = Compression::multibyte_read(in);
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

void
Interchunk::applyRule(wstring rule)
{
  bool in_let_setup = false;
  for(unsigned int i = 0; i < rule.size(); i++)
  {
    switch(rule[i])
    {
      case L's': // string
      {
        if(printingSteps) { wcerr << "string" << endl; }
        int ct = rule[++i];
        pushStack(rule.substr(i+1, ct));
        i += ct;
        if(printingSteps) { wcerr << " -> " << theStack.top().s << endl; }
      }
        break;
      case L'j': // jump
        if(printingSteps) { wcerr << "jump" << endl; }
        i += rule[++i];
        break;
      case L'?': // test
        if(printingSteps) { wcerr << "test" << endl; }
        if(popBool())
        {
          if(printingSteps) { wcerr << " -> passed" << endl; }
          i++;
        }
        else
        {
          i += rule[++i];
        }
        break;
      case L'&': // and
        if(printingSteps) { wcerr << "and" << endl; }
      {
        int count = rule[++i];
        bool val = true;
        for(int j = 0; j < count; j++)
        {
          val = popBool() && val;
        }
        pushStack(val);
      }
        break;
      case L'|': // or
        if(printingSteps) { wcerr << "or" << endl; }
      {
        int count = rule[++i];
        bool val = false;
        for(int j = 0; j < count; j++)
        {
          val = popBool() || val;
        }
        pushStack(val);
      }
        break;
      case L'!': // not
        if(printingSteps) { wcerr << "not" << endl; }
        theStack.top().b = !theStack.top().b;
        break;
      case L'=': // equal
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
        //wstring a = popStack().s;
        //wstring b = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          a = StringUtils::tolower(a);
          b = StringUtils::tolower(b);
        }
        pushStack(a == b);
        if(printingSteps) { wcerr << " -> " << theStack.top().b << endl; }
      }
        break;
      case L'(': // begins-with
        if(printingSteps) { wcerr << "begins-with" << endl; }
      {
        wstring substr = popString();
        wstring str = popString();
        if(rule[i+1] == '#')
        {
          i++;
          pushStack(beginsWith(StringUtils::tolower(str), StringUtils::tolower(substr)));
        }
        else
        {
          pushStack(beginsWith(str, substr));
        }
      }
        break;
      case L')': // ends-with
        if(printingSteps) { wcerr << "ends-with" << endl; }
      {
        wstring substr = popString();
        wstring str = popString();
        if(rule[i+1] == '#')
        {
          i++;
          pushStack(endsWith(StringUtils::tolower(str), StringUtils::tolower(substr)));
        }
        else
        {
          pushStack(endsWith(str, substr));
        }
      }
        break;
      case L'[': // begins-with-list
        if(printingSteps) { wcerr << "begins-with-list" << endl; }
      {
        wstring list = popString();
        wstring needle = popString();
        set<wstring, Ltstr>::iterator it, limit;

        if(rule[i+1] == '#')
        {
          i++;
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
      case L']': // ends-with-list
        if(printingSteps) { wcerr << "ends-with-list" << endl; }
      {
        wstring list = popString();
        wstring needle = popString();
        set<wstring, Ltstr>::iterator it, limit;

        if(rule[i+1] == '#')
        {
          i++;
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
      case L'c': // contains-substring
        if(printingSteps) { wcerr << "contains-substring" << endl; }
      {
        wstring needle = popString();
        wstring haystack = popString();
        if(rule[i+1] == '#')
        {
          i++;
          needle = StringUtils::tolower(needle);
          haystack = StringUtils::tolower(haystack);
        }
        pushStack(haystack.find(needle) != wstring::npos);
      }
        break;
      case L'n': // in
        if(printingSteps) { wcerr << "in" << endl; }
      {
        wstring list = popString();
        wstring str = popString();
        if(rule[i+1] == '#')
        {
          i++;
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
      case L'4': // let (var, end)
        if(printingSteps) { wcerr << "let var" << endl; }
      {
        wstring var = popString();
        wstring val = popString();
        if(i+1 < rule.size() && rule[i+1] == L'#')
        {
          i++;
          val = copycase(val, variables[var]);
        }
        variables[var] = val;
      }
        break;
      case L'<': // out
        if(printingSteps) { wcerr << "out" << endl; }
      {
        int count = rule[++i];
        currentOutput.resize(currentOutput.size()+count);
        for(unsigned int j = 1; j <= count; j++)
        {
          currentOutput[currentOutput.size()-j] = popChunk();
        }
      }
        break;
      case L'S': // clip side="sl"
        if(printingSteps) { wcerr << "sl clip" << endl; }
      {
        wstring part = popString();
        int pos = 2*(rule[++i]-1);
        pushStack(currentInput[pos]->chunkPart(attr_items[part], L"sl"));
      }
        break;
      case L'T': // clip side="tl"
        if(printingSteps) { wcerr << "tl clip" << endl; }
      {
        wstring part = popString();
        int pos = 2*(rule[++i]-1);
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
          if(printingSteps) { wcerr << " -> " << theStack.top().s << endl; }
        }
      }
        break;
      case L'R': // clip side="ref"
        if(printingSteps) { wcerr << "ref clip" << endl; }
      {
        wstring part = popString();
        int pos = 2*(rule[++i]-1);
        pushStack(currentInput[pos]->chunkPart(attr_items[part], L"ref"));
      }
        break;
      case L't': // let clip side="tl"
        if(printingSteps) { wcerr << "let clip" << endl; }
      {
        wstring part = popString();
        int pos = 2*(rule[++i]-1);
        currentInput[pos]->setChunkPart(attr_items[part], popString());
      }
        break;
      case L'$': // var
        if(printingSteps) { wcerr << "var" << endl; }
        if(in_let_setup)
        {
          in_let_setup = false;
        }
        else
        {
          pushStack(variables[popString()]);
        }
        break;
      case L'G': // get-case-from, case-of
        if(printingSteps) { wcerr << "get-case-from or case-of" << endl; }
        pushStack(caseOf(popString()));
        break;
      case L'A': // copy-case
        if(printingSteps) { wcerr << "copy case" << endl; }
      {
        wstring src = popString();
        wstring dest = popString();
        pushStack(copycase(src, dest));
      }
        break;
      case L'+': // concat
        if(printingSteps) { wcerr << "concat" << endl; }
      {
        int count = rule[++i];
        wstring result;
        for(unsigned int j = 0; j < count; j++)
        {
          result.append(popString());
        }
        pushStack(result);
      }
        break;
      case L'{': // chunk
        if(printingSteps) { wcerr << "chunk" << endl; }
      {
        Chunk* ch = new Chunk();
        ch->isBlank = false;
        int count = rule[++i];
        for(unsigned int j = 0; j < count; j++)
        {
          StackElement c = popStack();
          if(c.mode == 2)
          {
            ch->target = c.s + ch->target;
          }
          else if(c.mode == 3)
          {
            if(c.c->target.size() == 0)
            {
              ch->contents.insert(ch->contents.begin(),
                                  c.c->contents.begin(), c.c->contents.end());
            }
            else
            {
              ch->contents.insert(ch->contents.begin(), c.c);
            }
          }
          else
          {
            wcerr << L"Unable to add to chunk StackElement with mode " << c.mode << endl;
            exit(1);
          }
        }
        if(printingSteps) { wcerr << L" pushing chunk with target surface " << ch->target << endl; }
        pushStack(ch);
      }
        break;
      case L'p': // pseudolemma
        if(printingSteps) { wcerr << "pseudolemma" << endl; }
        pushStack(popChunk()->target);
        break;
      case L' ': // b
        if(printingSteps) { wcerr << "b" << endl; }
        pushStack(new Chunk(wstring(L" ")));
        break;
      case L'_': // b
        if(printingSteps) { wcerr << "b pos" << endl; }
      {
        int loc = 2*(rule[++i]-1) + 1;
        pushStack(currentInput[loc]);
      }
        break;
      default:
        wcerr << "unknown instruction: " << rule[i] << endl;
        exit(1);
    }
  }
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
    int val = ms.classifyFinals(me->getFinals());
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
    parseTower[layer].erase(parseTower[layer].begin(), parseTower[layer].begin()+len);
    applyRule(rule_map[rule]);
    parseTower[layer+1].insert(parseTower[layer+1].end(), currentOutput.begin(), currentOutput.end());
    currentInput.clear();
    currentOutput.clear();
    if(layer+2 == parseTower.size())
    {
      shiftCount = 0;
    }
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
