#include <rtx_config.h>
#include <rtx_processor.h>
#include <bytecode.h>
//#include <apertium/trx_reader.h>
#include <lttoolbox/compression.h>

#include <iostream>
#include <lttoolbox/string_utils.h>
//#include <apertium/unlocked_cstdio.h>

using namespace std;

RTXProcessor::RTXProcessor()
{
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
    cerr << "Unable to open file " << filename.c_str() << endl;
    exit(EXIT_FAILURE);
  }

  longestPattern = 2*Compression::multibyte_read(in) - 1;
  int count = Compression::multibyte_read(in);
  pat_size.reserve(count);
  rule_map.reserve(count);
  for(int i = 0; i < count; i++)
  {
    pat_size.push_back(Compression::multibyte_read(in));
    rule_map.push_back(Compression::string_read(in));
  }
  count = Compression::multibyte_read(in);
  output_rules.reserve(count);
  for(int i = 0; i < count; i++)
  {
    output_rules.push_back(Compression::string_read(in));
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
  bool recompile_attrs = !Compression::string_read(in).empty();
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    UString const cad_k = Compression::string_read(in);
    attr_items[cad_k].read(in);
    UString fallback = Compression::string_read(in);
    if (recompile_attrs && cad_k == "chname"_u) {
      // chname was previously "({([^/]+)\\/)"
      // which was fine for PCRE, but ICU chokes on the unmatched bracket
      fallback = "(\\{([^/]+)\\/)"_u;
    }
    attr_items[cad_k].compile(fallback);
  }

  // variables
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    UString const cad_k = Compression::string_read(in);
    variables[cad_k] = Compression::string_read(in);
  }

  // lists
  for(int i = 0, limit = Compression::multibyte_read(in); i != limit; i++)
  {
    UString const cad_k = Compression::string_read(in);

    for(int j = 0, limit2 = Compression::multibyte_read(in); j != limit2; j++)
    {
      UString const cad_v = Compression::string_read(in);
      lists[cad_k].insert(cad_v);
      listslow[cad_k].insert(StringUtils::tolower(cad_v));
    }
  }

  int nameCount = Compression::multibyte_read(in);
  for(int i = 0; i < nameCount; i++)
  {
    inRuleNames.push_back(Compression::string_read(in));
  }
  nameCount = Compression::multibyte_read(in);
  for(int i = 0; i < nameCount; i++)
  {
    outRuleNames.push_back(Compression::string_read(in));
  }

  fclose(in);
}

bool
RTXProcessor::beginsWith(UString const &s1, UString const &s2) const
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
RTXProcessor::endsWith(UString const &s1, UString const &s2) const
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

inline bool
RTXProcessor::popBool()
{
  if(theStack[stackIdx].mode == 0)
  {
    return theStack[stackIdx--].b;
  }
  else
  {
    cerr << "tried to pop bool but mode is " << theStack[stackIdx].mode << endl;
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
    cerr << "tried to pop int but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline UString
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
    cerr << "tried to pop UString but mode is " << theStack[stackIdx].mode << endl;
    exit(1);
  }
}

inline void
RTXProcessor::popString(UString& dest)
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
    cerr << "tried to pop UString but mode is " << theStack[stackIdx].mode << endl;
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
    cerr << "tried to pop Chunk but mode is " << theStack[stackIdx].mode << endl;
    cerr << "The most common reason for getting this error is a macro that is missing an else clause." << endl;
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
      theWblankStack[dest] = theWblankStack[src];
      break;
    case 1:
      theStack[dest].i = theStack[src].i;
      theWblankStack[dest] = theWblankStack[src];
      break;
    case 2:
      theStack[dest].s = theStack[src].s;
      theWblankStack[dest] = theWblankStack[src];
      break;
    case 3:
      theStack[dest].c = theStack[src].c;
      theWblankStack[dest] = theWblankStack[src];
      break;
    default:
      cerr << "Unknown StackElement mode " << theStack[src].mode;
      break;
  }
}

bool
RTXProcessor::gettingLemmaFromWord(UString attr)
{
    return (attr.compare("lem"_u) == 0 || attr.compare("lemh"_u) == 0 || attr.compare("whole"_u) == 0);
}

