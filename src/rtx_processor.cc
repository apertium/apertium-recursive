#include <rtx_config.h>
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
  printingSteps = false;
  printingRules = false;
  printingBranches = false;
  printingAll = false;
  noCoref = true;
  isLinear = false;
  null_flush = false;
  printingTrees = false;
  printingText = true;
  treePrintMode = TreeModeNest;
  newBranchId = 0;
  noFilter = true;
  currentBranch = NULL;
}

RTXProcessor::~RTXProcessor()
{
  delete mx;
}

void
RTXProcessor::read(string const &filename)
{
  FILE *in = fopen(filename.c_str(), "rb");
  if(in == NULL)
  {
    wcerr << "Unable to open file " << filename.c_str() << endl;
    exit(EXIT_FAILURE);
  }

  longestPattern = 2*Compression::multibyte_read(in) - 1;
  int count = Compression::multibyte_read(in);
  pat_size.reserve(count);
  rule_map.reserve(count);
  for(int i = 0; i < count; i++)
  {
    pat_size.push_back(Compression::multibyte_read(in));
    rule_map.push_back(Compression::wstring_read(in));
  }
  count = Compression::multibyte_read(in);
  output_rules.reserve(count);
  for(int i = 0; i < count; i++)
  {
    output_rules.push_back(Compression::wstring_read(in));
  }

  varCount = Compression::multibyte_read(in);

  alphabet.read(in);

  Transducer* t = new Transducer();
  t->read(in, alphabet.size()); 

  multimap<int, pair<int, double>> finals;

  map<int, double> finalWeights = t->getFinals();

  // finals
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    int key = Compression::multibyte_read(in);
    int rl = Compression::multibyte_read(in);
    double wgt = Compression::long_multibyte_read(in);
    finals.insert(make_pair(key, make_pair(rl, wgt)));
  }

  mx = new MatchExe2(*t, &alphabet, finals, pat_size);

  delete t;

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

  int nameCount = Compression::multibyte_read(in);
  for(int i = 0; i < nameCount; i++)
  {
    inRuleNames.push_back(Compression::wstring_read(in));
  }
  nameCount = Compression::multibyte_read(in);
  for(int i = 0; i < nameCount; i++)
  {
    outRuleNames.push_back(Compression::wstring_read(in));
  }

  fclose(in);
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
    if(isLinear)
    {
      result = target_word;
      result[0] = towlower(result[0]);
    }
    else result = StringUtils::tolower(target_word);
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

