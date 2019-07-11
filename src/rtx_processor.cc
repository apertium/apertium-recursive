#include <rtx_processor.h>
#include <bytecode.h>
#include <apertium/trx_reader.h>
#include <apertium/utf_converter.h>
#include <lttoolbox/compression.h>

#include <iostream>
#include <apertium/string_utils.h>
//#include <apertium/unlocked_cstdio.h>

using namespace Apertium;
using namespace std;

RTXProcessor::RTXProcessor()
{
  furtherInput = true;
  inword = false;
  allDone = false;
  printingSteps = false;
  printingRules = false;
  printingMatch = false;
  noCoref = false;
  isLinear = false;
  null_flush = false;
  internal_null_flush = false;
  printingTrees = false;
}

RTXProcessor::~RTXProcessor()
{
  delete mx;
}

void
RTXProcessor::readData(FILE *in)
{
  alphabet.read(in);

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

  mx = new MatchExe2(*t, &alphabet, finals, pat_size);

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

  delete t;
}

void
RTXProcessor::read(string const &transferfile, string const &datafile)
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
  count = fgetwc(in);
  for(int i = 0; i < count; i++)
  {
    cur.clear();
    len = getwc(in);
    cur.reserve(len);
    for(int j = 0; j < len; j++)
    {
      cur.append(1, fgetwc(in));
    }
    output_rules.push_back(cur);
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
RTXProcessor::beginsWith(wstring const &s1, wstring const &s2) const
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
RTXProcessor::endsWith(wstring const &s1, wstring const &s2) const
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
RTXProcessor::copycase(wstring const &source_word, wstring const &target_word)
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
RTXProcessor::caseOf(wstring const &s)
{
  return copycase(s, wstring(L"aa"));
}

inline bool
RTXProcessor::popBool()
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
RTXProcessor::popInt()
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
RTXProcessor::popString()
{
  if(theStack[stackIdx].mode == 2)
  {
    return theStack[stackIdx--].s;
  }
  else if(theStack[stackIdx].mode == 3)
  {
    return theStack[stackIdx--].c->target;
  }
  else
  {
    wcerr << "tried to pop wstring but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline Chunk*
RTXProcessor::popChunk()
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
RTXProcessor::stackCopy(int src, int dest)
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
RTXProcessor::applyRule(const wstring& rule)
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
        if(printingSteps) { wcerr << "int " << (int)rule[i+1] << endl; }
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
        if(printingSteps) { wcerr << "output" << endl; }
      {
        Chunk* ch = popChunk();
        ch->source.clear();
        if(isLinear && ch->contents.size() == 0)
        {
          bool word = true;
          unsigned int last = 0;
          wchar_t* targ = ch->target.data();
          bool chunk = false;
          for(unsigned int c = 0, limit = ch->target.size(); c < limit; c++)
          {
            if(targ[c] == L'\\') c++;
            else if((targ[c] == L'{' || targ[c] == L'$') && word)
            {
              if(targ[c] == L'{') chunk = true;
              Chunk* temp = chunkPool.next();
              temp->isBlank = false;
              temp->target = ch->target.substr(last, c-last);
              if(chunk) currentOutput.back()->contents.push_back(temp);
              else currentOutput.push_back(temp);
              last = c+1;
              word = false;
            }
            else if((targ[c] == L'^' || targ[c] == L'}') && !word)
            {
              if(c > last)
              {
                Chunk* temp = chunkPool.next();
                temp->isBlank = true;
                temp->target = ch->target.substr(last, c-last);
                if(chunk) currentOutput.back()->contents.push_back(temp);
                else currentOutput.push_back(temp);
              }
              if(targ[c] == L'}') chunk = false;
              last = c+1;
              word = true;
            }
          }
          if(last == 0 && ch->target.size() != 0)
          {
            currentOutput.push_back(ch);
          }
          else if(last < ch->target.size())
          {
            Chunk* temp = chunkPool.next();
            temp->isBlank = true;
            temp->target = ch->target.substr(last);
            currentOutput.push_back(temp);
          }
        }
        else
        {
          currentOutput.push_back(ch);
        }
      }
        break;
      case OUTPUTALL:
        if(printingSteps) { wcerr << "outputall" << endl; }
        currentOutput = currentInput;
        return true;
        break;
      case SOURCECLIP:
        if(printingSteps) { wcerr << "sourceclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        Chunk* ch = (pos == -2) ? parentChunk : currentInput[pos];
        pushStack(ch->chunkPart(attr_items[part], SourceClip));
      }
        break;
      case TARGETCLIP:
        if(printingSteps) { wcerr << "targetclip" << endl; }
      {
        int loc = popInt();
        int pos = 2*(loc-1);
        wstring part = popString();
        Chunk* ch = NULL;
        if(pos == -2) ch = parentChunk;
        else if(0 <= pos && pos < currentInput.size()) ch = currentInput[pos];
        else
        {
          int n = 0;
          for(unsigned int x = 0; x < currentInput.size(); x++)
          {
            if(!currentInput[x]->isBlank) n++;
            if(n == loc)
            {
              ch = currentInput[x];
              break;
            }
          }
          if(ch == NULL)
          {
            //wcerr << L"Clip index is out of bounds." << endl;
            //exit(EXIT_FAILURE);
            ch = currentInput.back();
          }
        }
        if(part == L"whole" || part == L"chcontent")
        {
          pushStack(ch);
        }
        else
        {
          pushStack(ch->chunkPart(attr_items[part], TargetClip));
          if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
        }
      }
        break;
      case REFERENCECLIP:
        if(printingSteps) { wcerr << "referenceclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        Chunk* ch = (pos == -2) ? parentChunk : currentInput[pos];
        pushStack(ch->chunkPart(attr_items[part], ReferenceClip));
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
        result = popString() + result;
        pushStack(result);
      }
        break;
      case CHUNK:
        if(printingSteps) { wcerr << "chunk" << endl; }
      {
        Chunk* ch = chunkPool.next();
        ch->isBlank = false;
        pushStack(ch);
      }
        break;
      case APPENDCHILD:
        if(printingSteps) { wcerr << "appendchild" << endl; }
      {
        Chunk* kid = popChunk();
        theStack[stackIdx].c->contents.push_back(kid);
        if(printingSteps) { wcerr << " -> child with surface '" << kid->target << L"' appended" << endl; }
      }
        break;
      case APPENDSURFACE:
        if(printingSteps) { wcerr << "appendsurface" << endl; }
      {
        wstring s = popString();
        theStack[stackIdx].c->target += s;
        if(printingSteps) { wcerr << " -> " << s << endl; }
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
      case APPENDALLINPUT:
        if(printingSteps) { wcerr << "appendallinput" << endl; }
        theStack[stackIdx].c->contents = currentInput;
        break;
      case BLANK:
        if(printingSteps) { wcerr << "blank" << endl; }
      {
        int loc = 2*(popInt()-1) + 1;
        if(loc == -1)
        {
          Chunk* ch = chunkPool.next();
          ch->target = L" ";
          ch->isBlank = true;
          pushStack(ch);
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
      case GETRULE:
        if(printingSteps) { wcerr << "getrule" << endl; }
      {
        int pos = 2*(popInt()-1);
        pushStack(currentInput[pos]->rule);
      }
        break;
      case SETRULE:
        if(printingSteps) { wcerr << "setrule" << endl; }
      {
        int pos = 2*(popInt()-1);
        int rl = popInt();
        if(pos == -2)
        {
          theStack[stackIdx].c->rule = rl;
        }
        else
        {
          currentInput[pos]->rule = rl;
        }
      }
        break;
      case LUCOUNT:
        if(printingSteps) { wcerr << "lucount" << endl; }
        pushStack(to_wstring((currentInput.size() + 1) / 2));
        break;
      default:
        wcerr << "unknown instruction: " << rule[i] << endl;
        exit(1);
    }
  }
  return true;
}

Chunk *
RTXProcessor::readToken(FILE *in)
{
  int pos = 0;
  wstring cur;
  wstring src;
  wstring dest;
  wstring coref;
  cur.reserve(256);
  bool inSquare = false;
  while(true)
  {
    int val = fgetwc_unlocked(in);
    if(feof(in) || (internal_null_flush && val == 0))
    {
      furtherInput = false;
      Chunk* ret = chunkPool.next();
      ret->target = cur;
      ret->isBlank = true;
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
      inSquare = true;
    }
    else if(inSquare)
    {
      cur += val;
      if(val == L']')
      {
        inSquare = false;
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
        Chunk* ret = chunkPool.next();
        ret->source = src;
        ret->target = dest;
        ret->coref = coref;
        ret->isBlank = false;
        return ret;
      }
    }
    else if(!inword && val == L'^')
    {
      inword = true;
      Chunk* ret = chunkPool.next();
      ret->target = cur;
      ret->isBlank = true;
      return ret;
    }
    else
    {
      cur += wchar_t(val);
    }
  }
}

bool
RTXProcessor::getNullFlush(void)
{
  return null_flush;
}

void
RTXProcessor::setNullFlush(bool null_flush)
{
  this->null_flush = null_flush;
}

void
RTXProcessor::setTrace(bool trace)
{
  this->trace = trace;
}

void
RTXProcessor::checkForReduce(vector<ParseNode*>& result, ParseNode* node)
{
  mx->resetRejected();
  int rule = node->getRule();
  double weight = node->weight;
  while(rule != -1)
  {
    int len = pat_size[rule-1];
    int first;
    int last = node->lastWord;
    currentInput.resize(len);
    node->getChunks(currentInput, len-1);
    currentOutput.clear();
    if(printingRules) {
      wcerr << "Applying rule " << rule << ": ";
      for(unsigned int i = 0; i < currentInput.size(); i++)
      {
        currentInput[i]->writeTree();
      }
      wcerr << endl;
    }
    if(applyRule(rule_map[rule-1]))
    {
      vector<Chunk*> temp;
      temp.reserve(currentOutput.size());
      while(currentOutput.size() > 1)
      {
        temp.push_back(currentOutput.back());
        currentOutput.pop_back();
      }
      ParseNode* back = node->popNodes(len);
      ParseNode* cur;
      if(back == NULL)
      {
        first = 0;
        cur = parsePool.next();
        cur->init(mx, currentOutput[0], weight + ruleWeights[rule-1]);
      }
      else
      {
        first = back->lastWord+1;
        cur = parsePool.next();
        cur->init(back, currentOutput[0], weight + ruleWeights[rule-1]);
      }
      if(temp.size() == 0)
      {
        checkForReduce(result, cur);
        break;
      }
      vector<ParseNode*> res;
      vector<ParseNode*> res2;
      checkForReduce(res, cur);
      while(temp.size() > 0)
      {
        for(vector<ParseNode*>::iterator it = res.begin(), limit = res.end();
              it != limit; it++)
        {
          cur = parsePool.next();
          cur->init(*it, temp.back());
          cur->firstWord = first;
          cur->lastWord = last;
          checkForReduce(res2, cur);
        }
        temp.pop_back();
        res.clear();
        res.swap(res2);
      }
      result.reserve(result.size() + res.size());
      for(vector<ParseNode*>::iterator it = res.begin(), limit = res.end();
            it != limit; it++)
      {
        result.push_back(*it);
      }
      break;
    }
    else
    {
      mx->rejectRule(rule);
      rule = node->getRule();
    }
  }
  if(rule == -1 || node->shouldShift())
  {
    result.push_back(node);
  }
}

void
RTXProcessor::outputAll(FILE* out)
{
  while(outputQueue.size() > 0)
  {
    Chunk* ch = outputQueue.front();
    outputQueue.pop_front();
    if(printingTrees)
    {
      ch->writeTree(out);
      continue;
    }
    if(ch->rule == -1)
    {
      if(printingRules) { wcerr << "No rule specified" << endl; }
      ch->output(out);
    }
    else
    {
      parentChunk = ch;
      vector<wstring> tags = ch->getTags(vector<wstring>());
      currentInput = ch->contents;
      for(unsigned int i = 0; i < currentInput.size(); i++)
      {
        currentInput[i]->updateTags(tags);
      }
      currentOutput.clear();
      if(printingRules) {
        wcerr << "Applying output rule " << ch->rule << ": ";
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->writeTree();
        }
        wcerr << endl;
      }
      applyRule(output_rules[ch->rule]);
      for(vector<Chunk*>::reverse_iterator it = currentOutput.rbegin(),
              limit = currentOutput.rend(); it != limit; it++)
      {
        outputQueue.push_front(*it);
      }
    }
  }
}

bool
RTXProcessor::filterParseGraph()
{
  bool shouldOutput = !furtherInput;
  int state[parseGraph.size()];
  const int N = parseGraph.size();
  memset(state, 1, N*sizeof(int));
  int count = N;
  if(furtherInput)
  {
    for(int i = 0; i < N; i++)
    {
      if(parseGraph[i]->isDone())
      {
        state[i] = 0;
        count--;
      }
    }
    if(count == 0)
    {
      shouldOutput = true;
      memset(state, 1, N*sizeof(int));
      count = N;
    }
  }
  int min = -1;
  ParseNode* minNode = NULL;
  ParseNode* cur = NULL;
  map<int, vector<int>> filter;
  //wcerr << L"shouldOutput: " << shouldOutput << L" branch count: " << N << endl;
  for(int i = 0; i < N; i++)
  {
    //wcerr << "examining node " << i << " ... ";
    if(state[i] == 0) continue;
    if(min == -1)
    {
      //wcerr << "FIRST!" << endl;
      min = i;
      minNode = parseGraph[i];
      cur = minNode;
      filter[cur->firstWord].push_back(i);
    }
    else
    {
      cur = parseGraph[i];
      if(shouldOutput)
      {
        if(cur->length < minNode->length
            || (cur->length == minNode->length && cur->weight > minNode->weight))
        {
          //wcerr << i << L" beats " << min << " in length or weight" << endl;
          state[min] = 0;
          min = i;
          minNode = cur;
        }
        else
        {
          state[i] = 0;
          //wcerr << min << L" beats " << i << " in length or weight" << endl;
        }
        count--;
      }
      else if(filter.find(cur->firstWord) == filter.end())
      {
        filter[cur->firstWord].push_back(i);
        //wcerr << i << " has nothing to compare with" << endl;
      }
      else
      {
        vector<int>& other = filter[cur->firstWord];
        double w = parseGraph[other[0]]->weight;
        if(w > cur->weight)
        {
          //wcerr << i << L" has lower weight - discarding." << endl;
          state[i] = 0;
          count--;
        }
        else if(w < cur->weight)
        {
          //wcerr << i << L" has higher weight - discarding others." << endl;
          for(vector<int>::iterator it = other.begin(), limit = other.end();
                it != limit; it++)
          {
            state[*it] = 0;
            count--;
          }
          other.resize(1);
          other[0] = i;
        }
        else
        {
          //wcerr << i << " has same weight - keeping all." << endl;
          other.push_back(i);
        }
      }
    }
  }
  vector<ParseNode*> temp;
  temp.reserve(count);
  for(int i = 0; i < N; i++)
  {
    if(state[i] != 0)
    {
      temp.push_back(parseGraph[i]);
      //wcerr << L"keeping branch " << i << " first word: " << parseGraph[i]->firstWord << " ending with ";
      //parseGraph[i]->chunk->writeTree();
      //wcerr << endl;
    }
    /*else
    {
      wcerr << L"discarding branch " << i << " first word: " << parseGraph[i]->firstWord << " ending with ";
      parseGraph[i]->chunk->writeTree();
      wcerr << endl;
    }*/
  }
  //wcerr << L"remaining branches: " << temp.size() << endl << endl;
  parseGraph.swap(temp);
  return shouldOutput;
}

void
RTXProcessor::processGLR(FILE *in, FILE *out)
{
  Chunk* next = readToken(in);
  while(true)
  {
    if(parseGraph.size() == 0)
    {
      ParseNode* temp = parsePool.next();
      temp->init(mx, next);
      checkForReduce(parseGraph, temp);
    }
    else
    {
      mx->prepareChunk(next->source);
      vector<ParseNode*> temp;
      for(unsigned int i = 0, limit = parseGraph.size(); i < limit; i++)
      {
        ParseNode* tempNode = parsePool.next();
        tempNode->init(parseGraph[i], next, true);
        checkForReduce(temp, tempNode);
      }
      parseGraph.swap(temp);
    }
    next = readToken(in);
    if(filterParseGraph())
    {
      parseGraph[0]->getChunks(outputQueue, parseGraph[0]->length-1);
      parseGraph.clear();
      //min->getChunks(outputQueue, min->length-1);
      outputAll(out);
      wstring s = next->source;
      wstring t = next->target;
      wstring r = next->coref;
      bool b = next->isBlank;
      chunkPool.reset();
      parsePool.reset();
      next = chunkPool.next();
      next->source = s;
      next->target = t;
      next->coref = r;
      next->isBlank = b;
    }
    if(!furtherInput)
    {
      // if stream is empty, next is definitely a blank
      next->output(out);
      break;
    }
  }
}

void
RTXProcessor::processTRXLayer(list<Chunk*>& t1x, list<Chunk*>& t2x)
{
  if(t1x.size() == 0)
  {
    return;
  }
  int state[1024];
  int first = 0;
  int last = 0;
  if(!furtherInput || t1x.size() >= longestPattern)
  {
    mx->resetRejected();
    int len = 0;
    int rule = -1;
    int i = 0;
  try_again_for_reject_rule:
    first = 0;
    last = 1;
    state[0] = mx->getInitial();
    for(list<Chunk*>::iterator it = t1x.begin(), limit = t1x.end();
          it != limit && i < longestPattern; it++)
    {
      i++;
      if((*it)->isBlank)
      {
        if(printingMatch) { wcerr << "  matching blank" << endl; }
        mx->matchBlank(state, first, last);
      }
      else
      {
        if(printingMatch) { wcerr << "  matching chunk " << (*it)->matchSurface() << endl; }
        mx->matchChunk(state, first, last, (*it)->matchSurface(), false);
        int r = mx->getRule(state, first, last);
        if(r != -1)
        {
          rule = r;
          len = i;
        }
      }
      if(first == last) break;
    }
    if(rule == -1)
    {
      t2x.push_back(t1x.front());
      if(!t2x.back()->isBlank && t2x.back()->target.size() == 0)
      {
        t2x.pop_back();
        if(t2x.size() > 0 && t1x.size() > 0)
        {
          t2x.back()->target += t1x.front()->target;
          t1x.pop_front();
        }
      }
      t1x.pop_front();
    }
    else
    {
      i = 0;
      currentInput.resize(len);
      for(list<Chunk*>::iterator it = t1x.begin(), limit = t1x.end();
            it != limit && i < len; it++)
      {
        currentInput[i] = *it;
        i++;
      }
      currentOutput.clear();
      if(printingRules) {
        wcerr << "Applying rule " << rule << ": ";
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->writeTree();
        }
        wcerr << endl;
      }
      if(applyRule(rule_map[rule-1]))
      {
        for(unsigned int n = 0; n < currentOutput.size(); n++)
        {
          t2x.push_back(currentOutput[n]);
        }
        for(int n = 0; n < len; n++)
        {
          t1x.pop_front();
        }
      }
      else
      {
        goto try_again_for_reject_rule;
      }
    }
  }
}

void
RTXProcessor::processTRX(FILE *in, FILE *out)
{
  list<Chunk*> t1x;
  list<Chunk*> t2x;
  list<Chunk*> t3x;
  while(furtherInput || t1x.size() > 0 || t2x.size() > 0)
  {
    while(furtherInput && t1x.size() < 2*longestPattern)
    {
      t1x.push_back(readToken(in));
    }
    if(furtherInput)
    {
      processTRXLayer(t1x, t2x);
      processTRXLayer(t2x, t3x);
    }
    else
    {
      while(t1x.size() > 0)
      {
        processTRXLayer(t1x, t2x);
      }
      while(t2x.size() > 0)
      {
        processTRXLayer(t2x, t3x);
      }
    }
    while(t3x.size() > 0)
    {
      Chunk* cur = t3x.front();
      t3x.pop_front();
      vector<wstring> tags = cur->getTags(vector<wstring>());
      if(cur->rule == -1)
      {
        if(cur->contents.size() == 0) cur->output(out);
        else
        {
          while(cur->contents.size() > 0)
          {
            t3x.push_front(cur->contents.back());
            t3x.front()->updateTags(tags);
            cur->contents.pop_back();
          }
        }
      }
      else
      {
        if(printingRules) {
          wcerr << L"Applying output rule " << cur->rule << ": ";
          cur->writeTree();
          wcerr << endl;
        }
        parentChunk = cur;
        currentInput = cur->contents;
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->updateTags(tags);
        }
        currentOutput.clear();
        applyRule(output_rules[cur->rule]);
        for(unsigned int i = 0; i < currentOutput.size(); i++)
        {
          currentOutput[i]->output(out);
        }
      }
    }
  }
}

void
RTXProcessor::process(FILE* in, FILE* out)
{
  output = out;
  if(null_flush)
  {
    null_flush = false;
    internal_null_flush = true;

    while(!feof(in))
    {
      furtherInput = true;
      if(isLinear)
      {
        processTRX(in, out);
      }
      else
      {
        processGLR(in, out);
      }
      fputwc_unlocked(L'\0', out);
      int code = fflush(out);
      if(code != 0)
      {
        wcerr << L"Could not flush output " << errno << endl;
      }
      chunkPool.reset();
      parsePool.reset();
    }
    internal_null_flush = false;
    null_flush = true;
  }
  else if(isLinear)
  {
    processTRX(in, out);
  }
  else
  {
    processGLR(in, out);
  }
}