bool
RTXProcessor::applyRule(const UString& rule)
{
  stackIdx = 0;
  vector<bool> editted = vector<bool>(currentInput.size(), false);
  const UChar* rule_data = rule.data();
  for(unsigned int i = 0, rule_size = rule.size(); i < rule_size; i++)
  {
    switch(rule_data[i])
    {
      case DROP:
        if(printingSteps) { cerr << "drop" << endl; }
        stackIdx--;
        break;
      case DUP:
        if(printingSteps) { cerr << "dup" << endl; }
        stackCopy(stackIdx, stackIdx+1);
        stackIdx++;
        break;
      case OVER:
        if(printingSteps) { cerr << "over" << endl; }
        stackCopy(stackIdx-1, stackIdx+1);
        stackIdx++;
        break;
      case SWAP:
        if(printingSteps) { cerr << "swap" << endl; }
      {
        stackCopy(stackIdx, stackIdx+1);
        stackCopy(stackIdx-1, stackIdx);
        stackCopy(stackIdx+1, stackIdx-1);
      }
        break;
      case STRING:
      {
        if(printingSteps) { cerr << "string" << endl; }
        int ct = rule_data[++i];
        stackIdx++;
        theStack[stackIdx].mode = 2;
        theStack[stackIdx].s.assign(rule, i+1, ct);
        //pushStack(rule.substr(i+1, ct));
        i += ct;
        if(printingSteps) { cerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case INT:
        if(printingSteps) { cerr << "int " << (int)rule[i+1] << endl; }
        pushStack((int)rule_data[++i]);
        break;
      case PUSHFALSE:
        if(printingSteps) { cerr << "pushfalse" << endl; }
        pushStack(false);
        break;
      case PUSHTRUE:
        if(printingSteps) { cerr << "pushtrue" << endl; }
        pushStack(true);
        break;
      case PUSHNULL:
        if(printingSteps) { cerr << "pushnull" << endl; }
        pushStack((Chunk*)NULL);
        break;
      case JUMP:
        if(printingSteps) { cerr << "jump" << endl; }
        ++i;
        i += rule_data[i];
        break;
      case JUMPONTRUE:
        if(printingSteps) { cerr << "jumpontrue" << endl; }
        if(!popBool())
        {
          i++;
          if(printingSteps) { cerr << " -> false" << endl; }
        }
        else
        {
          ++i;
          i += rule_data[i];
          if(printingSteps) { cerr << " -> true, jumping" << endl; }
        }
        break;
      case JUMPONFALSE:
        if(printingSteps) { cerr << "jumponfalse" << endl; }
        if(popBool())
        {
          i++;
          if(printingSteps) { cerr << " -> true" << endl; }
        }
        else
        {
          ++i;
          i += rule_data[i];
          if(printingSteps) { cerr << " -> false, jumping" << endl; }
        }
        break;
      case AND:
        if(printingSteps) { cerr << "and" << endl; }
      {
        bool a = popBool();
        bool b = popBool();
        pushStack(a && b);
      }
        break;
      case OR:
        if(printingSteps) { cerr << "or" << endl; }
      {
        bool a = popBool();
        bool b = popBool();
        pushStack(a || b);
      }
        break;
      case NOT:
        if(printingSteps) { cerr << "not" << endl; }
        theStack[stackIdx].b = !theStack[stackIdx].b;
        break;
      case EQUAL:
      case EQUALCL:
        if(printingSteps) { cerr << "equal" << endl; }
      {
        UString a;
        popString(a);
        UString b;
        popString(b);
        if(rule_data[i] == EQUALCL)
        {
          a = StringUtils::tolower(a);
          b = StringUtils::tolower(b);
        }
        pushStack(a == b);
        if(printingSteps) { cerr << " -> " << (a == b ? "true" : "false") << endl; }
      }
        break;
      case ISPREFIX:
      case ISPREFIXCL:
        if(printingSteps) { cerr << "isprefix" << endl; }
      {
        UString substr = popString();
        UString str = popString();
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
        if(printingSteps) { cerr << "issuffix" << endl; }
      {
        UString substr = popString();
        UString str = popString();
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
        if(printingSteps) { cerr << "hasprefix" << endl; }
      {
        UString list = popString();
        UString needle = popString();
        set<UString>::iterator it, limit;

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
        if(printingSteps) { cerr << "hassuffix" << endl; }
      {
        UString list = popString();
        UString needle = popString();
        set<UString>::iterator it, limit;

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
        if(printingSteps) { cerr << "issubstring" << endl; }
      {
        UString needle = popString();
        UString haystack = popString();
        if(rule[i] == ISSUBSTRINGCL)
        {
          needle = StringUtils::tolower(needle);
          haystack = StringUtils::tolower(haystack);
        }
        pushStack(haystack.find(needle) != UString::npos);
      }
        break;
      case IN:
      case INCL:
        if(printingSteps) { cerr << "in" << endl; }
      {
        UString list = popString();
        UString str = popString();
        if(rule[i] == INCL)
        {
          str = StringUtils::tolower(str);
          set<UString> &myset = listslow[list];
          pushStack(myset.find(str) != myset.end());
        }
        else
        {
          set<UString> &myset = lists[list];
          pushStack(myset.find(str) != myset.end());
        }
      }
        break;
      case SETVAR:
        if(printingSteps) { cerr << "setvar" << endl; }
      {
        UString var = popString();
        UString val = popString();
        currentBranch->stringVars[var] = val;
        currentBranch->wblankVars[var] = theWblankStack[stackIdx+1];
        theWblankStack[stackIdx+1].clear();
        if(printingSteps) { cerr << " -> " << var << " = '" << val << "'" << endl; }
      }
        break;
      case OUTPUT:
        if(printingSteps) { cerr << "output" << endl; }
      {
        Chunk* ch = popChunk();
        if(ch == NULL) break; // FETCHCHUNK
        if(isLinear && ch->contents.size() == 0)
        {
          bool word = true;
          unsigned int last = 0;
          const UChar* targ = ch->target.data();
          bool chunk = false;
          for(unsigned int c = 0, limit = ch->target.size(); c < limit; c++)
          {
            if(targ[c] == '\\') c++;
            else if((targ[c] == '{' || targ[c] == '$') && word)
            {
              if(targ[c] == '{') chunk = true;
              Chunk* temp = chunkPool.next();
              temp->isBlank = false;
              temp->target = ch->target.substr(last, c-last);
              temp->wblank = out_wblank;
              out_wblank.clear();
              if(chunk) currentOutput.back()->contents.push_back(temp);
              else currentOutput.push_back(temp);
              last = c+1;
              word = false;
            }
            else if((targ[c] == '^' || targ[c] == '}') && !word)
            {
              if(c > last)
              {
                Chunk* temp = chunkPool.next();
                temp->isBlank = true;
                temp->target = ch->target.substr(last, c-last);
                if(chunk) currentOutput.back()->contents.push_back(temp);
                else currentOutput.push_back(temp);
              }
              if(targ[c] == '}') chunk = false;
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
          ch->wblank = out_wblank;
          currentOutput.push_back(ch);
          out_wblank.clear();
        }
      }
        break;
      case OUTPUTALL:
        if(printingSteps) { cerr << "outputall" << endl; }
        currentOutput = currentInput;
        return true;
        break;
      case PUSHINPUT:
        if(printingSteps) { cerr << "pushinput" << endl; }
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
            //cerr << "Clip index is out of bounds." << endl;
            //exit(EXIT_FAILURE);
            ch = currentInput.back();
          }
        }
        pushStack(ch);
      }
        break;
      case SOURCECLIP:
        if(printingSteps) { cerr << "sourceclip" << endl; }
      {
        UString part;
        popString(part);
        Chunk* ch = popChunk();
        if(ch == NULL) pushStack("");
        else
        {
          if(gettingLemmaFromWord(part))
          {
            pushStack(ch->chunkPart(attr_items[part], SourceClip), ch->wblank);
          }
          else
          {
            pushStack(ch->chunkPart(attr_items[part], SourceClip));
          }
        }
        if(printingSteps) { cerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case TARGETCLIP:
        if(printingSteps) { cerr << "targetclip" << endl; }
      {
        UString part;
        popString(part);
        Chunk* ch = popChunk();
        if(ch == NULL) pushStack("");
        else
        {
          if(gettingLemmaFromWord(part))
          {
            pushStack(ch->chunkPart(attr_items[part], TargetClip), ch->wblank);
          }
          else
          {
             pushStack(ch->chunkPart(attr_items[part], TargetClip));
          }
        }
        if(printingSteps) { cerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case REFERENCECLIP:
        if(printingSteps) { cerr << "referenceclip" << endl; }
      {
        UString part;
        popString(part);
        Chunk* ch = popChunk();
        if(ch == NULL) pushStack("");
        else pushStack(ch->chunkPart(attr_items[part], ReferenceClip));
        if(printingSteps) { cerr << " -> " << theStack[stackIdx].s << endl; }
      }
        break;
      case SETCLIP:
        if(printingSteps) { cerr << "setclip" << endl; }
      {
        int pos = 2*(popInt()-1);
        UString part = popString();
        if(pos >= 0)
        {
          if(!editted[pos])
          {
            currentInput[pos] = currentInput[pos]->copy();
            editted[pos] = true;
          }
          currentInput[pos]->setChunkPart(attr_items[part], popString());
          if(printingSteps) { cerr << " -> " << currentInput[pos]->target << endl; }
        }
        else
        {
          theStack[stackIdx].c->setChunkPart(attr_items[part], popString());
        }
      }
        break;
      case FETCHVAR:
        if(printingSteps) { cerr << "fetchvar" << endl; }
        {
          UString name = popString();
          UString val = currentBranch->stringVars[name];
          UString wblank_val = currentBranch->wblankVars[name];
          pushStack(val, wblank_val);
          if(printingSteps) { cerr << " -> " << name << " = " << val << endl; }
        }
        break;
      case FETCHCHUNK:
        if(printingSteps) { cerr << "fetchchunk" << endl; }
        pushStack(currentBranch->chunkVars[popInt()]);
        break;
      case SETCHUNK:
        if(printingSteps) { cerr << "setchunk" << endl; }
        {
          int pos = popInt();
          currentBranch->chunkVars[pos] = popChunk();
        }
        break;
      case GETCASE:
        if(printingSteps) { cerr << "getcase" << endl; }
        pushStack(StringUtils::getcase(popString()));
        if(printingSteps) { cerr << " -> " << theStack[stackIdx].s << endl; }
        break;
      case SETCASE:
        if(printingSteps) { cerr << "setcase" << endl; }
      {
        UString src = popString();
        UString dest = popString();
        pushStack(StringUtils::copycase(src, dest));
      }
        if(printingSteps) { cerr << " -> " << theStack[stackIdx].s << endl; }
        break;
      case CONCAT:
        if(printingSteps) { cerr << "concat" << endl; }
      {
        if(theStack[stackIdx].mode != 2 || theStack[stackIdx-1].mode != 2)
        {
          cerr << "Cannot CONCAT non-strings." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        theStack[stackIdx].s.append(theStack[stackIdx+1].s);
      }
        break;
      case CHUNK:
        if(printingSteps) { cerr << "chunk" << endl; }
      {
        Chunk* ch = chunkPool.next();
        ch->isBlank = false;
        pushStack(ch);
      }
        break;
      case APPENDCHILD:
        if(printingSteps) { cerr << "appendchild" << endl; }
      {
        Chunk* kid = popChunk();
        if(isLinear && kid->target[0] == '^')
        {
          unsigned int j = 0;
          for(; j < kid->target.size(); j++)
          {
            if(kid->target[j] == '$') break;
          }
          Chunk* ch = chunkPool.next();
          ch->isBlank = false;
          ch->target = kid->target.substr(1, j-1);
          ch->wblank = out_wblank;
          out_wblank.clear();
          theStack[stackIdx].c->contents.push_back(ch);
          ch = chunkPool.next();
          ch->isBlank = true;
          ch->target = kid->target.substr(j+1);
          theStack[stackIdx].c->contents.push_back(ch);
        }
        else
        {
          kid->wblank = out_wblank;
          out_wblank.clear();
          theStack[stackIdx].c->contents.push_back(kid);
        }
        if(printingSteps) { cerr << " -> child with surface '" << kid->target << "' appended" << endl; }
      }
        break;
      case APPENDSURFACE:
        if(printingSteps) { cerr << "appendsurface" << endl; }
      {
        if(theStack[stackIdx].mode != 2 && theStack[stackIdx].mode != 3)
        {
          cerr << "Cannot append non-string to chunk surface." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        if(theStack[stackIdx].mode != 3)
        {
          cerr << "Cannot APPENDSURFACE to non-chunk." << endl;
          exit(EXIT_FAILURE);
        }
        if(theStack[stackIdx+1].mode == 2)
        {
          theStack[stackIdx].c->target += theStack[stackIdx+1].s;
          out_wblank = combineWblanks(out_wblank, theWblankStack[stackIdx+1]);
          theWblankStack[stackIdx+1].clear();
        }
        else
        {
          theStack[stackIdx].c->target += theStack[stackIdx+1].c->target;
          theStack[stackIdx].c->wblank += theStack[stackIdx+1].c->wblank;
        }
        if(printingSteps) { cerr << " -> " << theStack[stackIdx+1].s << endl; }
      }
        break;
      case APPENDSURFACESL:
        if(printingSteps) { cerr << "appendsurfacesl" << endl; }
      {
        if(theStack[stackIdx].mode != 2 && theStack[stackIdx].mode != 3)
        {
          cerr << "Cannot append non-string to chunk surface." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        if(theStack[stackIdx].mode != 3)
        {
          cerr << "Cannot APPENDSURFACESL to non-chunk." << endl;
          exit(EXIT_FAILURE);
        }
        if(theStack[stackIdx+1].mode == 2)
        {
          theStack[stackIdx].c->source += theStack[stackIdx+1].s;
          out_wblank = combineWblanks(out_wblank, theWblankStack[stackIdx+1]);
          theWblankStack[stackIdx+1].clear();
        }
        else
        {
          theStack[stackIdx].c->source += theStack[stackIdx+1].c->source;
          theStack[stackIdx].c->wblank += theStack[stackIdx+1].c->wblank;
        }
        if(printingSteps) { cerr << " -> " << theStack[stackIdx+1].s << endl; }
      }
        break;
      case APPENDSURFACEREF:
        if(printingSteps) { cerr << "appendsurfaceref" << endl; }
      {
        if(theStack[stackIdx].mode != 2 && theStack[stackIdx].mode != 3)
        {
          cerr << "Cannot append non-string to chunk surface." << endl;
          exit(EXIT_FAILURE);
        }
        stackIdx--;
        if(theStack[stackIdx].mode != 3)
        {
          cerr << "Cannot APPENDSURFACEREF to non-chunk." << endl;
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
        if(printingSteps) { cerr << " -> " << theStack[stackIdx+1].s << endl; }
      }
        break;
      case APPENDALLCHILDREN:
        if(printingSteps) { cerr << "appendallchildren" << endl; }
      {
        Chunk* ch = popChunk();
        for(unsigned int k = 0; k < ch->contents.size(); k++)
        {
          theStack[stackIdx].c->contents.push_back(ch->contents[k]);
        }
      }
        break;
      case APPENDALLINPUT:
        if(printingSteps) { cerr << "appendallinput" << endl; }
      {
        vector<Chunk*>& vec = theStack[stackIdx].c->contents;
        vec.insert(vec.end(), currentInput.begin(), currentInput.end());
      }
        break;
      case BLANK:
        if(printingSteps) { cerr << "blank" << endl; }
      {
        int loc = 2*(popInt()-1) + 1;
        if(loc == -1)
        {
          Chunk* ch = chunkPool.next();
          ch->target = " "_u;
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
        if(printingSteps) { cerr << "conjoin" << endl; }
      {
        Chunk* join = chunkPool.next();
        join->isBlank = true;
        join->isJoiner = true;
        join->target = "+"_u;
        pushStack(join);
      }
        break;
      case REJECTRULE:
        if(printingSteps) { cerr << "rejectrule" << endl; }
        return false;
        break;
      case DISTAG:
        if(printingSteps) { cerr << "distag" << endl; }
      {
        if(theStack[stackIdx].mode != 2)
        {
          cerr << "Cannot DISTAG non-string." << endl;
          exit(EXIT_FAILURE);
        }
        UString& s = theStack[stackIdx].s;
        if(s.size() > 0 && s[0] == '<' && s[s.size()-1] == '>')
        {
          s = StringUtils::substitute(s.substr(1, s.size()-2), "><"_u, "."_u);
        }
      }
        break;
      case GETRULE:
        if(printingSteps) { cerr << "getrule" << endl; }
      {
        int pos = 2*(popInt()-1);
        pushStack(currentInput[pos]->rule);
      }
        break;
      case SETRULE:
        if(printingSteps) { cerr << "setrule" << endl; }
      {
        int pos = 2*(popInt()-1);
        int rl = popInt();
        if(pos == -2)
        {
          if(stackIdx == 0 || theStack[stackIdx].mode != 3)
          {
            cerr << "Empty stack or top item is not chunk." << endl;
            cerr << "Check for conditionals that might not generate output" << endl;
            cerr << "and ensure that lists of attributes are complete." << endl;
            exit(1);
          }
          theStack[stackIdx].c->rule = rl;
        }
        else
        {
          currentInput[pos]->rule = rl;
        }
      }
        break;
      case LUCOUNT:
        if(printingSteps) { cerr << "lucount" << endl; }
        pushStack(StringUtils::itoa((currentInput.size() + 1) / 2));
        break;
      default:
        cerr << "unknown instruction: " << rule[i] << endl;
        exit(1);
    }
  }
  return true;
}

Chunk *
RTXProcessor::readToken()
{
  int pos = 0;
  UString cur;
  UString wbl;
  UString src;
  UString dest;
  UString coref;
  cur.reserve(256);
  while(true)
  {
    UChar32 val = infile.get();
    if (infile.eof() || (null_flush && val == '\0')) {
      furtherInput = false;
      Chunk* ret = chunkPool.next();
      ret->target = cur;
      ret->isBlank = true;
      return ret;
    }
    else if(val == '\\')
    {
      cur += '\\';
      cur += infile.get();
    }
    else if(val == '[' && !inword)
    {
      val = infile.get();
      
      if(val == '[')
      {
        inwblank = true;
        Chunk* ret = chunkPool.next();
        ret->target = cur;
        ret->isBlank = true;
        return ret;
      }
      else
      {
        infile.unget(val);
        cur += infile.readBlock('[', ']');
      }
    }
    else if(inwblank)
    {
      if(val == ']')
      {
        cur += val;
        val = infile.get();
        
        if(val == '\\')
        {
          cur += '\\';
          cur += infile.get();
        }
        else if(val == ']')
        {
          cur += val;
          val = infile.get();
          
          if(val == '\\')
          {
            cur += '\\';
            cur += infile.get();
          }
          else if(val == '^')
          {
            inwblank = false;
            cur = "[["_u + cur;
            wbl.swap(cur);
            inword = true;
          }
          else
          {
            cerr << "Parse Error: Wordbound blank should be immediately followed by a Lexical Unit -> [[..]]^..$" << endl;
            exit(EXIT_FAILURE);
          }
        }
        else
        {
          cur += val;
        }
      }
      else
      {
        cur += val;
      }
    }
    else if(inword && (val == '$' || val == '/'))
    {
      if(pos == 0)
      {
        src.swap(cur);
      }
      else if(pos == 1)
      {
        dest.swap(cur);
      }
      else if(pos >= 2 && !noCoref && val == '$')
      {
        coref.swap(cur);
      }
      else
      {
        cur.clear();
      }
      pos++;
      if(val == '$')
      {
        inword = false;
        Chunk* ret = chunkPool.next();
        ret->wblank = wbl;
        ret->source = src;
        ret->target = dest;
        ret->coref = coref;
        ret->isBlank = false;
        if(src.size() > 0 && src[0] == '*' && dest.size() > 0 && dest[0] == '*')
        {
          Chunk* ret2 = chunkPool.next();
          ret2->target = ret->target.substr(1) + "<UNKNOWN:INTERNAL>"_u;
          ret2->contents.push_back(ret);
          ret2->rule = -1;
          ret2->isBlank = false;
          return ret2;
        }
        return ret;
      }
    }
    else if(!inword && val == '^')
    {
      inword = true;
      Chunk* ret = chunkPool.next();
      ret->target = cur;
      ret->isBlank = true;
      return ret;
    }
    else
    {
      cur += val;
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
  if(printingAll) cerr << "Checking for reductions for branch " << node->id << endl;
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
      if(printingAll && treePrintMode == TreeModeLatex) cerr << "\\subsection{";
      else cerr << endl;
      cerr << "Applying rule " << rule;
      if(rule <= (int)inRuleNames.size())
      {
        cerr << " (" << inRuleNames[rule-1] << ")";
      }
      if(printingAll) cerr << " to branch " << node->id << " with weight " << rule_and_weight.second;
      if(printingAll && treePrintMode == TreeModeLatex) cerr << "}" << endl << endl;
      else cerr << ": ";
      for(unsigned int i = 0; i < currentInput.size(); i++)
      {
        currentInput[i]->writeTree((printingAll ? treePrintMode : TreeModeFlat), NULL);
      }
      cerr << endl;
    }
    if(applyRule(rule_map[rule-1]))
    {
      if(printingAll)
      {
        for(auto c : currentOutput) c->writeTree(treePrintMode, NULL);
        cerr << endl;
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
      cur->wblankVars = node->wblankVars;
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
          cur->wblankVars = (*it)->wblankVars;
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
      if(printingRules) { cerr << " -> rule was rejected" << endl; }
      if(printingAll) cerr << "This rule was rejeced." << endl << endl;
      mx->rejectRule(rule);
      rule_and_weight = node->getRule();
      rule = rule_and_weight.first;
      weight = node->weight + rule_and_weight.second;
    }
  }
  if(rule == -1)
  {
    if(printingAll) cerr << "No further reductions possible for branch " << node->id << "." << endl;
    result.push_back(node);
  }
  else if(lookahead(node))
  {
    node->id = ++newBranchId;
    if(printingAll) cerr << endl << "Splitting stack and creating branch " << node->id << endl;
    result.push_back(node);
  }
}

void
RTXProcessor::outputAll(UFILE* out)
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
      if(printingText) u_fputc('\n', out);
      queueSize--;
      ch->writeTree(treePrintMode, out);
      u_fflush(out);
      if(!printingText) continue;
    }
    if(ch->rule == -1)
    {
      if(printingRules && !ch->isBlank)
      {
        u_fflush(out);
        cerr << endl << "No rule specified: ";
        ch->writeTree(TreeModeFlat, NULL);
        cerr << endl;
      }
      if(printingAll && !ch->isBlank)
      {
        if(treePrintMode == TreeModeLatex) cerr << "\\subsubsection{Output Node}" << endl;
        else cerr << "Output Node:" << endl;
        ch->writeTree(treePrintMode, NULL);
        cerr << endl;
      }
      if(ch->contents.size() > 0)
      {
        vector<UString> tags = ch->getTags(vector<UString>());
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
        if(ch->isBlank)
        {
          writeBlank(out);
        }
        else tojoin = ch;
      }
    }
    else
    {
      parentChunk = ch;
      vector<UString> tags = ch->getTags(vector<UString>());
      currentInput = ch->contents;
      for(unsigned int i = 0; i < currentInput.size(); i++)
      {
        currentInput[i]->updateTags(tags);
      }
      currentOutput.clear();
      if(printingRules) {
        u_fflush(out);
        cerr << endl << "Applying output rule " << ch->rule;
        if(ch->rule < (int)outRuleNames.size())
        {
          cerr << " (" << outRuleNames[ch->rule] << ")";
        }
        cerr << ": " << parentChunk->target << " -> ";
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->writeTree(TreeModeFlat, NULL);
        }
        cerr << endl;
      }
      if(printingAll)
      {
        if(treePrintMode == TreeModeLatex)
        {
          cerr << "\\subsubsection{Applying Output Rule " << ch->rule;
          if(ch->rule < (int)outRuleNames.size())
          {
            cerr << ": " << outRuleNames[ch->rule] << "}" << endl << endl;
          }
        }
        else
        {
          cerr << "Applying Output Rule " << ch->rule;
          if(ch->rule < (int)outRuleNames.size())
          {
            cerr << ": " << outRuleNames[ch->rule] << endl << endl;
          }
        }
        ch->writeTree(treePrintMode, NULL);
      }
      u_fflush(out);
      applyRule(output_rules[ch->rule]);
      for(vector<Chunk*>::reverse_iterator it = currentOutput.rbegin(),
              limit = currentOutput.rend(); it != limit; it++)
      {
        outputQueue.push_front(*it);
      }
    }
  }
  if(tojoin != NULL) tojoin->output(out);
  while(!blankQueue.empty())
  {
    if(blankQueue.front() == " "_u)
    {
      blankQueue.pop_front();
    }
    else
    {
      writeBlank(out);
    }
  }
}

void
RTXProcessor::writeBlank(UFILE* out)
{
  if(blankQueue.empty())
  {
    blankQueue.push_back(" "_u);
  }
  Chunk* blank = chunkPool.next();
  blank->target = blankQueue.front();
  blankQueue.pop_front();
  blank->isBlank = true;
  if (printingText) {
    blank->output(out);
  }
  if (printingTrees) {
    blank->writeTree(treePrintMode, out);
  }
}

bool
RTXProcessor::filterParseGraph()
{
  if(printingAll)
  {
    if(treePrintMode == TreeModeLatex)
    {
      cerr << "\\subsection{Filtering Branches}\n\n\\begin{itemize}" << endl;
    }
    else cerr << endl << "Filtering Branches:" << endl;
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
          cerr << "\\item No branch can accept further input." << endl;
        }
        else cerr << "No branch can accept further input." << endl;
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
      cerr << "\\item Input buffer is empty." << endl;
    }
    else cerr << "Input buffer is empty." << endl;
  }
  int min = -1;
  ParseNode* minNode = NULL;
  ParseNode* cur = NULL;
  map<int, vector<int>> filter;
  if(printingBranches) { cerr << "shouldOutput: " << shouldOutput << " branch count: " << N << endl; }
  for(int i = 0; i < N; i++)
  {
    if(printingBranches) { cerr << "examining node " << i << "(length: " << parseGraph[i]->length << ", weight: " << parseGraph[i]->weight << ") ... "; }
    if(printingAll)
    {
      if(treePrintMode == TreeModeLatex) cerr << "\\item ";
      cerr << "Branch " << parseGraph[i]->id << " ";
    }
    if(state[i] == 0)
    {
      if(printingAll) cerr << " has no possible continuations." << endl;
      continue;
    }
    else if(noFilter && !shouldOutput) continue;
    if(min == -1)
    {
      if(printingAll) cerr << " has no active branch to compare to." << endl;
      if(printingBranches) { cerr << "FIRST!" << endl; }
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
          if(printingBranches) { cerr << i << " beats " << min << " in length or weight" << endl; }
          if(printingAll) cerr << " has fewer partial parses or a higher weight than branch " << minNode->id << "." << endl;
          state[min] = 0;
          min = i;
          minNode = cur;
        }
        else
        {
          state[i] = 0;
          if(printingBranches) {cerr << min << " beats " << i << " in length or weight" << endl; }
          if(printingAll) cerr << " has more partial parses or a lower weight than branch " << minNode->id << "." << endl;
        }
        count--;
      }
      else if(filter.find(cur->firstWord) == filter.end())
      {
        filter[cur->firstWord].push_back(i);
        if(printingBranches) { cerr << i << " has nothing to compare with" << endl; }
        if(printingAll) cerr << " has no prior branch covering the same final span." << endl;
      }
      else
      {
        vector<int>& other = filter[cur->firstWord];
        double w = parseGraph[other[0]]->weight;
        if(w > cur->weight)
        {
          if(printingBranches) { cerr << i << " has lower weight - discarding." << endl; }
          if(printingAll) cerr << " has a lower weight than branch " << parseGraph[other[0]]->id << " and will be discarded." << endl;
          state[i] = 0;
          count--;
        }
        else if(w < cur->weight)
        {
          if(printingBranches) { cerr << i << " has higher weight - discarding others." << endl; }
          if(printingAll)
          {
            cerr << " has a higher weight than ";
            for(auto it : other) cerr << "branch " << parseGraph[it]->id << ", ";
            cerr << "which will be discarded." << endl;
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
          if(printingBranches) { cerr << i << " has same weight - keeping all." << endl; }
          if(printingAll) cerr << " has the same weight as branch " << parseGraph[other[0]]->id << "." << endl;
          other.push_back(i);
        }
      }
    }
  }
  if(printingAll && treePrintMode == TreeModeLatex) cerr << "\\end{itemize}" << endl << endl;
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
        cerr << "keeping branch " << i << " first word: " << parseGraph[i]->firstWord << " ending with ";
        parseGraph[i]->chunk->writeTree(TreeModeFlat, NULL);
        cerr << endl;
      }
    }
    else if(printingBranches)
    {
      cerr << "discarding branch " << i << " first word: " << parseGraph[i]->firstWord << " ending with ";
      parseGraph[i]->chunk->writeTree(TreeModeFlat, NULL);
      cerr << endl;
    }
  }
  if(printingBranches) { cerr << "remaining branches: " << temp.size() << endl << endl; }
  parseGraph.swap(temp);
  return shouldOutput;
}

void
RTXProcessor::processGLR(UFILE *out)
{
  int sentenceId = 1;
  if(printingAll && treePrintMode == TreeModeLatex)
  {
    cerr << "\\section{Sentence " << sentenceId << "}" << endl << endl;
  }
  while(furtherInput && inputBuffer.size() < 5)
  {
    inputBuffer.push_back(readToken());
  }
  bool real_printingAll = printingAll;
  while(true)
  {
    Chunk* next = inputBuffer.front();
    if(next->isBlank)
    {
      printingAll = false;
      if(!parseGraph.empty() && !next->target.empty())
      {
        // if parseGraph is empty, we're about to write it immediately anyway,
        // so don't bother with the queue
        // if target is empty, we don't want to put it in the wrong place
        // if a rule asks for a space
        // -D.S. 2020-09-21
        blankQueue.push_back(next->target);
      }
    }
    if(printingAll)
    {
      cerr << endl;
      if(treePrintMode == TreeModeLatex) cerr << "\\subsection{Reading Input}" << endl << endl;
      else cerr << "Reading Input:" << endl;
      next->writeTree(treePrintMode, NULL);
      cerr << endl;
    }
    inputBuffer.pop_front();
    if(parseGraph.size() == 0)
    {
      // skip parseGraph stuff if a blank is the only thing being processed
      if(next->isBlank)
      {
        if (printingText) {
          next->output(out);
        }
        if (printingTrees) {
          next->writeTree(treePrintMode, out);
        }
        if(furtherInput)
        {
          inputBuffer.push_back(readToken());
        }
        if(inputBuffer.empty())
        {
          cerr.flush();
          u_fflush(out);
          break;
        }
        continue;
      }
      ParseNode* temp = parsePool.next();
      temp->init(mx, next);
      temp->id = ++newBranchId;
      temp->stringVars = variables;
      temp->wblankVars = wblank_variables;
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
        tempNode->wblankVars = parseGraph[i]->wblankVars;
        tempNode->chunkVars = parseGraph[i]->chunkVars;
        checkForReduce(temp, tempNode);
      }
      parseGraph.swap(temp);
    }
    if(printingAll && treePrintMode != TreeModeLatex)
    {
      for(auto branch : parseGraph)
      {
        cerr << "Branch " << branch->id << ": " << branch->length << " nodes, weight = " << branch->weight << endl;
        vector<Chunk*> parts;
        parts.resize(branch->length);
        branch->getChunks(parts, branch->length-1);
        for(auto node : parts)
        {
          if(node->isBlank) cerr << "[Blank]: " << endl;
          else cerr << "[Chunk]: " << endl;
          node->writeTree(treePrintMode, NULL);
        }
      }
    }
    if(furtherInput) inputBuffer.push_back(readToken());
    if(filterParseGraph())
    {
      cerr.flush();
      if(printingAll)
      {
        if(treePrintMode == TreeModeLatex) cerr << "\\subsection{Outputting Branch " << parseGraph[0]->id << "}" << endl << endl;
        else
        {
          cerr << endl;
          cerr << "************************************************************" << endl;
          cerr << "************************************************************" << endl;
          cerr << "************************************************************" << endl;
          cerr << "Outputting Branch " << parseGraph[0]->id << endl << endl;
          vector<Chunk*> parts;
          parts.resize(parseGraph[0]->length);
          parseGraph[0]->getChunks(parts, parseGraph[0]->length-1);
          for(auto node : parts)
          {
            if(node->isBlank) cerr << "[Blank]: " << endl;
            else cerr << "[Chunk]: " << endl;
            node->writeTree(treePrintMode, NULL);
          }
          cerr << "************************************************************" << endl;
          cerr << "************************************************************" << endl;
          cerr << "************************************************************" << endl;
          cerr << endl;
        }
      }
      currentBranch = parseGraph[0];
      parseGraph[0]->getChunks(outputQueue, parseGraph[0]->length-1);
      parseGraph.clear();

      // We have now parsed input into a tree, and are ready to run
      // output rules and do the output. But first: For every chunk
      // that didn't get a parse, reparse it disregarding context, so
      // we can at least use single-word rules on them.
      {
        ParseNode* prevBranch = currentBranch;
        for(auto it = outputQueue.begin(); it != outputQueue.end();) {
          Chunk* ch = *it;
          if(ch->rule == -1 && !ch->isBlank) { // -1 means didn't get a parse
            if(printingAll) cerr << "Reparsing chunk ^" << ch->source << "/" << ch->target << "$" << endl;
            ParseNode* temp = parsePool.next();
            temp->init(mx, ch);
            temp->id = ++newBranchId;
            temp->stringVars = variables;
            temp->wblankVars = wblank_variables;
            temp->chunkVars = vector<Chunk*>(varCount, NULL);
            checkForReduce(parseGraph, temp);

            list<Chunk*> outputQueueReparsed;
            parseGraph[0]->getChunks(outputQueueReparsed, parseGraph[0]->length-1);
            it = outputQueue.erase(it); // skip current word since reparse includes it
            outputQueue.splice(it, outputQueueReparsed);
            parseGraph.clear();
          }
          else {
              ++it;
          }
        }
        currentBranch = prevBranch;
      }

      outputAll(out);
      variables = currentBranch->stringVars;
      wblank_variables = currentBranch->wblankVars;
      u_fflush(out);
      vector<UString> wblanks;
      vector<UString> sources;
      vector<UString> targets;
      vector<UString> corefs;
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
        wblanks.push_back(temp->wblank);
        sources.push_back(temp->source);
        targets.push_back(temp->target);
        corefs.push_back(temp->coref);
        blanks.push_back(temp->isBlank);
        inputBuffer.pop_front();
      }
      //cerr << "clearing chunkPool, size was " << chunkPool.size() << endl;
      //cerr << "clearing parsePool, size was " << parsePool.size() << endl;
      chunkPool.reset();
      parsePool.reset();
      newBranchId = 0;
      if(printingAll) sentenceId++;
      if((furtherInput || inputBuffer.size() > 1) && printingAll && treePrintMode == TreeModeLatex)
      {
        cerr << endl << endl << "\\section{Sentence " << sentenceId << "}" << endl << endl;
      }
      for(int i = 0; i < N; i++)
      {
        Chunk* c = chunkPool.next();
        c->wblank = wblanks[i];
        c->source = sources[i];
        c->target = targets[i];
        c->coref = corefs[i];
        c->isBlank = blanks[i];
        if(unknowns[i])
        {
          Chunk* c2 = chunkPool.next();
          c2->target = targets[i].substr(1) + "<UNKNOWN:INTERNAL>"_u;
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
      cerr.flush();
      inputBuffer.front()->output(out);
      blankQueue.clear();
      inputBuffer.pop_front();
      u_fflush(out);
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
        cerr << endl << "Applying rule " << rule;
        if(rule <= (int)inRuleNames.size())
        {
          cerr << " (" << inRuleNames[rule-1] << ")";
        }
        cerr << ": ";
        for(unsigned int i = 0; i < currentInput.size(); i++)
        {
          currentInput[i]->writeTree(TreeModeFlat, NULL);
        }
        cerr << endl;
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
RTXProcessor::processTRX(UFILE *out)
{
  list<Chunk*> t1x;
  list<Chunk*> t2x;
  list<Chunk*> t3x;
  while(furtherInput || t1x.size() > 0 || t2x.size() > 0)
  {
    while(furtherInput && t1x.size() < 2*longestPattern)
    {
      t1x.push_back(readToken());
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
      vector<UString> tags = cur->getTags(vector<UString>());
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
          cerr << endl << "Applying output rule " << cur->rule;
          if(cur->rule < (int)outRuleNames.size())
          {
            cerr << " (" << outRuleNames[cur->rule] << ")";
          }
          cerr << ": ";
          cur->writeTree(TreeModeFlat, NULL);
          cerr << endl;
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
RTXProcessor::process(FILE* in, UFILE* out)
{
  if(printingAll && treePrintMode == TreeModeLatex)
  {
    cerr << "\\documentclass{article}" << endl;
    cerr << "\\usepackage{fontspec}" << endl;
    cerr << "\\setmainfont{FreeSans}" << endl;
    cerr << "\\usepackage{forest}" << endl;
    cerr << "\\usepackage[cm]{fullpage}" << endl << endl;
    cerr << "\\begin{document}" << endl << endl;
  }
  infile.wrap(in);
  if(null_flush)
  {
    while(!infile.eof())
    {
      furtherInput = true;
      if(isLinear)
      {
        processTRX(out);
      }
      else
      {
        processGLR(out);
      }
      u_fputc('\0', out);
      u_fflush(out);
      chunkPool.reset();
      parsePool.reset();
      inputBuffer.clear();
      // I'm not sure how the leading blank after a null gets into inputBuffer,
      // but it does and clearing the buffer seems to fix the problem
      // (in theory, clearing the buffer here should have no effect at all
      // because processGLR() should consume everything in it)
      // - D.S. Aug 26 2020
    }
  }
  else if(isLinear)
  {
    processTRX(out);
  }
  else
  {
    processGLR(out);
  }
  if(printingAll && treePrintMode == TreeModeLatex)
  {
    cerr << endl << endl << "\\end{document}" << endl;
  }
}
