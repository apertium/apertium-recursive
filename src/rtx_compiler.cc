#include <rtx_compiler.h>
#include <apertium/string_utils.h>
#include <apertium/utf_converter.h>

using namespace std;

wstring const
RTXCompiler::ANY_TAG = L"<ANY_TAG>";

wstring const
RTXCompiler::ANY_CHAR = L"<ANY_CHAR>";

RTXCompiler::RTXCompiler()
{
  td.getAlphabet().includeSymbol(ANY_TAG);
  td.getAlphabet().includeSymbol(ANY_CHAR);
  td.getAttrItems()[L"pos_tag"] = L"(<[^>]+>)";
  longestPattern = 0;
  currentRule = NULL;
  currentChunk = NULL;
  currentChoice = NULL;
  errorsAreSyntax = true;
  inOutputRule = false;
  parserIsInChunk = false;
}

wstring const
RTXCompiler::SPECIAL_CHARS = L"!@$%()={}[]|/:;<>,.~→";

void
RTXCompiler::die(wstring message)
{
  if(errorsAreSyntax)
  {
    wcerr << L"Syntax error on line " << currentLine << L" of ";
  }
  else
  {
    wcerr << L"Error in rule beginning on line " << currentRule->line << L" of ";
  }
  wcerr << UtfConverter::fromUtf8(sourceFile) << L": " << message << endl;
  if(errorsAreSyntax && !source.eof())
  {
    wstring arr = wstring(recentlyRead.size()-2, L' ');
    while(!source.eof() && source.peek() != L'\n')
    {
      recentlyRead += source.get();
    }
    wcerr << recentlyRead << endl;
    wcerr << arr << L"^^^" << endl;
  }
  exit(EXIT_FAILURE);
}

void
RTXCompiler::eatSpaces()
{
  wchar_t c;
  bool inComment = false;
  while(!source.eof())
  {
    c = source.peek();
    if(c == L'\n')
    {
      source.get();
      inComment = false;
      currentLine++;
      recentlyRead.clear();
    }
    else if(inComment)
    {
      recentlyRead += source.get();
    }
    else if(isspace(c))
    {
      recentlyRead += source.get();
    }
    else if(c == L'!')
    {
      recentlyRead += source.get();
      inComment = true;
    }
    else
    {
      break;
    }
  }
}

wstring
RTXCompiler::nextTokenNoSpace()
{
  if(source.eof())
  {
    die(L"Unexpected end of file");
  }
  wchar_t c = source.get();
  wchar_t next = source.peek();
  wstring ret;
  if(c == L'→')
  {
    recentlyRead += c;
    ret = L"->";
  }
  else if(SPECIAL_CHARS.find(c) != string::npos)
  {
    ret = wstring(1, c);
    recentlyRead += c;
  }
  else if(c == L'-' && next == L'>')
  {
    next = source.get();
    ret = wstring(1, c) + wstring(1, next);
    recentlyRead += ret;
  }
  else if(isspace(c))
  {
    die(L"unexpected space");
  }
  else if(c == L'!')
  {
    die(L"unexpected comment");
  }
  else
  {
    ret = wstring(1, c);
    while(!source.eof())
    {
      c = source.peek();
      if(c == L'\\')
      {
        ret += source.get();
        ret += source.get();
      }
      else if(SPECIAL_CHARS.find(c) == string::npos && !isspace(c))
      {
        ret += wstring(1, source.get());
      }
      else
      {
        break;
      }
    }
    recentlyRead += ret;
  }
  return ret;
}

bool
RTXCompiler::isNextToken(wchar_t c)
{
  if(source.peek() == c)
  {
    recentlyRead += source.get();
    return true;
  }
  return false;
}

wstring
RTXCompiler::nextToken(wstring check1 = L"", wstring check2 = L"")
{
  eatSpaces();
  wstring tok = nextTokenNoSpace();
  if(tok == check1 || tok == check2 || (check1 == L"" && check2 == L""))
  {
  }
  else if(check1 != L"" && check2 != L"")
  {
    die(L"expected '" + check1 + L"' or '" + check2 + L"', found '" + tok + L"'");
  }
  else if(check1 != L"")
  {
    die(L"expected '" + check1 + L"', found '" + tok + L"'");
  }
  else
  {
    die(L"expected '" + check2 + L"', found '" + tok + L"'");
  }
  return tok;
}

wstring
RTXCompiler::parseIdent(bool prespace = false)
{
  if(prespace)
  {
    eatSpaces();
  }
  wstring ret = nextTokenNoSpace();
  if(ret == L"->" || (ret.size() == 1 && SPECIAL_CHARS.find(ret[0]) != string::npos))
  {
    die(L"expected identifier, found '" + ret + L"'");
  }
  return ret;
}

unsigned int
RTXCompiler::parseInt()
{
  wstring ret;
  while(isdigit(source.peek()))
  {
    ret += source.get();
  }
  recentlyRead += ret;
  return stoul(ret);
}

float
RTXCompiler::parseWeight()
{
  wstring ret;
  while(isdigit(source.peek()) || source.peek() == L'.')
  {
    ret += source.get();
  }
  recentlyRead += ret;
  try
  {
    wstring::size_type loc;
    float r = stof(ret, &loc);
    if(loc == ret.size())
    {
      return r;
    }
    else
    {
      die(L"unable to parse weight: " + ret);
    }
  }
  catch(const invalid_argument& ia)
  {
    die(L"unable to parse weight: " + ret);
  }
}

