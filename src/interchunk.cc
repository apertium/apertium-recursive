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
  int count = fgetc(in);
  longestPattern = fgetc(in);
  rule_map.resize(count);
  int len;
  wstring cur;
  for(int i = 0; i < count; i++)
  {
    cur.clear();
    fgetc(in);
    len = fgetc(in);
    for(int j = 0; j < len; j++)
    {
      cur.append(1, fgetc(in));
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

StackElement
Interchunk::popStack()
{
  StackElement ret = theStack.top();
  theStack.pop();
  return ret;
}

void
Interchunk::processInstruction(wstring rule)
{
  for(int i = 0; i < rule.size(); i++)
  {
    switch(rule[i])
    {
      case L's': // string
      {
        int ct = rule[++i];
        wstring blob;
        for(int j = 0; j < ct; j++)
        {
          blob.append(1, rule[++i]);
        }
        pushStack(blob);
      }
        break;
      case L'~': // choose
        pushStack(i + rule[++i]);
        pushStack(false);
        break;
      case L'i': // when
        if(popStack().b)
        {
          i = popStack().i;
        }
        else
        {
          pushStack(rule[++i]);
        }
        break;
      case L'e': // otherwise
        if(popStack().b)
        {
          i = popStack().i; // jump instruction
        }
        else
        {
          popStack(); // jump instruction
        }
        break;
      case L'?': // test
        if(popStack().b)
        {
          popStack(); // jump instruction
          popStack(); // false
          pushStack(true);
        }
        else
        {
          i = popStack().i;
        }
        break;
      case L'&': // and
      {
        int count = rule[++i];
        bool val = true;
        for(int j = 0; j < count; j++)
        {
          val = val && popStack().b;
        }
        pushStack(val);
      }
        break;
      case L'|': // or
      {
        int count = rule[++i];
        bool val = false;
        for(int j = 0; j < count; j++)
        {
          val = val || popStack().b;
        }
        pushStack(val);
      }
        break;
      case L'!': // not
        theStack.top().b = !theStack.top().b;
        break;
      case L'=': // equal
      {
        wstring a = popStack().s;
        wstring b = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          a = StringUtils::tolower(a);
          b = StringUtils::tolower(b);
        }
        pushStack(a == b);
      }
        break;
      case L'(': // begins-with
      {
        wstring substr = popStack().s;
        wstring str = popStack().s;
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
      {
        wstring substr = popStack().s;
        wstring str = popStack().s;
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
      {
        wstring list = popStack().s;
        wstring needle = popStack().s;
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
      {
        wstring list = popStack().s;
        wstring needle = popStack().s;
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
      {
        wstring needle = popStack().s;
        wstring haystack = popStack().s;
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
      {
        wstring list = popStack().s;
        wstring str = popStack().s;
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
        pushStack(L" LET ");
        break;
      case L'*': // let (clip, end)
      case L'4': // let (var, end)
      // TODO: append
      case L'<': // out
      {
        int count = rule[++i];
        vector<Chunk*> nodes;
        nodes.resize(count);
        for(int j = 0; j < count; j++)
        {
          nodes.push_back(popStack().c);
        }
        for(int j = 0; j < count; j++)
        {
          currentOutput.push_back(nodes.back());
          nodes.pop_back();
        }
      }
        break;
      // TODO: modify-case
      case L'.': // clip
      {
        wstring part = popStack().s;
        int pos = rule[++i];
        if(theStack.top().s == L" LET ")
        {
          popStack();
          pushStack(pair<int, wstring>(pos, part));
        }
        else
        {
          // TODO: get value
        }
      }
        break;
      case L'$': // var
      {
        wstring name = theStack.top().s;
        popStack();
        if(theStack.top().s == L" LET ")
        {
          popStack();
          pushStack(name);
        }
        else
        {
          // TODO: get value
        }
      }
        break;
      // TODO: get-case-from
      // TODO: case-of
      // TODO: concat
      case L'{':
      {
        Chunk* ch = new Chunk();
        int count = rule[++i];
        vector<Chunk*> temp;
        for(int j = 0; j < count; j++)
        {
          temp.push_back(popStack().c);
        }
        for(int j = count-1; j >= 0; j--)
        {
          ch->addPiece(temp[j]);
        }
        pushStack(ch);
      }
        break;
      // TODO: pseudolemma
      case L' ': // b
      {
        Chunk* ch = new Chunk(L" ");
        pushStack(ch);
      }
        break;
      case L'_': // b
        // TODO: get value
        break;
      default:
        cout << "unknown instruction: " << rule[i] << endl;
    }
  }
}

Chunk &
Interchunk::readToken(FILE *in)
{
  if(!input_buffer.isEmpty())
  {
    return input_buffer.next();
  }

  wstring content;
  wstring data;
  while(true)
  {
    int val = fgetwc_unlocked(in);
    if(feof(in) || (internal_null_flush && val == 0))
    {
      furtherInput = false;
      return input_buffer.add(Chunk(content));
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
          int val3 = char(fgetwc_unlocked(in));
          ungetwc(val3, in);

          if(val3 == L'$')
          {
            break;
          }
        }
        else
        {
          content += wchar_t(val2);
        }
      }
    }
    else if(inword && val == L'$')
    {
      inword = false;
      return input_buffer.add(Chunk(content, data));
    }
    else if(val == L'^')
    {
      inword = true;
      return input_buffer.add(Chunk(content));
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
Interchunk::applyWord(Chunk* word)
{
  wstring word_str = word->surface;
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
  if(getNullFlush())
  {
    interchunk_wrapper_null_flush(in, out);
  }
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
Interchunk::interchunk_linear(FILE *in, FILE *out)
{
/*  int last = 0;

  output = out;
  ms.init(me->getInitial());

  while(true)
  {
    if(ms.size() == 0)
    {
      if(lastrule != NULL)
      {
        applyRule();
        input_buffer.setPos(last);
      }
      else
      {
        if(tmpword.size() != 0)
        {
          fputwc_unlocked(L'^', output);
          fputws_unlocked(tmpword[0]->c_str(), output);
          fputwc_unlocked(L'$', output);
          tmpword.clear();
          input_buffer.setPos(last);
          input_buffer.next();
          last = input_buffer.getPos();
          ms.init(me->getInitial());
        }
        else if(tmpblank.size() != 0)
        {
          fputws_unlocked(tmpblank[0]->c_str(), output);
          tmpblank.clear();
          last = input_buffer.getPos();
          ms.init(me->getInitial());
        }
      }
    }
    int val = ms.classifyFinals(me->getFinals());
    if(val != -1)
    {
      lastrule = rule_map[val-1];
      last = input_buffer.getPos();

      if(trace)
      {
        wcerr << endl << L"apertium-interchunk: Rule " << val << L" ";
        for (unsigned int ind = 0; ind < tmpword.size(); ind++)
        {
          if (ind != 0)
          {
            wcerr << L" ";
          }
          fputws_unlocked(tmpword[ind]->c_str(), stderr);
        }
        wcerr << endl;
      }
    }

    TransferToken &current = readToken(in);

    switch(current.getType())
    {
      case tt_word:
        applyWord(current.getContent());
        tmpword.push_back(&current.getContent());
        break;

      case tt_blank:
        ms.step(L' ');
        tmpblank.push_back(&current.getContent());
        break;

      case tt_eof:
        if(tmpword.size() != 0)
        {
          tmpblank.push_back(&current.getContent());
          ms.clear();
        }
        else
        {
          fputws_unlocked(current.getContent().c_str(), output);
          tmpblank.clear();
          return;
        }
        break;

      default:
        wcerr << "Error: Unknown input token." << endl;
        return;
    }
  }*/
}

void
Interchunk::interchunk_recursive(FILE *in, FILE *out)
{
  
}
