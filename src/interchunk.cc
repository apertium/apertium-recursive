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
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));
    attr_items[cad_k].read(in);
    wstring fallback = Compression::wstring_read(in);
    if(recompile_attrs) {
      attr_items[cad_k].compile(UtfConverter::toUtf8(fallback));
    }
  }

  // variables
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));
    variables[cad_k] = UtfConverter::toUtf8(Compression::wstring_read(in));
  }

  // macros
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));
    macros[cad_k] = Compression::multibyte_read(in);
  }

  // lists
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    string const cad_k = UtfConverter::toUtf8(Compression::wstring_read(in));

    for(int j = 0, limit2 = Compression::multibyte_read(in); j != limit2; j++)
    {
      wstring const cad_v = Compression::wstring_read(in);
      lists[cad_k].insert(UtfConverter::toUtf8(cad_v));
      listslow[cad_k].insert(UtfConverter::toUtf8(StringUtils::tolower(cad_v)));
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
  rules.resize(count);
  int len;
  string cur;
  for(int i = 0; i < count; i++)
  {
    cur = "";
    fgetc(in);
    len = fgetc(in);
    for(int j = 0; j < len; j++)
    {
      cur.append(1, fgetc(in));
    }
    rules.push_back(cur);
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
Interchunk::beginsWith(string const &s1, string const &s2) const
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
Interchunk::endsWith(string const &s1, string const &s2) const
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

string
Interchunk::tolower(string const &str) const
{
  return UtfConverter::toUtf8(StringUtils::tolower(UtfConverter::fromUtf8(str)));
}

StackElement
Interchunk::popStack()
{
  StackElement ret = theStack.top();
  theStack.pop();
  return ret;
}

void
Interchunk::processInstruction(string rule)
{
  for(int i = 0; i < rule.size(); i++)
  {
    switch(rule[i])
    {
      case 's': // string
      {
        int ct = rule[++i];
        string blob;
        for(int j = 0; j < ct; j++)
        {
          blob.append(1, rule[++i]);
        }
        pushStack(blob);
      }
        break;
      case '~': // choose
        pushStack(i + rule[++i]);
        pushStack(false);
        break;
      case 'i': // when
        if(popStack().b)
        {
          i = popStack().i;
        }
        else
        {
          pushStack(rule[++i]);
        }
        break;
      case 'e': // otherwise
        if(popStack().b)
        {
          i = popStack().i; // jump instruction
        }
        else
        {
          popStack(); // jump instruction
        }
        break;
      case '?': // test
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
      case '&': // and
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
      case '|': // or
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
      case '!': // not
        theStack.top().b = !theStack.top().b;
        break;
      case '=': // equal
      {
        string a = popStack().s;
        string b = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          a = tolower(a);
          b = tolower(b);
        }
        pushStack(a == b);
      }
        break;
      case '(': // begins-with
      {
        string substr = popStack().s;
        string str = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          pushStack(beginsWith(tolower(str), tolower(substr)));
        }
        else
        {
          pushStack(beginsWith(str, substr));
        }
      }
        break;
      case ')': // ends-with
      {
        string substr = popStack().s;
        string str = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          pushStack(endsWith(tolower(str), tolower(substr)));
        }
        else
        {
          pushStack(endsWith(str, substr));
        }
      }
        break;
      case '[': // begins-with-list
      {
        string list = popStack().s;
        string needle = popStack().s;
        set<string, Ltstr>::iterator it, limit;

        if(rule[i+1] == '#')
        {
          i++;
          it = lists[list].begin();
          limit = lists[list].end();
        }
        else
        {
          needle = tolower(needle);
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
      case ']': // ends-with-list
      {
        string list = popStack().s;
        string needle = popStack().s;
        set<string, Ltstr>::iterator it, limit;

        if(rule[i+1] == '#')
        {
          i++;
          it = lists[list].begin();
          limit = lists[list].end();
        }
        else
        {
          needle = tolower(needle);
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
      case 'c': // contains-substring
      {
        string needle = popStack().s;
        string haystack = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          needle = tolower(needle);
          haystack = tolower(haystack);
        }
        pushStack(haystack.find(needle) != string::npos);
      }
        break;
      case 'n': // in
      {
        string list = popStack().s;
        string str = popStack().s;
        if(rule[i+1] == '#')
        {
          i++;
          str = tolower(str);
          set<string, Ltstr> &myset = listslow[list];
          pushStack(myset.find(str) != myset.end());
        }
        else
        {
          set<string, Ltstr> &myset = lists[list];
          pushStack(myset.find(str) != myset.end());
        }
      }
        break;
      case '>': // let (begin)
        pushStack(" LET ");
        break;
      case '*': // let (clip, end)
      case '4': // let (var, end)
      // TODO: append
      case '<': // out
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
      case '.': // clip
      {
        string part = popStack().s;
        int pos = rule[++i];
        if(theStack.top().s == " LET ")
        {
          popStack();
          pushStack(pair<int, string>(pos, part));
        }
        else
        {
          // TODO: get value
        }
      }
        break;
      case '$': // var
      {
        string name = theStack.top().s;
        popStack();
        if(theStack.top().s == " LET ")
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
      case '{':
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
      case ' ': // b
      {
        Chunk* ch = new Chunk();
        ch->surface = " ";
        pushStack(ch);
      }
        break;
      case '_': // b
        // TODO: get value
        break;
      default:
        cout << "unknown instruction: " << rule[i] << endl;
    }
  }
}