void
RTXCompiler::parseRule()
{
  wstring firstLabel = parseIdent();
  wstring next = nextToken();
  if(next == L":")
  {
    parseOutputRule(firstLabel);
  }
  else if(next == L">")
  {
    parseRetagRule(firstLabel);
  }
  else if(next == L"=")
  {
    parseAttrRule(firstLabel);
  }
  else
  {
    parseReduceRule(firstLabel, next);
  }
}

void
RTXCompiler::parseOutputRule(wstring pattern)
{
  nodeIsSurface[pattern] = !isNextToken(L':');
  vector<wstring> output;
  wstring cur;
  while(!source.eof())
  {
    cur = nextToken();
    if(cur == L"<")
    {
      cur = cur + parseIdent();
      cur += nextToken(L">");
    }
    output.push_back(cur);
    if(nextToken(L".", L";") == L";")
    {
      break;
    }
  }
  if(output.size() == 0)
  {
    die(L"empty tag order rule");
  }
  outputRules[pattern] = output;
}

void
RTXCompiler::parseRetagRule(wstring srcTag)
{
  wstring destTag = parseIdent(true);
  nextToken(L":");
  vector<pair<wstring, wstring>> rule;
  rule.push_back(pair<wstring, wstring>(srcTag, destTag));
  while(!source.eof())
  {
    wstring cs = parseIdent(true);
    wstring cd = parseIdent(true);
    rule.push_back(pair<wstring, wstring>(cs, cd));
    if(nextToken(L";", L",") == L";")
    {
      break;
    }
  }
  retagRules.push_back(rule);
  if(altAttrs.find(destTag) == altAttrs.end())
  {
    altAttrs[destTag].push_back(destTag);
  }
  altAttrs[destTag].push_back(srcTag);
}

void
RTXCompiler::parseAttrRule(wstring categoryName)
{
  if(collections.find(categoryName) != collections.end())
  {
    die(L"redefinition of category " + categoryName);
  }
  eatSpaces();
  if(isNextToken(L'('))
  {
    wstring undef = parseIdent(true);
    wstring def = parseIdent(true);
    attrDefaults[categoryName] = make_pair(undef, def);
    nextToken(L")");
  }
  vector<wstring> members;
  vector<wstring> noOver;
  while(true)
  {
    eatSpaces();
    if(isNextToken(L';'))
    {
      break;
    }
    if(isNextToken(L'@'))
    {
      wstring next = parseIdent();
      members.push_back(next);
      noOver.push_back(next);
    }
    else
    {
      members.push_back(parseIdent());
    }
  }
  if(members.size() == 0)
  {
    die(L"empty attribute list");
  }
  collections.insert(pair<wstring, vector<wstring>>(categoryName, members));
  noOverwrite.insert(pair<wstring, vector<wstring>>(categoryName, noOver));
}

RTXCompiler::Clip*
RTXCompiler::parseClip(int src = -2)
{
  Clip* ret = new Clip;
  eatSpaces();
  if(src != -2)
  {
    ret->src = src;
  }
  else if(isdigit(source.peek()))
  {
    ret->src = parseInt();
    nextToken(L".");
  }
  else if(isNextToken(L'$'))
  {
    ret->src = -1;
  }
  else
  {
    ret->src = 0;
  }
  ret->part = parseIdent();
  if(isNextToken(L'/'))
  {
    if(ret->src == 0)
    {
      die(L"literal value cannot have a side");
    }
    else if(ret->src == -1)
    {
      die(L"variable cannot have a side");
    }
    ret->side = parseIdent();
  }
  if(isNextToken(L'>'))
  {
    if(ret->src == 0)
    {
      die(L"literal value cannot be rewritten");
    }
    else if(ret->src == -1)
    {
      die(L"variable cannot be rewritten");
    }
    ret->rewrite = parseIdent();
  }
  return ret;
}

RTXCompiler::Cond*
RTXCompiler::parseCond()
{
  Cond* ret = new Cond;
  ret->op = 0;
  nextToken(L"(");
  eatSpaces();
  if(isNextToken(L'~'))
  {
    Cond* left = new Cond;
    left->op = NOT;
    left->right = parseCond();
    ret->left = left;
  }
  else if(!source.eof() && source.peek() == L'(')
  {
    ret->left = parseCond();
  }
  else
  {
    Cond* left = new Cond;
    left->op = 0;
    left->val = parseClip();
    ret->left = left;
  }
  while(true)
  {
    bool isNotted = false;
    wstring op = nextToken();
    if(op == L")")
    {
      return ret->left;
    }
    else
    {
      if(op == L"not")
      {
        isNotted = true;
        op = nextToken();
      }
      bool found = false;
      wstring key = StringUtils::tolower(op);
      key = StringUtils::substitute(key, L"-", L"");
      key = StringUtils::substitute(key, L"_", L"");
      for(unsigned int i = 0; i < OPERATORS.size(); i++)
      {
        if(key == OPERATORS[i].first)
        {
          ret->op = OPERATORS[i].second;
          found = true;
          break;
        }
      }
      if(!found)
      {
        die(L"unknown operator '" + op + L"'");
      }
    }
    eatSpaces();
    if(!source.eof() && source.peek() == L'(')
    {
      ret->right = parseCond();
    }
    else if(isNextToken(L'~'))
    {
      eatSpaces();
      ret->right = new Cond;
      ret->right->op = NOT;
      ret->right->right = parseCond();
    }
    else
    {
      ret->right = new Cond;
      ret->right->op = 0;
      ret->right->val = parseClip();
    }
    Cond* temp = ret;
    if(isNotted)
    {
      ret = new Cond;
      ret->op = NOT;
      ret->right = temp;
      temp = ret;
    }
    ret = new Cond;
    ret->left = temp;
  }
}

