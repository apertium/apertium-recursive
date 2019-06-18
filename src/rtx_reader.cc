#include <rtx_reader.h>
#include <rtx_parser.h>
#include <bytecode.h>
#include <apertium/string_utils.h>
#include <vector>
#include <algorithm>
#include <iterator>

using namespace std;

wstring const
RTXReader::ANY_TAG = L"<ANY_TAG>";

wstring const
RTXReader::ANY_CHAR = L"<ANY_CHAR>";

RTXReader::RTXReader()
{
  td.getAlphabet().includeSymbol(ANY_TAG);
  td.getAlphabet().includeSymbol(ANY_CHAR);
  longestPattern = 0;
  currentRule = NULL;
  currentChunk = NULL;
  errorsAreSyntax = true;
}

wstring const RTXReader::SPECIAL_CHARS = L"!@$%()={}[]|/:;<>,.~→";

void
RTXReader::die(wstring message)
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
RTXReader::eatSpaces()
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
RTXReader::nextTokenNoSpace()
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
RTXReader::isNextToken(wchar_t c)
{
  if(source.peek() == c)
  {
    recentlyRead += source.get();
    return true;
  }
  return false;
}

wstring
RTXReader::nextToken(wstring check1 = L"", wstring check2 = L"")
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
RTXReader::parseIdent(bool prespace = false)
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

int
RTXReader::parseInt()
{
  wstring ret;
  while(isdigit(source.peek()))
  {
    ret += source.get();
  }
  recentlyRead += ret;
  return stoi(ret);
}

float
RTXReader::parseWeight()
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
RTXReader::parseRule()
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
RTXReader::parseOutputRule(wstring pattern)
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
RTXReader::parseRetagRule(wstring srcTag)
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
RTXReader::parseAttrRule(wstring categoryName)
{
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

RTXReader::Clip*
RTXReader::parseClip(int src = -2)
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
    ret->side = parseIdent();
  }
  return ret;
}

RTXReader::Cond*
RTXReader::parseCond()
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
    wstring op = nextToken();
    if(op == L")")
    {
      return ret->left;
    }
    else
    {
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
    ret = new Cond;
    ret->left = temp;
  }
}