inline void
RTXProcessor::popString(wstring& dest)
{
  if(theStack[stackIdx].mode == 2)
  {
    theStack[stackIdx--].s.swap(dest);
  }
  else if(theStack[stackIdx].mode == 3)
  {
    dest.assign(theStack[stackIdx--].c->target);
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
  vector<bool> editted = vector<bool>(currentInput.size(), false);
  const wchar_t* rule_data = rule.data();
  for(unsigned int i = 0, rule_size = rule.size(); i < rule_size; i++)
  {
    switch(rule_data[i])
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
        int ct = rule_data[++i];
        stackIdx++;
        theStack[stackIdx].mode = 2;
        theStack[stackIdx].s.assign(rule, i+1, ct);
        //pushStack(rule.substr(i+1, ct));
        i += ct;
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case INT:
        if(printingSteps) { wcerr << "int " << (int)rule[i+1] << endl; }
        pushStack((int)rule_data[++i]);
        break;
      case PUSHFALSE:
        if(printingSteps) { wcerr << "pushfalse" << endl; }
        pushStack(false);
        break;
      case PUSHTRUE:
        if(printingSteps) { wcerr << "pushtrue" << endl; }
        pushStack(true);
        break;
      case PUSHNULL:
        if(printingSteps) { wcerr << "pushnull" << endl; }
        pushStack((Chunk*)NULL);
        break;
      case JUMP:
        if(printingSteps) { wcerr << "jump" << endl; }
        ++i;
        i += rule_data[i];
        break;
      case JUMPONTRUE:
        if(printingSteps) { wcerr << "jumpontrue" << endl; }
        if(!popBool())
        {
          i++;
          if(printingSteps) { wcerr << " -> false" << endl; }
        }
        else
        {
          ++i;
          i += rule_data[i];
          if(printingSteps) { wcerr << " -> true, jumping" << endl; }
        }
        break;
      case JUMPONFALSE:
        if(printingSteps) { wcerr << "jumponfalse" << endl; }
        if(popBool())
        {
          i++;
          if(printingSteps) { wcerr << " -> true" << endl; }
        }
        else
        {
          ++i;
          i += rule_data[i];
          if(printingSteps) { wcerr << " -> false, jumping" << endl; }
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
        popString(a);
        wstring b;
        popString(b);
        if(rule_data[i] == EQUALCL)
        {
          a = StringUtils::tolower(a);
          b = StringUtils::tolower(b);
        }
        pushStack(a == b);
        if(printingSteps) { wcerr << " -> " << (a == b ? "true" : "false") << endl; }
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
        currentBranch->stringVars[var] = val;
        if(printingSteps) { wcerr << " -> " << var << " = '" << val << "'" << endl; }
      }
        break;
      case OUTPUT:
        if(printingSteps) { wcerr << "output" << endl; }
      {
        Chunk* ch = popChunk();
        if(ch == NULL) break; // FETCHCHUNK
        if(isLinear && ch->contents.size() == 0)
        {
          bool word = true;
          unsigned int last = 0;
          const wchar_t* targ = ch->target.data();
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
      case PUSHINPUT:
        if(printingSteps) { wcerr << "pushinput" << endl; }
      {
        int loc = popInt();
        int pos = 2*(loc-1);
        Chunk* ch = NULL;
        if(pos == -2) ch = parentChunk;
        else if(0 <= pos && pos < (int)currentInput.size()) ch = currentInput[pos];
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
        pushStack(ch);
      }
        break;
      case SOURCECLIP:
        if(printingSteps) { wcerr << "sourceclip" << endl; }
      {
        wstring part;
        popString(part);
        Chunk* ch = popChunk();
        if(ch == NULL) pushStack(L"");
        else pushStack(ch->chunkPart(attr_items[part], SourceClip));
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case TARGETCLIP:
        if(printingSteps) { wcerr << "targetclip" << endl; }
      {
        wstring part;
        popString(part);
        Chunk* ch = popChunk();
        if(ch == NULL) pushStack(L"");
        else pushStack(ch->chunkPart(attr_items[part], TargetClip));
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case REFERENCECLIP:
        if(printingSteps) { wcerr << "referenceclip" << endl; }
      {
        wstring part;
        popString(part);
        Chunk* ch = popChunk();
        if(ch == NULL) pushStack(L"");
        else pushStack(ch->chunkPart(attr_items[part], ReferenceClip));
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case SETCLIP:
        if(printingSteps) { wcerr << "setclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        wstring part = popString();
        if(pos >= 0)
        {
          if(!editted[pos])
          {
            currentInput[pos] = currentInput[pos]->copy();
            editted[pos] = true;
          }
          currentInput[pos]->setChunkPart(attr_items[part], popString());
          if(printingSteps) { wcerr << " -> " << currentInput[pos]->target << endl; }
        }
        else
        {
          theStack[stackIdx].c->setChunkPart(attr_items[part], popString());
        }
      }
        break;
      case FETCHVAR:
        if(printingSteps) { wcerr << "fetchvar" << endl; }
        {
          wstring name = popString();
          wstring val = currentBranch->stringVars[name];
          pushStack(val);
          if(printingSteps) { wcerr << " -> " << name << " = " << val << endl; }
        }
        break;
      case FETCHCHUNK:
        if(printingSteps) { wcerr << "fetchchunk" << endl; }
        pushStack(currentBranch->chunkVars[popInt()]);
        break;
      case SETCHUNK:
        if(printingSteps) { wcerr << "setchunk" << endl; }
        {
          int pos = popInt();
          currentBranch->chunkVars[pos] = popChunk();
        }
        break;
      case GETCASE:
        if(printingSteps) { wcerr << "getcase" << endl; }
        pushStack(caseOf(popString()));
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
        break;
      case SETCASE:
        if(printingSteps) { wcerr << "setcase" << endl; }
      {
        wstring src = popString();
        wstring dest = popString();
        pushStack(copycase(src, dest));
      }
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx].s << endl; }
        break;
      case CONCAT:
        if(printingSteps) { wcerr << "concat" << endl; }
      {
        if(theStack[stackIdx].mode != 2 || theStack[stackIdx-1].mode != 2)
        {
          wcerr << L"Cannot CONCAT non-strings." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        theStack[stackIdx].s.append(theStack[stackIdx+1].s);
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
        if(isLinear && kid->target[0] == L'^')
        {
          unsigned int j = 0;
          for(; j < kid->target.size(); j++)
          {
            if(kid->target[j] == L'$') break;
          }
          Chunk* ch = chunkPool.next();
          ch->isBlank = false;
          ch->target = kid->target.substr(1, j-1);
          theStack[stackIdx].c->contents.push_back(ch);
          ch = chunkPool.next();
          ch->isBlank = true;
          ch->target = kid->target.substr(j+1);
          theStack[stackIdx].c->contents.push_back(ch);
        }
        else
        {
          theStack[stackIdx].c->contents.push_back(kid);
        }
        if(printingSteps) { wcerr << " -> child with surface '" << kid->target << L"' appended" << endl; }
      }
        break;
      case APPENDSURFACE:
        if(printingSteps) { wcerr << "appendsurface" << endl; }
      {
        if(theStack[stackIdx].mode != 2 && theStack[stackIdx].mode != 3)
        {
          wcerr << L"Cannot append non-string to chunk surface." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        if(theStack[stackIdx].mode != 3)
        {
          wcerr << L"Cannot APPENDSURFACE to non-chunk." << endl;
          exit(EXIT_FAILURE);
        }
        if(theStack[stackIdx+1].mode == 2)
        {
          theStack[stackIdx].c->target += theStack[stackIdx+1].s;
        }
        else
        {
          theStack[stackIdx].c->target += theStack[stackIdx+1].c->target;
        }
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx+1].s << endl; }
      }
        break;
      case APPENDSURFACESL:
        if(printingSteps) { wcerr << "appendsurfacesl" << endl; }
      {
        if(theStack[stackIdx].mode != 2 && theStack[stackIdx].mode != 3)
        {
          wcerr << L"Cannot append non-string to chunk surface." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        if(theStack[stackIdx].mode != 3)
        {
          wcerr << L"Cannot APPENDSURFACESL to non-chunk." << endl;
          exit(EXIT_FAILURE);
        }
        if(theStack[stackIdx+1].mode == 2)
        {
          theStack[stackIdx].c->source += theStack[stackIdx+1].s;
        }
        else
        {
          theStack[stackIdx].c->source += theStack[stackIdx+1].c->source;
        }
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx+1].s << endl; }
      }
        break;
      case APPENDSURFACEREF:
        if(printingSteps) { wcerr << "appendsurfaceref" << endl; }
      {
        if(theStack[stackIdx].mode != 2 && theStack[stackIdx].mode != 3)
        {
          wcerr << L"Cannot append non-string to chunk surface." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        if(theStack[stackIdx].mode != 3)
        {
          wcerr << L"Cannot APPENDSURFACEREF to non-chunk." << endl;
          exit(EXIT_FAILURE);
        }
        if(theStack[stackIdx+1].mode == 2)
        {
          theStack[stackIdx].c->coref += theStack[stackIdx+1].s;
        }
        else
        {
          theStack[stackIdx].c->coref += theStack[stackIdx+1].c->coref;
        }
        if(printingSteps) { wcerr << " -> " << theStack[stackIdx+1].s << endl; }
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
      {
        vector<Chunk*>& vec = theStack[stackIdx].c->contents;
        vec.insert(vec.end(), currentInput.begin(), currentInput.end());
      }
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
      case CONJOIN:
        if(printingSteps) { wcerr << "conjoin" << endl; }
      {
        Chunk* join = chunkPool.next();
        join->isBlank = true;
        join->isJoiner = true;
        join->target = L"+";
        pushStack(join);
      }
        break;
      case REJECTRULE:
        if(printingSteps) { wcerr << "rejectrule" << endl; }
        return false;
        break;
      case DISTAG:
        if(printingSteps) { wcerr << "distag" << endl; }
      {
        if(theStack[stackIdx].mode != 2)
        {
          wcerr << L"Cannot DISTAG non-string." << endl;
          exit(EXIT_FAILURE);
        }
        wstring& s = theStack[stackIdx].s;
        if(s.size() > 0 && s[0] == L'<' && s[s.size()-1] == L'>')
        {
          s = StringUtils::substitute(s.substr(1, s.size()-2), L"><", L".");
        }
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
    if(feof(in) || (null_flush && val == 0))
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
    else if(val == L'[' && !inword)
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
        if(src.size() > 0 && src[0] == L'*' && dest.size() > 0 && dest[0] == L'*')
        {
          Chunk* ret2 = chunkPool.next();
          ret2->target = ret->target.substr(1) + L"<UNKNOWN:INTERNAL>";
          ret2->contents.push_back(ret);
          ret2->rule = -1;
          ret2->isBlank = false;
          return ret2;
        }
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

bool
RTXProcessor::setOutputMode(string mode)
{
  if(mode == "flat")
  {
    treePrintMode = TreeModeFlat;
    return true;
  }
  else if(mode == "nest")
  {
    treePrintMode = TreeModeNest;
    return true;
  }
  else if(mode == "latex")
  {
    treePrintMode = TreeModeLatex;
    return true;
  }
  else if(mode == "dot")
  {
    treePrintMode = TreeModeDot;
    return true;
  }
  else if(mode == "box")
  {
    treePrintMode = TreeModeBox;
    return true;
  }
  else
  {
    return false;
  }
}

bool
RTXProcessor::lookahead(ParseNode* node)
{
  Chunk* next = NULL;
  for(vector<vector<Chunk*>*>::reverse_iterator it = currentContinuation.rbegin(),
        limit = currentContinuation.rend(); it != limit; it++)
  {
    for(vector<Chunk*>::reverse_iterator it2 = (*it)->rbegin(), limit2 = (*it)->rend();
        it2 != limit2; it++)
    {
      if(!(*it2)->isBlank)
      {
        next = *it2;
        break;
      }
    }
    if(next != NULL) break;
  }
  if(next == NULL)
  {
    for(list<Chunk*>::iterator it = inputBuffer.begin(), limit = inputBuffer.end();
        it != limit; it++)
    {
      if(!(*it)->isBlank)
      {
        next = *it;
        break;
      }
    }
  }
  return (next != NULL && node->shouldShift(next));
}

void
RTXProcessor::checkForReduce(vector<ParseNode*>& result, ParseNode* node)
{
  if(printingAll) wcerr << "Checking for reductions for branch " << node->id << endl;
  mx->resetRejected();
  pair<int, double> rule_and_weight = node->getRule();
  int rule = rule_and_weight.first;
  double weight = node->weight + rule_and_weight.second;
  currentBranch = node;
  while(rule != -1)
  {
    int len = pat_size[rule-1];
    int first;
    int last = node->lastWord;
    currentInput.resize(len);
    node->getChunks(currentInput, len-1);
    currentOutput.clear();
    if(printingRules || printingAll) {
      if(printingAll && treePrintMode == TreeModeLatex) wcerr << "\\subsection{";
      else wcerr << endl;
      wcerr << "Applying rule " << rule;
      if(rule <= (int)inRuleNames.size())
      {
        wcerr << " (" << inRuleNames[rule-1] << ")";
      }
      if(printingAll) wcerr << " to branch " << node->id << " with weight " << rule_and_weight.second;
      if(printingAll && treePrintMode == TreeModeLatex) wcerr << "}" << endl << endl;
      else wcerr << ": ";
      for(unsigned int i = 0; i < currentInput.size(); i++)
      {
        currentInput[i]->writeTree((printingAll ? treePrintMode : TreeModeFlat), NULL);
      }
      wcerr << endl;
    }
    if(applyRule(rule_map[rule-1]))
    {
      if(printingAll)
      {
        for(auto c : currentOutput) c->writeTree(treePrintMode, NULL);
        wcerr << endl;
      }
      vector<Chunk*> temp;
      temp.reserve(currentOutput.size());
      while(currentOutput.size() > 1)
      {
        temp.push_back(currentOutput.back());
        currentOutput.pop_back();
      }
      ParseNode* back = node->popNodes(len);
      ParseNode* cur = parsePool.next();
      if(back == NULL)
      {
        first = 0;
        cur->init(mx, currentOutput[0], weight);
      }
      else
      {
        first = back->lastWord+1;
        cur->init(back, currentOutput[0], weight);
      }
      cur->stringVars = node->stringVars;
      cur->chunkVars = node->chunkVars;
      cur->id = node->id;
      if(temp.size() == 0)
      {
        checkForReduce(result, cur);
        break;
      }
      currentContinuation.push_back(&temp);
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
          cur->stringVars = (*it)->stringVars;
          cur->chunkVars = (*it)->chunkVars;
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
      currentContinuation.pop_back();
      break;
    }
    else
    {
      if(printingRules) { wcerr << " -> rule was rejected" << endl; }
      if(printingAll) wcerr << "This rule was rejeced." << endl << endl;
      mx->rejectRule(rule);
      rule_and_weight = node->getRule();
      rule = rule_and_weight.first;
      weight = node->weight + rule_and_weight.second;
    }
  }
  if(rule == -1)
  {
    if(printingAll) wcerr << "No further reductions possible for branch " << node->id << "." << endl;
    result.push_back(node);
  }
  else if(lookahead(node))
  {
    node->id = ++newBranchId;
    if(printingAll) wcerr << endl << "Splitting stack and creating branch " << node->id << endl;
    result.push_back(node);
  }
}

void
RTXProcessor::outputAll(FILE* out)
{
  unsigned int queueSize = outputQueue.size() - 1;
  bool conjoining = false;
  Chunk* tojoin = NULL;
  while(outputQueue.size() > 0)
  {
    Chunk* ch = outputQueue.front();
    outputQueue.pop_front();
    if(printingTrees && outputQueue.size() == queueSize)
    {
      if(printingText) fputc_unlocked('\n', out);
      queueSize--;
      ch->writeTree(treePrintMode, out);
      fflush(out);
      if(!printingText) continue;
    }
    if(ch->rule == -1)
    {
      if(printingRules)// && !ch->isBlank)
      {
        fflush(out);
        wcerr << endl << "No rule specified: ";
        ch->writeTree(TreeModeFlat, NULL);
        wcerr << endl;
      }
      if(printingAll && !ch->isBlank)
      {
        if(treePrintMode == TreeModeLatex) wcerr << "\\subsubsection{Output Node}" << endl;
        else wcerr << "Output Node:" << endl;
        ch->writeTree(treePrintMode, NULL);
        wcerr << endl;
      }
      if(ch->contents.size() > 0)
      {
        vector<wstring> tags = ch->getTags(vector<wstring>());
        for(auto it = ch->contents.rbegin(); it != ch->contents.rend(); it++)
        {
          (*it)->updateTags(tags);
          outputQueue.push_front(*it);
        }
      }
      else if(conjoining && !ch->isBlank)
      {
        tojoin->conjoin(ch);
      }
      else if(ch->isJoiner)
      {
        if(tojoin != NULL) conjoining = true;
      }
      else
      {
        conjoining = false;
        if(tojoin != NULL)
        {
          tojoin->output(out);
          tojoin = NULL;
        }
        if(ch->isBlank) ch->output(out);
        else tojoin = ch;
      }
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
        fflush(out);
        wcerr << endl << "Applying output rule " << ch->rule;
        if(ch->rule < (int)outRuleNames.size())
        {
          wcerr << " (" << outRuleNames[ch->rule] << ")";
        }
        wcerr << ": " << parentChunk->target << " -> ";
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->writeTree(TreeModeFlat, NULL);
        }
        wcerr << endl;
      }
      if(printingAll)
      {
        if(treePrintMode == TreeModeLatex)
        {
          wcerr << "\\subsubsection{Applying Output Rule " << ch->rule;
          if(ch->rule < (int)outRuleNames.size())
          {
            wcerr << ": " << outRuleNames[ch->rule] << "}" << endl << endl;
          }
        }
        else
        {
          wcerr << "Applying Output Rule " << ch->rule;
          if(ch->rule < (int)outRuleNames.size())
          {
            wcerr << ": " << outRuleNames[ch->rule] << endl << endl;
          }
        }
        ch->writeTree(treePrintMode, NULL);
      }
      fflush(out);
      applyRule(output_rules[ch->rule]);
      for(vector<Chunk*>::reverse_iterator it = currentOutput.rbegin(),
              limit = currentOutput.rend(); it != limit; it++)
      {
        outputQueue.push_front(*it);
      }
    }
  }
  if(tojoin != NULL) tojoin->output(out);
}

bool
RTXProcessor::filterParseGraph()
{
  if(printingAll)
  {
    if(treePrintMode == TreeModeLatex)
    {
      wcerr << "\\subsection{Filtering Branches}\n\n\\begin{itemize}" << endl;
    }
    else wcerr << endl << "Filtering Branches:" << endl;
  }
  bool shouldOutput = !furtherInput && inputBuffer.size() == 1;
  int state[parseGraph.size()];
  const int N = parseGraph.size();
  memset(state, 1, N*sizeof(int));
  int count = N;
  if(furtherInput || inputBuffer.size() > 1)
  {
    for(int i = 0; i < N; i++)
    {
      if(parseGraph[i]->isDone() ||
         (!parseGraph[i]->chunk->isBlank && !lookahead(parseGraph[i])))
      {
        state[i] = 0;
        count--;
      }
    }
    if(count == 0)
    {
      if(printingAll)
      {
        if(treePrintMode == TreeModeLatex)
        {
          wcerr << L"\\item No branch can accept further input." << endl;
        }
        else wcerr << L"No branch can accept further input." << endl;
      }
      shouldOutput = true;
      memset(state, 1, N*sizeof(int));
      count = N;
    }
  }
  else if(printingAll)
  {
    if(treePrintMode == TreeModeLatex)
    {
      wcerr << "\\item Input buffer is empty." << endl;
    }
    else wcerr << L"Input buffer is empty." << endl;
  }
  int min = -1;
  ParseNode* minNode = NULL;
  ParseNode* cur = NULL;
  map<int, vector<int>> filter;
  if(printingBranches) { wcerr << L"shouldOutput: " << shouldOutput << L" branch count: " << N << endl; }
  for(int i = 0; i < N; i++)
  {
    if(printingBranches) { wcerr << "examining node " << i << "(length: " << parseGraph[i]->length << ", weight: " << parseGraph[i]->weight << ") ... "; }
    if(printingAll)
    {
      if(treePrintMode == TreeModeLatex) wcerr << "\\item ";
      wcerr << "Branch " << parseGraph[i]->id << " ";
    }
    if(state[i] == 0)
    {
      if(printingAll) wcerr << " has no possible continuations." << endl;
      continue;
    }
    else if(noFilter && !shouldOutput) continue;
    if(min == -1)
    {
      if(printingAll) wcerr << " has no active branch to compare to." << endl;
      if(printingBranches) { wcerr << "FIRST!" << endl; }
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
            || (cur->length == minNode->length && cur->weight >= minNode->weight))
        {
          if(printingBranches) { wcerr << i << L" beats " << min << " in length or weight" << endl; }
          if(printingAll) wcerr << " has fewer partial parses or a higher weight than branch " << minNode->id << "." << endl;
          state[min] = 0;
          min = i;
          minNode = cur;
        }
        else
        {
          state[i] = 0;
          if(printingBranches) {wcerr << min << L" beats " << i << " in length or weight" << endl; }
          if(printingAll) wcerr << " has more partial parses or a lower weight than branch " << minNode->id << "." << endl;
        }
        count--;
      }
      else if(filter.find(cur->firstWord) == filter.end())
      {
        filter[cur->firstWord].push_back(i);
        if(printingBranches) { wcerr << i << " has nothing to compare with" << endl; }
        if(printingAll) wcerr << " has no prior branch covering the same final span." << endl;
      }
      else
      {
        vector<int>& other = filter[cur->firstWord];
        double w = parseGraph[other[0]]->weight;
        if(w > cur->weight)
        {
          if(printingBranches) { wcerr << i << L" has lower weight - discarding." << endl; }
          if(printingAll) wcerr << " has a lower weight than branch " << parseGraph[other[0]]->id << " and will be discarded." << endl;
          state[i] = 0;
          count--;
        }
        else if(w < cur->weight)
        {
          if(printingBranches) { wcerr << i << L" has higher weight - discarding others." << endl; }
          if(printingAll)
          {
            wcerr << " has a higher weight than ";
            for(auto it : other) wcerr << "branch " << parseGraph[it]->id << ", ";
            wcerr << "which will be discarded." << endl;
          }
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
          if(printingBranches) { wcerr << i << " has same weight - keeping all." << endl; }
          if(printingAll) wcerr << " has the same weight as branch " << parseGraph[other[0]]->id << "." << endl;
          other.push_back(i);
        }
      }
    }
  }
  if(printingAll && treePrintMode == TreeModeLatex) wcerr << "\\end{itemize}" << endl << endl;
  if(count == N) return shouldOutput;
  if(count > 100 && filter.size() > 0)
  {
    int maxbin = 100 / filter.size();
    for(auto& it : filter)
    {
      for(unsigned int i = maxbin; i < it.second.size(); i++)
      {
        state[it.second[i]] = 0;
        count--;
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
      if(printingBranches)
      {
        wcerr << L"keeping branch " << i << " first word: " << parseGraph[i]->firstWord << " ending with ";
        parseGraph[i]->chunk->writeTree(TreeModeFlat, NULL);
        wcerr << endl;
      }
    }
    else if(printingBranches)
    {
      wcerr << L"discarding branch " << i << " first word: " << parseGraph[i]->firstWord << " ending with ";
      parseGraph[i]->chunk->writeTree(TreeModeFlat, NULL);
      wcerr << endl;
    }
  }
  if(printingBranches) { wcerr << L"remaining branches: " << temp.size() << endl << endl; }
  parseGraph.swap(temp);
  return shouldOutput;
}

void
RTXProcessor::processGLR(FILE *in, FILE *out)
{
  int sentenceId = 1;
  if(printingAll && treePrintMode == TreeModeLatex)
  {
    wcerr << "\\section{Sentence " << sentenceId << "}" << endl << endl;
  }
  while(furtherInput && inputBuffer.size() < 5)
  {
    inputBuffer.push_back(readToken(in));
  }
  bool real_printingAll = printingAll;
  while(true)
  {
    Chunk* next = inputBuffer.front();
    if(next->isBlank) printingAll = false;
    if(printingAll)
    {
      wcerr << endl;
      if(treePrintMode == TreeModeLatex) wcerr << "\\subsection{Reading Input}" << endl << endl;
      else wcerr << "Reading Input:" << endl;
      next->writeTree(treePrintMode, NULL);
      wcerr << endl;
    }
    inputBuffer.pop_front();
    if(parseGraph.size() == 0)
    {
      ParseNode* temp = parsePool.next();
      temp->init(mx, next);
      temp->id = ++newBranchId;
      temp->stringVars = variables;
      temp->chunkVars = vector<Chunk*>(varCount, NULL);
      checkForReduce(parseGraph, temp);
    }
    else
    {
      mx->prepareChunk(next->source.size() > 0 ? next->source : next->target);
      // conditional deals with unknowns
      vector<ParseNode*> temp;
      for(unsigned int i = 0, limit = parseGraph.size(); i < limit; i++)
      {
        ParseNode* tempNode = parsePool.next();
        tempNode->init(parseGraph[i], next, true);
        tempNode->id = parseGraph[i]->id;
        tempNode->stringVars = parseGraph[i]->stringVars;
        tempNode->chunkVars = parseGraph[i]->chunkVars;
        checkForReduce(temp, tempNode);
      }
      parseGraph.swap(temp);
    }
    if(printingAll && treePrintMode != TreeModeLatex)
    {
      for(auto branch : parseGraph)
      {
        wcerr << "Branch " << branch->id << ": " << branch->length << " nodes, weight = " << branch->weight << endl;
        vector<Chunk*> parts;
        parts.resize(branch->length);
        branch->getChunks(parts, branch->length-1);
        for(auto node : parts)
        {
          if(node->isBlank) wcerr << "[Blank]: " << endl;
          else wcerr << "[Chunk]: " << endl;
          node->writeTree(treePrintMode, NULL);
        }
      }
    }
    if(furtherInput) inputBuffer.push_back(readToken(in));
    if(filterParseGraph())
    {
      wcerr.flush();
      if(printingAll)
      {
        if(treePrintMode == TreeModeLatex) wcerr << "\\subsection{Outputting Branch " << parseGraph[0]->id << "}" << endl << endl;
        else
        {
          wcerr << endl;
          wcerr << "************************************************************" << endl;
          wcerr << "************************************************************" << endl;
          wcerr << "************************************************************" << endl;
          wcerr << "Outputting Branch " << parseGraph[0]->id << endl << endl;
          vector<Chunk*> parts;
          parts.resize(parseGraph[0]->length);
          parseGraph[0]->getChunks(parts, parseGraph[0]->length-1);
          for(auto node : parts)
          {
            if(node->isBlank) wcerr << "[Blank]: " << endl;
            else wcerr << "[Chunk]: " << endl;
            node->writeTree(treePrintMode, NULL);
          }
          wcerr << "************************************************************" << endl;
          wcerr << "************************************************************" << endl;
          wcerr << "************************************************************" << endl;
          wcerr << endl;
        }
      }
      currentBranch = parseGraph[0];
      parseGraph[0]->getChunks(outputQueue, parseGraph[0]->length-1);
      parseGraph.clear();
      outputAll(out);
      variables = currentBranch->stringVars;
      fflush(out);
      vector<wstring> sources;
      vector<wstring> targets;
      vector<wstring> corefs;
      vector<bool> blanks;
      vector<bool> unknowns;
      int N = inputBuffer.size();
      for(int i = 0; i < N; i++)
      {
        Chunk* temp = inputBuffer.front();
        if(temp->contents.size() > 0)
        {
          unknowns.push_back(true);
          temp = temp->contents[0];
        }
        else
        {
          unknowns.push_back(false);
        }
        sources.push_back(temp->source);
        targets.push_back(temp->target);
        corefs.push_back(temp->coref);
        blanks.push_back(temp->isBlank);
        inputBuffer.pop_front();
      }
      //wcerr << "clearing chunkPool, size was " << chunkPool.size() << endl;
      //wcerr << "clearing parsePool, size was " << parsePool.size() << endl;
      chunkPool.reset();
      parsePool.reset();
      newBranchId = 0;
      if(printingAll) sentenceId++;
      if((furtherInput || inputBuffer.size() > 1) && printingAll && treePrintMode == TreeModeLatex)
      {
        wcerr << endl << endl << "\\section{Sentence " << sentenceId << "}" << endl << endl;
      }
      for(int i = 0; i < N; i++)
      {
        Chunk* c = chunkPool.next();
        c->source = sources[i];
        c->target = targets[i];
        c->coref = corefs[i];
        c->isBlank = blanks[i];
        if(unknowns[i])
        {
          Chunk* c2 = chunkPool.next();
          c2->target = targets[i].substr(1) + L"<UNKNOWN:INTERNAL>";
          c2->contents.push_back(c);
          c = c2;
        }
        inputBuffer.push_back(c);
      }
    }
    printingAll = real_printingAll;
    if(!furtherInput && inputBuffer.size() == 1)
    {
      // if stream is empty, the last token is definitely a blank
      wcerr.flush();
      inputBuffer.front()->output(out);
      fflush(out);
      break;
    }
    else if(!furtherInput && inputBuffer.size() == 0) break;
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
    unsigned int len = 0;
    int rule = -1;
    unsigned int i = 0;
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
        mx->matchBlank(state, first, last);
      }
      else
      {
        mx->matchChunk(state, first, last, (*it)->matchSurface(), false);
        int r = mx->getRule(state, first, last).first;
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
        wcerr << endl << "Applying rule " << rule;
        if(rule <= (int)inRuleNames.size())
        {
          wcerr << " (" << inRuleNames[rule-1] << ")";
        }
        wcerr << ": ";
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->writeTree(TreeModeFlat, NULL);
        }
        wcerr << endl;
      }
      if(applyRule(rule_map[rule-1]))
      {
        for(unsigned int n = 0; n < currentOutput.size(); n++)
        {
          t2x.push_back(currentOutput[n]);
        }
        for(unsigned int n = 0; n < len; n++)
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
          wcerr << endl << L"Applying output rule " << cur->rule;
          if(cur->rule < (int)outRuleNames.size())
          {
            wcerr << " (" << outRuleNames[cur->rule] << ")";
          }
          wcerr << ": ";
          cur->writeTree(TreeModeFlat, NULL);
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
  if(printingAll && treePrintMode == TreeModeLatex)
  {
    wcerr << "\\documentclass{article}" << endl;
    wcerr << "\\usepackage{fontspec}" << endl;
    wcerr << "\\setmainfont{FreeSans}" << endl;
    wcerr << "\\usepackage{forest}" << endl;
    wcerr << "\\usepackage[cm]{fullpage}" << endl << endl;
    wcerr << "\\begin{document}" << endl << endl;
  }
  if(null_flush)
  {
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
  }
  else if(isLinear)
  {
    processTRX(in, out);
  }
  else
  {
    processGLR(in, out);
  }
  if(printingAll && treePrintMode == TreeModeLatex)
  {
    wcerr << endl << endl << "\\end{document}" << endl;
  }
}