void
RTXCompiler::parsePatternElement(Rule* rule)
{
  vector<wstring> pat;
  if(isNextToken(L'%'))
  {
    rule->grab_all = rule->pattern.size()+1;
  }
  wstring t1 = nextToken();
  if(t1 == L"$")
  {
    t1 += parseIdent();
  }
  if(isNextToken(L'@'))
  {
    pat.push_back(t1);
    pat.push_back(parseIdent());
  }
  else if(t1[0] == L'$')
  {
    die(L"first tag in pattern element must be literal");
  }
  else
  {
    pat.push_back(L"");
    pat.push_back(t1);
  }
  while(!source.eof())
  {
    if(!isNextToken(L'.'))
    {
      break;
    }
    wstring cur = nextToken();
    if(cur == L"$")
    {
      Clip* cl = parseClip(rule->pattern.size()+1);
      if(rule->vars.find(cl->part) != rule->vars.end())
      {
        die(L"rule has multiple sources for attribute " + cl->part);
      }
      rule->vars[cl->part] = cl;
    }
    else
    {
      pat.push_back(cur);
    }
  }
  rule->pattern.push_back(pat);
  eatSpaces();
}

void
RTXCompiler::parseOutputElement()
{
  OutputChunk* ret = new OutputChunk;
  ret->conjoined = isNextToken(L'+');
  ret->nextConjoined = false;
  if(ret->conjoined)
  {
    if(currentChunk == NULL)
    {
      die(L"Cannot conjoin from within if statement.");
    }
    if(currentChunk->children.size() == 0)
    {
      die(L"Cannot conjoin first element.");
    }
    if(currentChunk->children.back()->conds.size() > 0)
    {
      die(L"Cannot conjoin to something in an if statement.");
    }
    if(currentChunk->children.back()->chunks.size() == 0)
    {
      die(L"Cannot conjoin inside and outside of if statement and cannot conjoin first element.");
    }
    if(currentChunk->children.back()->chunks[0]->mode == L"_")
    {
      die(L"Cannot conjoin to a blank.");
    }
    eatSpaces();
    currentChunk->children.back()->chunks[0]->nextConjoined = true;
  }
  ret->getall = isNextToken(L'%');
  if(source.peek() == L'_')
  {
    if(ret->getall)
    {
      die(L"% cannot be used on blanks");
    }
    ret->mode = L"_";
    recentlyRead += source.get();
    if(isdigit(source.peek()))
    {
      ret->pos = parseInt();
      if(currentRule->pattern.size() == 1)
      {
        die(L"Cannot output indexed blank because pattern is one element long and thus does not include blanks.");
      }
      if(ret->pos < 1 || ret->pos >= currentRule->pattern.size())
      {
        die(L"Position index of blank out of bounds, expected an integer from 1 to " + to_wstring(currentRule->pattern.size()-1) + L".");
      }
    }
    else
    {
      ret->pos = 0;
    }
  }
  else if(isdigit(source.peek()))
  {
    ret->mode = L"#";
    ret->pos = parseInt();
    if(ret->pos == 0)
    {
      die(L"There is no position 0.");
    }
    else if(ret->pos > currentRule->pattern.size())
    {
      die(L"There are only " + to_wstring(currentRule->pattern.size()) + L" elements in the pattern.");
    }
    if(source.peek() == L'[')
    {
      nextToken(L"[");
      ret->pattern = parseIdent();
      nextToken(L"]");
    }
    else
    {
      ret->pattern = currentRule->pattern[ret->pos-1][1];
    }
  }
  else
  {
    if(ret->getall)
    {
      die(L"% not currently supported on output literals");
    }
    ret->lemma = parseIdent();
    ret->mode = nextToken(L"@");
    while(true)
    {
      wstring cur = nextToken();
      wstring var = to_wstring(ret->tags.size());
      ret->tags.push_back(var);
      Clip* cl = new Clip;
      if(cur == L"$")
      {
        cl->src = -1;
        cl->part = parseIdent();
      }
      else if(cur == L"[")
      {
        cl = parseClip();
        nextToken(L"]");
      }
      else
      {
        cl->src = 0;
        cl->part = cur;
      }
      ret->vars[var] = cl;
      if(!isNextToken(L'.'))
      {
        break;
      }
    }
  }
  if(isNextToken(L'('))
  {
    while(!source.eof() && source.peek() != L')')
    {
      eatSpaces();
      wstring var = parseIdent();
      nextToken(L"=");
      eatSpaces();
      Clip* cl = parseClip();
      if(cl->part == L"_")
      {
        cl->part = L"";
      }
      ret->vars[var] = cl;
      if(nextToken(L",", L")") == L")")
      {
        break;
      }
    }
  }
  if(currentChoice != NULL)
  {
    currentChoice->chunks.push_back(ret);
    currentChoice->nest.push_back(NULL);
  }
  else
  {
    OutputChoice* temp = new OutputChoice;
    temp->chunks.push_back(ret);
    temp->nest.push_back(NULL);
    if(currentChunk == NULL)
    {
      currentRule->output.push_back(temp);
    }
    else
    {
      currentChunk->children.push_back(temp);
    }
  }
  eatSpaces();
}