void
RTXReader::parsePatternElement(Rule* rule)
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
RTXReader::parseOutputElement()
{
  OutputChunk* ret = new OutputChunk;
  ret->getall = isNextToken(L'%');
  ret->isToplevel = false;
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
      if(ret->pos < 1 || ret->pos >= currentRule->pattern.size())
      {
        die(L"position index of blank out of bounds");
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
    if(ret->pos < 1 || ret->pos > currentRule->pattern.size())
    {
      die(L"output index is out of bounds");
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
        cl->side = parseIdent();
      }
      else if(cur == L"[")
      {
        cl = parseClip();
        nextToken(L"]");
      }
      else
      {
        cl->src = 0;
        cl->side = cur;
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
  currentChunk->children.push_back(ret);
  eatSpaces();
}

void
RTXReader::parseOutputChunk(bool recursing = false)
{
  nextToken(L"{");
  eatSpaces();
  OutputChunk* ch = new OutputChunk;
  OutputChunk* was = currentChunk;
  currentChunk = ch;
  ch->mode = L"{}";
  ch->pos = 0;
  bool hasrec = false;
  while(source.peek() != L'}')
  {
    if(!recursing && source.peek() == L'{')
    {
      parseOutputChunk(true);
      hasrec = true;
    }
    else
    {
      parseOutputElement();
    }
  }
  nextToken(L"}");
  eatSpaces();
  Cond* cnd = NULL;
  if(!recursing && source.peek() == L'(')
  {
    cnd = parseCond();
  }
  eatSpaces();
  ch->isToplevel = !recursing;
  currentChunk = was;
  if(recursing)
  {
    currentChunk->children.push_back(ch);
  }
  else if(hasrec)
  {
    currentRule->output.push_back(make_pair(ch, cnd));
  }
  else
  {
    OutputChunk* ret = new OutputChunk;
    ret->mode = L"{}";
    ret->children.push_back(ch);
    currentRule->output.push_back(make_pair(ret, cnd));
  }
}

void
RTXReader::parseReduceRule(wstring output, wstring next)
{
  vector<wstring> outNodes;
  outNodes.push_back(output);
  if(next != L"->")
  {
    wstring cur = next;
    while(cur != L"->")
    {
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
    rule->line = currentLine;
    eatSpaces();
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
    while(!source.eof() && source.peek() != L'{' && source.peek() != L'[' && source.peek() != L'(')
    {
      parsePatternElement(rule);
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
    if(source.peek() == L'(')
    {
      rule->cond = parseCond();
      eatSpaces();
    }
    eatSpaces();
    while(!source.eof() && source.peek() == L'{')
    {
      parseOutputChunk();
      OutputChunk* ch = rule->output.back().first;
      int n = 0;
      for(unsigned int i = 0; i < ch->children.size(); i++)
      {
        OutputChunk* cur = ch->children[i];
        if(cur->mode == L"{}" || cur->pattern == outNodes[n])
        {
          if(n == outNodes.size())
          {
            die(L"too many chunks in output pattern");
          }
          cur->pattern = outNodes[n];
          for(map<wstring, Clip*>::iterator it = rule->vars.begin();
                  it != rule->vars.end(); ++it)
          {
            if(cur->vars.find(it->first) == cur->vars.end())
            {
              cur->vars[it->first] = it->second;
            }
          }
          n++;
        }
        cur->isToplevel = true;
      }
      if(n < outNodes.size())
      {
        die(L"not enough chunks in output pattern");
      }
    }
    reductionRules.push_back(rule);
    if(nextToken(L"|", L";") == L";")
    {
      break;
    }
  }
}

int
RTXReader::insertLemma(int const base, wstring const &lemma)
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
RTXReader::insertTags(int const base, wstring const &tags)
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
RTXReader::makePattern(int ruleid)
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
RTXReader::compileString(wstring s)
{
  wstring ret;
  ret += STRING;
  ret += (wchar_t)s.size();
  ret += s;
  return ret;
}

wstring
RTXReader::compileTag(wstring s)
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
RTXReader::compileClip(Clip* c)
{
  bool useReplace = (currentChunk == NULL) ? false : !currentChunk->isToplevel;
  wstring cl = (c->part == L"lemcase") ? compileString(L"lem") : compileString(c->part);
  cl += INT;
  cl += c->src;
  wstring ret = cl;
  if(c->src == -1)
  {
    for(unsigned int i = 0; i < parentTags.size(); i++)
    {
      if(parentTags[i] == c->part)
      {
        return compileTag(to_wstring(i+1));
      }
    }
    die(L"parent chunk has no attribute " + c->part);
  }
  else if(c->src == 0)
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
  else if(c->side == L"tl" || c->part == L"lemcase")
  {
    ret += TARGETCLIP;
  }
  else
  {
    wstring undeftag;
    wstring deftag;
    wstring undef;
    wstring def;
    if(attrDefaults.find(c->part) != attrDefaults.end())
    {
      undeftag = attrDefaults[c->part].first;
      deftag = attrDefaults[c->part].second;
      undef += DROP;
      undef += compileTag(undeftag);
      def += DROP;
      def += compileTag(deftag);
    }
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
    blank += JUMPONFALSE;
    if(c->fromChunk)
    {
      ret += TARGETCLIP;
      ret += blank;
      if(useReplace)
      {
        ret += (wchar_t)def.size();
        ret += def;
      }
      else
      {
        ret += (wchar_t)undef.size();
        ret += undef;
      }
    }
    else
    {
      int s = (useReplace ? def.size() : undef.size());
      ret += TARGETCLIP;
      ret += blank;
      ret += (wchar_t)(6 + 2*cl.size() + 2*blank.size() + s);
      ret += DROP;
      ret += cl;
      ret += REFERENCECLIP;
      ret += blank;
      ret += (wchar_t)(3 + cl.size() + blank.size() + s);
      ret += DROP;
      ret += cl;
      ret += SOURCECLIP;
      ret += blank;
      ret += (wchar_t)s;
      ret += (useReplace ? def : undef);
    }
  }
  if(c->part == L"lemcase")
  {
    ret += GETCASE;
  }
  return ret;
}

wstring
RTXReader::compileClip(wstring part, int pos, wstring side = L"", bool usereplace = false)
{
  wstring c = compileString(part);
  if(part == L"lemcase")
  {
    c = compileString(L"lem");
  }
  c += INT;
  c += pos;
  wstring ret = c;
  if(side == L"sl")
  {
    ret += SOURCECLIP;
  }
  else if(side == L"tl")
  {
    ret += TARGETCLIP;
  }
  else if(side == L"ref")
  {
    ret += REFERENCECLIP;
  }
  else if(!usereplace || attrDefaults.find(part) == attrDefaults.end())
  {
    wstring def;
    if(attrDefaults.find(part) != attrDefaults.end())
    {
      wstring tg = compileTag(attrDefaults[part].first);
      def += DUP;
      def += compileString(L"");
      def += EQUAL;
      def += JUMPONFALSE;
      def += (wchar_t)(tg.size() + 1);
      def += DROP;
      def += tg;
    }
    ret += TARGETCLIP;
    ret += DUP;
    ret += compileString(L"");
    ret += EQUAL;
    ret += JUMPONFALSE;
    ret += (2*c.size() + def.size() + 10);
    ret += DROP;
    ret += c;
    ret += REFERENCECLIP;
    ret += DUP;
    ret += compileString(L"");
    ret += EQUAL;
    ret += JUMPONFALSE;
    ret += (c.size() + def.size() + 2);
    ret += DROP;
    ret += c;
    ret += SOURCECLIP;
    ret += def;
  }
  else
  {
    wstring undef = attrDefaults[part].first;
    wstring repl = attrDefaults[part].second;
    wstring emp1 = compileString(L"") + EQUAL;
    wstring emp2 = compileTag(undef) + EQUAL;
    wstring emp = wstring(1, DUP) + emp1 + OVER + emp2 + OR;
    ret = wstring(1, DROP) + compileTag(repl);
    ret = emp + wstring(1, JUMPONFALSE) + (wchar_t)ret.size() + ret;
    ret = wstring(1, DROP) + c + SOURCECLIP + ret;
    ret = emp + wstring(1, JUMPONFALSE) + (wchar_t)ret.size() + ret;
    ret = wstring(1, DROP) + c + REFERENCECLIP + ret;
    ret = emp + wstring(1, JUMPONFALSE) + (wchar_t)ret.size() + ret;
    ret = c + TARGETCLIP + ret;
  }
  if(part == L"lemcase")
  {
    ret += GETCASE;
  }
  return ret;
}

wstring
RTXReader::processOutput(OutputChunk* r)
{
  wstring ret;
  if(r->mode == L"_")
  {
    ret += INT;
    ret += (wchar_t)r->pos;
    ret += BLANK;
  }
  else if(r->mode == L"{}" && r->pattern == L"")
  {
    OutputChunk* was = currentChunk;
    currentChunk = r;
    for(unsigned int i = 0; i < r->children.size(); i++)
    {
      ret += processOutput(r->children[i]);
      ret += OUTPUT;
    }
    currentChunk = was;
  }
  else if(r->mode == L"#" || r->mode == L"{}")
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
      for(map<wstring, Clip*>::iterator it = currentRule->vars.begin();
              it != currentRule->vars.end(); ++it)
      {
        if(r->vars.find(it->first) == r->vars.end())
        {
          Clip* cl = new Clip;
          cl->src = -1;
          cl->part = it->first;
          r->vars[it->first] = cl;
        }
      }
    }

    ret += CHUNK;
    for(unsigned int i = 0; i < pattern.size(); i++)
    {
      if(pattern[i] == L"_")
      {
        if(r->vars.find(L"lem") != r->vars.end())
        {
          ret += compileClip(r->vars[L"lem"]);
        }
        else if(r->vars.find(L"lemh") != r->vars.end())
        {
          ret += compileClip(r->vars[L"lemh"]);
        }
        else if(r->mode == L"{}")
        {
          ret += compileString(L"unknown");
        }
        else
        {
          ret += compileClip(L"lemh", r->pos, L"tl");
        }
        if(r->vars.find(L"lemcase") != r->vars.end())
        {
          ret += compileClip(r->vars[L"lemcase"]);
          ret += SETCASE;
        }
        ret += APPENDSURFACE;
        ret += compileTag(pos);
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
        if(var == L"")
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
            ret += compileClip(cl);
          }
        }
        else
        {
          ret += compileClip(r->vars[var]);
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
    if(r->mode == L"#")
    {
      ret += compileClip(L"whole", r->pos, L"tl");
      ret += APPENDALLCHILDREN;
    }
    else
    {
      vector<wstring> was = parentTags;
      parentTags = pattern;
      for(unsigned int i = 0; i < r->children.size(); i++)
      {
        ret += processOutput(r->children[i]);
        ret += APPENDCHILD;
      }
      parentTags = was;
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
RTXReader::processCond(Cond* cond)
{
  wstring ret;
  if(cond == NULL)
  {
    ret += PUSHTRUE;
    return ret;
  }
  if(cond->op == 0)
  {
    if(cond->val->src == -1)
    {
      die(L"conditionals cannot refer to output chunk tags");
    }
    else if(cond->val->src == 0)
    {
      ret = compileString(cond->val->part);
    }
    else
    {
      ret = compileClip(cond->val);
      if(cond->val->part != L"lem")
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

void
RTXReader::makeDefaultRule()
{
  Rule* rule = new Rule;
  rule->weight = -1.0;
  rule->compiled += compileString(L"yes");
  rule->compiled += compileString(L"foundany");
  rule->compiled += SETVAR;
  vector<wstring> tags;
  for(map<wstring, pair<wstring, wstring>>::iterator it = attrDefaults.begin();
          it != attrDefaults.end(); ++it)
  {
    wstring cat = it->first;
    wstring undef = it->second.first;
    tags.push_back(L"<" + undef + L">");
    wstring def = it->second.second;
    wstring set = compileTag(def) + compileString(cat);
    set += INT;
    set += (wchar_t)1;
    set += SETCLIP;
    set += compileString(L"yes");
    set += compileString(L"foundany");
    set += SETVAR;

    rule->compiled += compileClip(cat, 1, L"tl");
    rule->compiled += compileTag(undef);
    rule->compiled += EQUAL;
    rule->compiled += JUMPONFALSE;
    rule->compiled += (wchar_t)set.size();
    rule->compiled += set;
  }
  if(tags.size() == 0)
  {
    delete rule;
    return;
  }
  rule->compiled += compileString(L"yes");
  rule->compiled += compileString(L"foundany");
  rule->compiled += FETCHVAR;
  rule->compiled += EQUAL;
  rule->compiled += JUMPONTRUE;
  rule->compiled += (wchar_t)1;
  rule->compiled += REJECTRULE;
  rule->compiled += compileClip(L"whole", 1, L"tl");
  rule->compiled += OUTPUT;

  int loc = td.getTransducer().getInitial();
  loc = td.getTransducer().insertSingleTransduction(L'^', loc);
  loc = insertLemma(loc, L"");
  loc = insertTags(loc, L"*");
  int epsilon = td.getAlphabet()(0,0);
  int tmp;
  for(unsigned int i = 0; i < tags.size(); i++)
  {
    td.getAlphabet().includeSymbol(tags[i]);
    tmp = td.getTransducer().insertSingleTransduction(td.getAlphabet()(tags[i]), loc);
    if(i == 0)
    {
      // this will make the rule only apply if the first undefined tags is present
      // but doing otherwise results in a seeminly infinite loop
      // TODO TODO TODO
      loc = td.getTransducer().insertSingleTransduction(epsilon, tmp);
    }
    else
    {
      td.getTransducer().linkStates(tmp, loc, epsilon);
    }
  }
  td.getTransducer().linkStates(loc, loc, td.getAlphabet()(ANY_TAG));
  loc = td.getTransducer().insertSingleTransduction(L'$', loc);
  reductionRules.push_back(rule);
  const int symbol = td.countToFinalSymbol(reductionRules.size());
  loc = td.getTransducer().insertSingleTransduction(symbol, loc, rule->weight);
  td.getTransducer().setFinal(loc);
}

void
RTXReader::processRules()
{
  Rule* rule;
  for(unsigned int ruleid = 0; ruleid < reductionRules.size(); ruleid++)
  {
    rule = reductionRules[ruleid];
    currentRule = rule;
    makePattern(ruleid);
    wstring comp;
    parentTags.clear();
    for(vector<pair<OutputChunk*, Cond*>>::reverse_iterator it = rule->output.rbegin();
              it != rule->output.rend(); ++it)
    {
      int pl = 0;
      if(comp.size() > 0)
      {
        comp = wstring(1, JUMP) + (wchar_t)comp.size() + comp;
        pl = 2;
      }
      wstring out = processOutput(it->first);
      wstring cnd;
      if(it->second != NULL)
      {
        cnd = processCond(it->second) + JUMPONFALSE + (wchar_t)(out.size()+pl);
      }
      comp = cnd + out + comp;
    }
    if(rule->cond != NULL)
    {
      comp = processCond(rule->cond) + JUMPONFALSE + (wchar_t)1 + REJECTRULE + comp;
    }
    rule->compiled = comp;
  }
  if(attrDefaults.size() != 0)
  {
    makeDefaultRule();
  }
}

void
RTXReader::read(const string &fname)
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
RTXReader::write(const string &fname, const string &bytename)
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
    for(unsigned int c = 0; c < reductionRules[i]->compiled.size(); c++)
    {
      fputwc(reductionRules[i]->compiled[c], out2);
      // char by char because there might be \0s and that could be a problem?
    }
  }
  fclose(out2);
}
