#include <interchunk.h>
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
  recursing = false;
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
    fgetwc(in);
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
  //wstring ret (L"aa");
  //return copycase(s, ret);
  return copycase(s, wstring(L"aa"));
  /*if(s.size() > 1)
  {
    if(!iswupper(s[0]))
    {
      return L"aa";
    }
    else if(!iswupper(s[s.size()-1]))
    {
      return L"Aa";
    }
    else
    {
      return L"AA";
    }
  }
  else if(s.size() == 1)
  {
    if(!iswupper(s[0]))
    {
      return L"aa";
    }
    else
    {
      return L"Aa";
    }
  }
  else
  {
    return L"aa";
  }*/
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

pair<int, wstring>
Interchunk::popClip()
{
  StackElement ret = popStack();
  if(ret.mode == 4)
  {
    return ret.clip;
  }
  else
  {
    wcerr << "tried to pop clip but mode is " << ret.mode << endl;
    exit(1);
  }
}

void
Interchunk::applyRule(wstring rule)
{
  bool in_let_setup = false;
  for(int i = 0; i < rule.size(); i++)
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
          a = _a.c->surface;
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
          b = _b.c->surface;
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
      case L'>': // let (begin)
        if(printingSteps) { wcerr << "let" << endl; }
        in_let_setup = true;
        break;
      case L'*': // let (clip, end)
        if(printingSteps) { wcerr << "let clip" << endl; }
      {
        wstring val = popString();
        pair<int, wstring> clip = popClip();
        if(i+1 < rule.size() && rule[i+1] == L'#')
        {
          i++;
          string temp = currentInput[2*(clip.first-1)]->chunkPart(attr_items[clip.second]);
          val = copycase(val, UtfConverter::fromUtf8(temp));
        }
        currentInput[2*(clip.first-1)]->setChunkPart(attr_items[clip.second], val);
      }
        break;
      case L'4': // let (var, end)
        if(printingSteps) { wcerr << "let var" << endl; }
      {
        wstring val = popString();
        wstring var = popString();
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
        for(int j = 1; j <= count; j++)
        {
          currentOutput[currentOutput.size()-j] = popChunk();
        }
      }
        break;
      case L'.': // clip
        if(printingSteps) { wcerr << "clip" << endl; }
      {
        wstring part = popString();
        int pos = rule[++i];
        if(in_let_setup)
        {
          pushStack(pair<int, wstring>(pos, part));
          in_let_setup = false;
        }
        else
        {
          string v = currentInput[2*(pos-1)]->chunkPart(attr_items[part]);
          pushStack(UtfConverter::fromUtf8(v));
          if(printingSteps) { wcerr << " " << pos << " -> " << theStack.top().s << endl; }
        }
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
        for(int j = 0; j < count; j++)
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
        int count = rule[++i];
        if(recursing)
        {
          vector<Chunk*> temp;
          for(int j = 0; j < count; j++)
          {
            temp.push_back(popChunk());
          }
          for(int j = 0; j < count; j++)
          {
            ch->addPiece(temp.back());
            temp.pop_back();
          }
        }
        else
        {
          for(int j = 0; j < count; j++)
          {
            StackElement c = popStack();
            if(c.mode == 2)
            {
              ch->surface = c.s + ch->surface;
            }
            else if(c.mode == 3)
            {
              ch->surface = c.c->surface + ch->surface;
            }
            else
            {
              wcerr << L"Unable to add to chunk StackElement with mode " << c.mode << endl;
              exit(1);
            }
          }
        }
        pushStack(ch);
      }
        break;
      case L'p': // pseudolemma
        if(printingSteps) { wcerr << "pseudolemma" << endl; }
        pushStack(popChunk()->surface);
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
  wstring content;
  wstring data;
  while(true)
  {
    int val = fgetwc_unlocked(in);
    if(feof(in) || (internal_null_flush && val == 0))
    {
      furtherInput = false;
      Chunk* ret = new Chunk(content);
      return ret;
    }
    if(val == L'\\')
    {
      content += L'\\';
      content += wchar_t(fgetwc_unlocked(in));
    }
    else if(val == L'[')
    {
      content += L'[';
      while(true)
      {
        int val2 = fgetwc_unlocked(in);
        if(val2 == L'\\')
        {
          content += L'\\';
          content += wchar_t(fgetwc_unlocked(in));
        }
        else if(val2 == L']')
        {
          content += L']';
          break;
        }
        else
        {
          content += wchar_t(val2);
        }
      }
    }
    else if(inword && val == L'{')
    {
      data += wchar_t(val);
      while(true)
      {
        int val2 = fgetwc_unlocked(in);
        if(val2 == L'\\')
        {
          data += L'\\';
          data += wchar_t(fgetwc_unlocked(in));
        }
        else if(val2 == L'}')
        {
          data += wchar_t(val2);
          int val3 = char(fgetwc_unlocked(in));
          ungetwc(val3, in);

          if(val3 == L'$')
          {
            break;
          }
        }
        else
        {
          data += wchar_t(val2);
        }
      }
    }
    else if(inword && val == L'$')
    {
      inword = false;
      Chunk* ret = new Chunk(content, data);
      return ret;
    }
    else if(val == L'^')
    {
      inword = true;
      Chunk* ret = new Chunk(content);
      return ret;
    }
    else
    {
      content += wchar_t(val);
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
  if(word.isBlank())
  {
    ms.step(L' ');
    return;
  }
  wstring word_str = word.surface;
  ms.step(L'^');
  for(unsigned int i = 0, limit = word_str.size(); i < limit; i++)
  {
    switch(word_str[i])
    {
      case L'\\':
        i++;
        ms.step(towlower(word_str[i]), any_char);
        break;

      case L'<':
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
        break;

      case L'{':  // ignore the unmodifiable part of the chunk
        ms.step(L'$');
        return;

      default:
        ms.step(towlower(word_str[i]), any_char);
        break;
    }
  }
  ms.step(L'$');
}

void
Interchunk::interchunk(FILE *in, FILE *out)
{
  /*if(getNullFlush())
  {
    interchunk_wrapper_null_flush(in, out);
  }*/
  if(recursing)
  {
    interchunk_recursive(in, out);
  }
  else
  {
    interchunk_linear(in, out);
  }
}

void
Interchunk::interchunk_do_pass()
{
  int minLayer = longestPattern;
  if(!furtherInput)
  {
    minLayer = 0;
    for(size_t i = 0; i < parseTower.size(); i++)
    {
      if(parseTower[i].size() > minLayer)
      {
        minLayer = parseTower[i].size();
      }
    }
    if(minLayer > longestPattern)
    {
      minLayer = longestPattern;
    }
    if(minLayer == 0)
    {
      allDone = true;
      return;
    }
  }
  int layer = -1;
  bool shouldshift = false;
  for(size_t l = parseTower.size()-1; l >= 0; l--)
  {
    if(parseTower[l].size() >= minLayer)
    {
      layer = l;
      break;
    }
  }
  if(layer <= -1)
  {
    shouldshift = true;
    if(furtherInput)
    {
      return;
    }
    for(size_t l = 0; l < parseTower.size(); l++)
    {
      if(parseTower[l].size() > 0)
      {
        layer = l;
        break;
      }
    }
    if(layer == -1)
    {
      allDone = true;
      return;
    }
  }
  if(layer+1 == parseTower.size())
  {
    parseTower.push_back(vector<Chunk*>());
  }
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
  }
  else if(shouldshift || parseTower[layer].size() >= minLayer)
  {
    parseTower[layer+1].push_back(parseTower[layer][0]);
    parseTower[layer].erase(parseTower[layer].begin());
  }
}

void
Interchunk::interchunk_linear(FILE *in, FILE *out)
{
  parseTower.push_back(vector<Chunk*>());
  parseTower.push_back(vector<Chunk*>());
  while(!allDone)
  {
    if(furtherInput)
    {
      Chunk* ch = readToken(in);
      parseTower[0].push_back(ch);
    }
    interchunk_do_pass();
    for(int i = 0; i < parseTower[1].size(); i++)
    {
      parseTower[1][i]->output(out);
      //delete parseTower[1][i];
    }
    parseTower[1].clear();
  }
}

void
Interchunk::interchunk_recursive(FILE *in, FILE *out)
{
  
}