void
RTXCompiler::parseOutputCond()
{
  nextToken(L"(");
  OutputChoice* choicewas = currentChoice;
  OutputChunk* chunkwas = currentChunk;
  OutputChoice* ret = new OutputChoice;
  currentChoice = ret;
  currentChunk = NULL;
  while(true)
  {
    wstring mode = StringUtils::tolower(nextToken());
    mode = StringUtils::substitute(mode, L"-", L"");
    mode = StringUtils::substitute(mode, L"_", L"");
    if(ret->conds.size() == 0 && mode != L"if")
    {
      die(L"If statement must begin with 'if'.");
    }
    if(mode == L"if" || mode == L"elif" || mode == L"elseif")
    {
      ret->conds.push_back(parseCond());
    }
    else if(mode != L"else" && mode != L"otherwise")
    {
      die(L"Unknown statement: '" + mode + L"'.");
    }
    eatSpaces();
    if(source.peek() == L'(')
    {
      parseOutputCond();
      ret->chunks.push_back(NULL);
    }
    else if(source.peek() == L'{')
    {
      if(parserIsInChunk)
      {
        die(L"Nested chunks are currently not allowed.");
      }
      ret->nest.push_back(NULL);
      parseOutputChunk();
    }
    else
    {
      if(!parserIsInChunk)
      {
        die(L"Conditional non-chunk output current not possible.");
      }
      parseOutputElement();
      ret->nest.push_back(NULL);
    }
    if(mode == L"else" || mode == L"otherwise")
    {
      break;
    }
  }
  currentChunk = chunkwas;
  currentChoice = choicewas;
  nextToken(L")");
  if(ret->conds.size() == 0)
  {
    die(L"If statement cannot be empty.");
  }
  if(ret->conds.size() == ret->nest.size())
  {
    die(L"If statement has no else clause and thus could produce no output.");
  }
  eatSpaces();
  if(currentChoice != NULL)
  {
    currentChoice->nest.push_back(ret);
    currentChoice->chunks.push_back(NULL);
  }
  else if(currentChunk != NULL)
  {
    currentChunk->children.push_back(ret);
  }
  else
  {
    currentRule->output.push_back(ret);
  }
}

void
RTXCompiler::parseOutputChunk()
{
  nextToken(L"{");
  parserIsInChunk = true;
  eatSpaces();
  OutputChunk* ch = new OutputChunk;
  OutputChunk* chunkwas = currentChunk;
  OutputChoice* choicewas = currentChoice;
  currentChunk = ch;
  currentChoice = NULL;
  ch->mode = L"{}";
  ch->pos = 0;
  while(source.peek() != L'}')
  {
    if(source.peek() == L'(')
    {
      parseOutputCond();
    }
    else
    {
      parseOutputElement();
    }
  }
  nextToken(L"}");
  parserIsInChunk = false;
  eatSpaces();
  currentChunk = chunkwas;
  currentChoice = choicewas;
  OutputChoice* ret = new OutputChoice;
  ret->chunks.push_back(ch);
  ret->nest.push_back(NULL);
  if(currentChoice != NULL)
  {
    currentChoice->chunks.push_back(ch);
  }
  else if(currentChunk != NULL)
  {
    currentChunk->children.push_back(ret);
  }
  else
  {
    currentRule->output.push_back(ret);
  }
}

void
RTXCompiler::parseReduceRule(wstring output, wstring next)
{
  vector<wstring> outNodes;
  outNodes.push_back(output);
  if(next != L"->")
  {
    wstring cur = next;
    while(cur != L"->")
    {
      if(SPECIAL_CHARS.find(cur) != wstring::npos)
      {
        die(L"Chunk names must be identifiers. (I think I'm parsing a reduction rule.)");
      }
      outNodes.push_back(cur);
      cur = nextToken();
    }
  }
  Rule* rule;
  while(true)
  {
    rule = new Rule();
    currentRule = rule;
    rule->grab_all = -1;
    rule->result = outNodes;
    eatSpaces();
    rule->line = currentLine;
    if(isdigit(source.peek()))
    {
      rule->weight = parseWeight();
      nextToken(L":");
      eatSpaces();
    }
    else
    {
      rule->weight = 0;
    }
    while(!source.eof() && source.peek() != L'{' && source.peek() != L'[' && source.peek() != L'(' && source.peek() != L'?')
    {
      parsePatternElement(rule);
    }
    if(rule->pattern.size() == 0)
    {
      die(L"empty pattern");
    }
    eatSpaces();
    if(isNextToken(L'?'))
    {
      rule->cond = parseCond();
      eatSpaces();
    }
    if(isNextToken(L'['))
    {
      while(!source.eof())
      {
        nextToken(L"$");
        wstring var = parseIdent();
        if(rule->vars.find(var) != rule->vars.end())
        {
          die(L"rule has multiple sources for attribute " + var);
        }
        nextToken(L"=");
        rule->vars[var] = parseClip();
        if(nextToken(L",", L"]") == L"]")
        {
          break;
        }
      }
      eatSpaces();
    }
    if(rule->result.size() > 1)
    {
      nextToken(L"{");
    }
    int chunk_count = 0;
    while(chunk_count < rule->result.size())
    {
      eatSpaces();
      if(source.eof()) die(L"Unexpected end of file.");
      switch(source.peek())
      {
        case L'(':
          parseOutputCond();
          chunk_count++;
          break;
        case L'{':
          parseOutputChunk();
          chunk_count++;
          break;
        case L'_':
          parseOutputElement();
          break;
        case L'}':
          if(rule->result.size() == 1)
          {
            die(L"Unexpected } in output pattern.");
          }
          else if(chunk_count < rule->result.size())
          {
            die(L"Output pattern does not have enough chunks.");
          }
          break;
        default:
          parseOutputElement();
          chunk_count++;
          break;
      }
    }
    if(rule->result.size() > 1)
    {
      nextToken(L"}");
    }
    reductionRules.push_back(rule);
    if(nextToken(L"|", L";") == L";")
    {
      break;
    }
  }
}

int
RTXCompiler::insertLemma(int const base, wstring const &lemma)
{
  int retval = base;
  static int const any_char = td.getAlphabet()(ANY_CHAR);
  if(lemma == L"")
  {
    retval = td.getTransducer().insertSingleTransduction(any_char, retval);
    td.getTransducer().linkStates(retval, retval, any_char);
    //int another = td.getTransducer().insertSingleTransduction(L'\\', retval);
    //td.getTransducer().linkStates(another, retval, any_char);
  }
  else
  {
    for(unsigned int i = 0, limit = lemma.size();  i != limit; i++)
    {
      if(lemma[i] == L'\\')
      {
        //retval = td.getTransducer().insertSingleTransduction(L'\\', retval);
        i++;
        retval = td.getTransducer().insertSingleTransduction(int(lemma[i]),
                                                             retval);
      }
      else if(lemma[i] == L'*')
      {
        retval = td.getTransducer().insertSingleTransduction(any_char, retval);
        td.getTransducer().linkStates(retval, retval, any_char);
      }
      else
      {
        retval = td.getTransducer().insertSingleTransduction(int(lemma[i]),
                                                             retval);
      }
    }
  }

  return retval;
}

int
RTXCompiler::insertTags(int const base, wstring const &tags)
{
  int retval = base;
  static int const any_tag = td.getAlphabet()(ANY_TAG);
  if(tags.size() != 0)
  {
    for(unsigned int i = 0, limit = tags.size(); i < limit; i++)
    {
      if(tags[i] == L'*')
      {
        retval = td.getTransducer().insertSingleTransduction(any_tag, retval);
        td.getTransducer().linkStates(retval, retval, any_tag);
        i++;
      }
      else
      {
        wstring symbol = L"<";
        for(unsigned int j = i; j != limit; j++)
        {
          if(tags[j] == L'.')
          {
            symbol.append(tags.substr(i, j-i));
            i = j;
            break;
          }
        }

        if(symbol == L"<")
        {
          symbol.append(tags.substr(i));
          i = limit;
        }
        symbol += L'>';
        td.getAlphabet().includeSymbol(symbol);
        retval = td.getTransducer().insertSingleTransduction(td.getAlphabet()(symbol), retval);
      }
    }
  }
  else
  {
    return base; // new line
  }

  return retval;
}

void
RTXCompiler::makePattern(int ruleid)
{
  int epsilon = td.getAlphabet()(0, 0);
  Rule* rule = reductionRules[ruleid];
  if(rule->pattern.size() > longestPattern)
  {
    longestPattern = rule->pattern.size();
  }
  int loc = td.getTransducer().getInitial();
  vector<wstring> pat;
  for(unsigned int i = 0; i < rule->pattern.size(); i++)
  {
    if(i != 0)
    {
      loc = td.getTransducer().insertSingleTransduction(L' ', loc);
      //td.getTransducer().linkStates(loc, loc, td.getAlphabet()(L" "));
    }
    loc = td.getTransducer().insertSingleTransduction(L'^', loc);
    pat = rule->pattern[i];
    if(pat[0].size() > 0 && pat[0][0] == L'$')
    {
      int lemend;
      int tmp = loc;
      vector<wstring> lems = collections[pat[0].substr(1)];
      for(unsigned int l = 0; l < lems.size(); l++)
      {
        lemend = insertLemma(tmp, lems[l]);
        if(l == 0)
        {
          loc = td.getTransducer().insertSingleTransduction(epsilon, lemend);
        }
        else
        {
          td.getTransducer().linkStates(lemend, loc, epsilon);
        }
      }
    }
    else
    {
      loc = insertLemma(loc, pat[0]);
    }
    wstring tags;
    for(unsigned int t = 1; t < pat.size(); t++)
    {
      if(t != 1)
      {
        tags += L'.';
      }
      tags += pat[t];
    }
    loc = insertTags(loc, tags);
    td.getTransducer().linkStates(loc, loc, td.getAlphabet()(ANY_TAG));
    loc = td.getTransducer().insertSingleTransduction(L'$', loc);
  }
  const int symbol = td.countToFinalSymbol(ruleid+1);
  loc = td.getTransducer().insertSingleTransduction(symbol, loc, rule->weight);
  td.getTransducer().setFinal(loc);
}

wstring
RTXCompiler::compileString(wstring s)
{
  wstring ret;
  ret += STRING;
  ret += (wchar_t)s.size();
  ret += s;
  return ret;
}

wstring
RTXCompiler::compileTag(wstring s)
{
  if(s.size() == 0)
  {
    return compileString(s);
  }
  wstring tag;
  tag += L'<';
  tag += s;
  tag += L'>';
  return compileString(tag);
}

wstring
RTXCompiler::compileClip(Clip* c, wstring _dest = L"")
{
  int src = (c->src == -1) ? 0 : c->src;
  bool useReplace = inOutputRule;
  wstring dest;
  if(inOutputRule && _dest.size() > 0)
  {
    dest = _dest;
  }
  else if(c->rewrite.size() > 0)
  {
    dest = c->rewrite;
  }
  wstring cl = (c->part == L"lemcase") ? compileString(L"lem") : compileString(c->part);
  cl += INT;
  cl += src;
  wstring ret = cl;
  if(c->src == 0)
  {
    return compileTag(c->part);
  }
  else if(c->side == L"sl")
  {
    ret += SOURCECLIP;
  }
  else if(c->side == L"ref")
  {
    ret += REFERENCECLIP;
  }
  else if(c->side == L"tl" || c->part == L"lemcase" ||
          (c->src != -1 && !nodeIsSurface[currentRule->pattern[c->src-1][1]]))
  {
    ret += TARGETCLIP;
  }
  else
  {
    wstring undeftag;
    wstring deftag;
    wstring thedefault;
    wstring blank;
    blank += DUP;
    blank += compileString(L"");
    blank += EQUAL;
    if(useReplace && undeftag.size() > 0)
    {
      blank += OVER;
      blank += compileTag(undeftag);
      blank += EQUAL;
      blank += OR;
    }
    if(attrDefaults.find(c->part) != attrDefaults.end())
    {
      undeftag = attrDefaults[c->part].first;
      deftag = attrDefaults[c->part].second;
      thedefault += DROP;
      thedefault += compileTag(useReplace ? deftag : undeftag);
      if(useReplace)
      {
        blank += OVER;
        blank += compileTag(undeftag);
        blank += EQUAL;
        blank += OR;
      }
    }
    blank += JUMPONFALSE;
    ret += TARGETCLIP;
    ret += blank;
    ret += (wchar_t)(6 + 2*cl.size() + 2*blank.size() + thedefault.size());
    ret += DROP;
    ret += cl;
    ret += REFERENCECLIP;
    ret += blank;
    ret += (wchar_t)(3 + cl.size() + blank.size() + thedefault.size());
    ret += DROP;
    ret += cl;
    ret += SOURCECLIP;
    ret += blank;
    ret += (wchar_t)thedefault.size();
    ret += thedefault;
  }
  if(c->part == L"lemcase")
  {
    ret += GETCASE;
  }
  if(dest.size() > 0)
  {
    bool found = false;
    vector<pair<wstring, wstring>> rule;
    for(unsigned int i = 0; i < retagRules.size(); i++)
    {
      if(retagRules[i][0].first == c->part && retagRules[i][0].second == dest)
      {
        found = true;
        rule = retagRules[i];
        break;
      }
    }
    if(!found && dest != c->part)
    {
      die(L"There is no tag-rewrite rule from '" + c->part + L"' to '" + dest + L"'.");
    }
    wstring check;
    for(unsigned int i = 1; i < rule.size(); i++)
    {
      wstring cur;
      cur += DUP;
      cur += compileTag(rule[i].first);
      cur += EQUAL;
      cur += JUMPONFALSE;
      cur += (wchar_t)(rule[i].second.size() + (i == 1 ? 5 : 7));
      cur += DROP;
      cur += compileTag(rule[i].second);
      if(i != 1)
      {
        cur += JUMP;
        cur += (wchar_t)check.size();
      }
      check = cur + check;
    }
    ret += check;
  }
  return ret;
}

wstring
RTXCompiler::compileClip(wstring part, int pos, wstring side = L"")
{
  Clip cl;
  cl.part = part;
  cl.src = pos;
  cl.side = side;
  return compileClip(&cl);
}

wstring
RTXCompiler::processOutputChunk(OutputChunk* r)
{
  wstring ret;
  if(r->mode == L"_")
  {
    ret += INT;
    ret += (wchar_t)r->pos;
    ret += BLANK;
  }
  else if(r->mode == L"{}")
  {
    for(unsigned int i = 0; i < r->children.size(); i++)
    {
      ret += processOutputChoice(r->children[i]);
      if(r->children[i]->chunks.size() == 1 && r->children[i]->chunks[0]->nextConjoined)
      {
      }
      else
      {
        ret += OUTPUT;
      }
    }
  }
  else if(r->mode == L"#")
  {
    wstring pos;
    if(r->pos != 0)
    {
      if(currentRule->pattern[r->pos-1].size() < 2)
      {
        die(L"could not find tag order for element " + to_wstring(r->pos));
      }
      wstring pos = currentRule->pattern[r->pos-1][1];
    }
    wstring patname = (r->pattern != L"") ? r->pattern : pos;
    pos = (pos != L"") ? pos : patname;
    if(outputRules.find(patname) == outputRules.end())
    {
      die(L"could not find output pattern '" + patname + L"'");
    }
    vector<wstring> pattern = outputRules[patname];

    if(r->getall)
    {
      for(unsigned int i = 0; i < parentTags.size(); i++)
      {
        if(r->vars.find(parentTags[i]) == r->vars.end())
        {
          Clip* cl = new Clip;
          cl->src = -1;
          cl->part = parentTags[i];
          r->vars[parentTags[i]] = cl;
        }
      }
    }

    if(r->conjoined)
    {
      ret += compileString(L"+");
      ret += APPENDSURFACE;
    }
    else
    {
      ret += CHUNK;
    }
    for(unsigned int i = 0; i < pattern.size(); i++)
    {
      if(pattern[i] == L"_")
      {
        if(r->vars.find(L"lem") != r->vars.end())
        {
          ret += compileClip(r->vars[L"lem"], L"lem");
        }
        else if(r->vars.find(L"lemh") != r->vars.end())
        {
          ret += compileClip(r->vars[L"lemh"], L"lemh");
        }
        else if(r->mode == L"{}")
        {
          ret += compileString(L"unknown");
        }
        else if(r->pos == 0)
        {
          ret += compileString(L"unknown");
        }
        else
        {
          Clip* c = new Clip;
          c->part = L"lemh";
          c->src = r->pos;
          c->side = L"tl";
          c->rewrite = L"lemh";
          ret += compileClip(c);
        }
        if(r->vars.find(L"lemcase") != r->vars.end())
        {
          ret += compileClip(r->vars[L"lemcase"], L"lemcase");
          ret += SETCASE;
        }
        ret += APPENDSURFACE;
        if(r->pos != 0)
        {
          //ret += compileTag(currentRule->pattern[r->pos-1][1]);
          ret += compileClip(L"pos_tag", r->pos, L"tl");
        }
        else
        {
          ret += compileTag(pos);
        }
        ret += APPENDSURFACE;
      }
      else if(pattern[i][0] == L'<')
      {
        ret += compileString(pattern[i]);
        ret += APPENDSURFACE;
      }
      else
      {
        vector<wstring> ops = altAttrs[pattern[i]];
        if(ops.size() == 0)
        {
          ops.push_back(pattern[i]);
        }
        wstring var;
        for(unsigned int v = 0; v < ops.size(); v++)
        {
          if(r->vars.find(ops[v]) != r->vars.end())
          {
            var = ops[v];
            break;
          }
        }
        if(var == L"" && r->pos != 0)
        {
          Clip* cl = new Clip;
          cl->src = r->pos;
          cl->part = pattern[i];
          ret += compileClip(cl, pattern[i]);
        }
        else if(var == L"")
        {
          bool found = false;
          for(unsigned int t = 0; t < parentTags.size(); t++)
          {
            if(parentTags[t] == pattern[i])
            {
              ret += compileTag(to_wstring(t+1));
              found = true;
              break;
            }
          }
          if(!found)
          {
            if(attrDefaults.find(pattern[i]) != attrDefaults.end())
            {
              ret += compileTag(attrDefaults[pattern[i]].first);
            }
            else
            {
              Clip* cl = new Clip;
              cl->src = r->pos;
              cl->part = pattern[i];
              if(r->pos == 0)
              {
                if(currentRule->grab_all == -1)
                {
                  die(L"cannot find source for tag '" + pattern[i] + L"'");
                }
                else
                {
                  cl->src = currentRule->grab_all;
                }
              }
              ret += compileClip(cl, pattern[i]);
            }
          }
        }
        else
        {
          ret += compileClip(r->vars[var], pattern[i]);
        }
        ret += APPENDSURFACE;
      }
    }
    if(r->vars.find(L"lemq") != r->vars.end())
    {
      ret += compileClip(r->vars[L"lemq"]);
      ret += APPENDSURFACE;
    }
    else if(r->pos != 0)
    {
      ret += compileClip(L"lemq", r->pos, L"tl");
      ret += APPENDSURFACE;
    }
    if(r->mode == L"#" && inOutputRule)
    {
      ret += compileClip(L"whole", r->pos, L"tl");
      ret += APPENDALLCHILDREN;
      ret += INT;
      ret += (wchar_t)r->pos;
      ret += GETRULE;
      ret += INT;
      ret += (wchar_t)0;
      ret += SETRULE;
    }
  }
  else
  {
    ret += CHUNK;
    ret += compileString(r->lemma);
    ret += APPENDSURFACE;
    for(unsigned int i = 0; i < r->tags.size(); i++)
    {
      ret += compileClip(r->vars[r->tags[i]]);
      ret += APPENDSURFACE;
    }
  }
  return ret;
}

wstring
RTXCompiler::processCond(Cond* cond)
{
  wstring ret;
  if(cond == NULL)
  {
    ret += PUSHTRUE;
    return ret;
  }
  if(cond->op == 0)
  {
    if(cond->val->src == 0)
    {
      ret = compileString(cond->val->part);
    }
    else
    {
      ret = compileClip(cond->val);
      if(cond->val->part != L"lem" && cond->val->part != L"lemh" && cond->val->part != L"lemq")
      {
        ret += DISTAG;
      }
    }
  }
  else if(cond->op == NOT)
  {
    ret = processCond(cond->right);
    ret += NOT;
  }
  else
  {
    ret = processCond(cond->left);
    ret += processCond(cond->right);
    ret += cond->op;
  }
  return ret;
}

wstring
RTXCompiler::processOutputChoice(OutputChoice* choice)
{
  wstring ret;
  if(choice->nest.back() == NULL)
  {
    ret += processOutputChunk(choice->chunks.back());
  }
  else
  {
    ret += processOutputChoice(choice->nest.back());
  }
  int n = choice->conds.size();
  for(int i = 1; i <= n; i++)
  {
    wstring act;
    if(choice->nest[n-i] != NULL)
    {
      act = processOutputChoice(choice->nest[n-i]);
    }
    else
    {
      act = processOutputChunk(choice->chunks[n-i]);
    }
    act += JUMP;
    act += (wchar_t)ret.size();
    wstring cond = processCond(choice->conds[n-i]);
    cond += JUMPONFALSE;
    cond += (wchar_t)act.size();
    ret = cond + act + ret;
  }
  return ret;
}

void
RTXCompiler::processRules()
{
  Rule* rule;
  for(unsigned int ruleid = 0; ruleid < reductionRules.size(); ruleid++)
  {
    rule = reductionRules[ruleid];
    currentRule = rule;
    currentChunk = NULL;
    currentChoice = NULL;
    makePattern(ruleid);
    wstring comp;
    if(rule->cond != NULL)
    {
      comp = processCond(rule->cond) + JUMPONTRUE + (wchar_t)1 + REJECTRULE;
    }
    vector<wstring> outcomp;
    outcomp.resize(rule->pattern.size());
    parentTags.clear();
    int patidx = 0;
    for(unsigned int i = 0; i < rule->output.size(); i++)
    {
      OutputChoice* cur = rule->output[i];
      if(cur->chunks.size() == 1 && cur->chunks[0]->mode == L"_")
      {
        comp += processOutputChoice(cur);
      }
      else if(cur->chunks.size() == 1 && cur->chunks[0]->mode == L"#")
      {
        comp += processOutputChoice(cur);
        patidx++;
      }
      else
      {
        OutputChunk* ch = new OutputChunk;
        ch->mode = L"#";
        ch->pos = 0;
        ch->getall = true;
        ch->vars = rule->vars;
        ch->conjoined = false;
        ch->nextConjoined = false;
        ch->pattern = rule->result[patidx];
        comp += processOutputChunk(ch);
        comp += INT;
        comp += (wchar_t)outputBytecode.size();
        comp += INT;
        comp += (wchar_t)0;
        comp += SETRULE;
        comp += APPENDALLINPUT;
        parentTags = outputRules[ch->pattern];
        inOutputRule = true;
        outputBytecode.push_back(processOutputChoice(cur));
        inOutputRule = false;
        parentTags.clear();
        patidx++;
      }
      comp += OUTPUT;
    }
    rule->compiled = comp;
  }
}

void
RTXCompiler::read(const string &fname)
{
  currentLine = 1;
  sourceFile = fname;
  source.open(fname);
  while(true)
  {
    eatSpaces();
    if(source.eof())
    {
      break;
    }
    parseRule();
  }
  source.close();
  errorsAreSyntax = false;
  processRules();
  for(map<wstring, vector<wstring>>::iterator it=collections.begin(); it != collections.end(); ++it)
  {
    wstring regex = L"(";
    for(unsigned int l = 0; l < it->second.size(); l++)
    {
      td.getLists()[it->first].insert(it->second[l]);
      wstring attr = it->second[l];
      if(l != 0)
      {
        regex += L'|';
      }
      regex += L'<';
      for(unsigned int c = 0; c < attr.size(); c++)
      {
        if(attr[c] == L'.')
        {
          regex += L"><";
        }
        else
        {
          regex += attr[c];
        }
      }
      regex += L'>';
    }
    regex += L")";
    td.getAttrItems()[it->first] = regex;
  }
}

void
RTXCompiler::write(const string &fname, const string &bytename)
{
  FILE *out = fopen(fname.c_str(), "wb");
  if(!out)
  {
    cerr << "Error: cannot open '" << fname;
    cerr << "' for writing" << endl;
    exit(EXIT_FAILURE);
  }

  td.write(out);

  fclose(out);
  
  FILE *out2 = fopen(bytename.c_str(), "wb");
  if(!out)
  {
    cerr << "Error: cannot open '" << bytename;
    cerr << "' for writing" << endl;
    exit(EXIT_FAILURE);
  }
  fputwc(longestPattern, out2);
  fputwc(reductionRules.size(), out2);
  for(unsigned int i = 0; i < reductionRules.size(); i++)
  {
    fputwc(reductionRules[i]->compiled.size(), out2);
    fputwc((reductionRules[i]->pattern.size() * 2) - 1, out2);
    for(unsigned int c = 0; c < reductionRules[i]->compiled.size(); c++)
    {
      fputwc(reductionRules[i]->compiled[c], out2);
      // char by char because there might be \0s and that could be a problem?
    }
  }
  fputwc(outputBytecode.size(), out2);
  for(unsigned int i = 0; i < outputBytecode.size(); i++)
  {
    fputwc(outputBytecode[i].size(), out2);
    for(unsigned int c = 0; c < outputBytecode[i].size(); c++)
    {
      fputwc(outputBytecode[i][c], out2);
    }
  }
  fclose(out2);
}
