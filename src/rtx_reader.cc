#include <rtx_reader.h>

#include <rtx_parser.h>
#include <bytecode.h>

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
}

wstring const RTXReader::SPECIAL_CHARS = L"!@$%()={}[]|\\/:;<>,.";

void
RTXReader::die(wstring message)
{
  wcerr << L"Syntax error on line " << currentLine << L" of ";
  wstring fname;
  fname.assign(sourceFile.begin(), sourceFile.end());
  wcerr << fname;
  wcerr <<L": " << message << endl;
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
      currentLine++;
    }
    if(inComment)
    {
      source.get();
      if(c == L'\n')
      {
        inComment = false;
      }
    }
    else if(isspace(c))
    {
      source.get();
    }
    else if(c == L'!')
    {
      source.get();
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
  if(SPECIAL_CHARS.find(c) != string::npos)
  {
    ret = wstring(1, c);
  }
  else if(c == L'-' && next == L'>')
  {
    next = source.get();
    ret = wstring(1, c) + wstring(1, next);
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
  }
  return ret;
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

vector<wstring>
RTXReader::parseIdentGroup(wstring first = L"")
{
  vector<wstring> ret;
  if(first == L"$" || first == L"%")
  {
    ret.push_back(first);
  }
  else if(first != L"")
  {
    ret.push_back(first);
    if(source.peek() == L'.')
    {
      nextToken();
    }
    else if(source.peek() == L'@')
    {
      ret.push_back(nextToken());
    }
    else
    {
      return ret;
    }
  }
  while(!source.eof())
  {
    if(source.peek() == L'$' || (source.peek() == L'%' && ret.size() == 0))
    {
      ret.push_back(nextToken());
    }
    ret.push_back(nextToken());
    if(source.peek() == L'.')
    {
      nextToken();
    }
    else if(source.peek() == L'@')
    {
      ret.push_back(nextToken());
    }
    else
    {
      break;
    }
  }
  return ret;
}

void
RTXReader::parseRule()
{
  vector<wstring> firstLabel = parseIdentGroup();
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
RTXReader::parseOutputRule(vector<wstring> pattern)
{
  vector<wstring> output;
  wstring cur;
  while(!source.eof())
  {
    cur = nextToken();
    if(cur == L"<")
    {
      cur = cur + nextToken() + nextToken(L">");
    }
    output.push_back(cur);
    if(nextToken(L".", L";") == L";")
    {
      break;
    }
  }
  outputRules.push_back(pair<vector<wstring>, vector<wstring>>(pattern, output));
}

void
RTXReader::parseRetagRule(vector<wstring> srcTags)
{
  vector<wstring> destTags = parseIdentGroup();
  nextToken(L":");
  vector<pair<vector<wstring>, vector<wstring>>> rule;
  rule.push_back(pair<vector<wstring>, vector<wstring>>(srcTags, destTags));
  wstring next;
  while(!source.eof())
  {
    rule.push_back(pair<vector<wstring>, vector<wstring>>(parseIdentGroup(), parseIdentGroup()));
    next = nextToken(L";", L",");
    if(next == L";")
    {
      break;
    }
  }
  retagRules.push_back(rule);
}

void
RTXReader::parseAttrRule(vector<wstring> name)
{
  if(name.size() > 1)
  {
    die(L"Found multiple symbols in attribute category name");
  }
  wstring categoryName = name[0];
  vector<wstring> members;
  wstring cur = nextToken();
  while(cur != L";" && !source.eof())
  {
    members.push_back(cur);
    cur = nextToken();
  }
  collections.insert(pair<wstring, vector<wstring>>(categoryName, members));
}

void
RTXReader::parsePatternElement(Rule* rule)
{
  vector<wstring> ret = parseIdentGroup();
  vector<wstring> pat;
  bool inVar = false;
  int atidx = 0;
  if(ret[0] == L"%")
  {
    rule->grab_all = rule->pattern.size();
    atidx++;
  }
  if(find(ret.begin(), ret.end(), L"@") != ret.end())
  {
    if(ret[atidx] == L"$")
    {
      pat.push_back(ret[atidx] + ret[atidx+1]);
      atidx += 3;
    }
    else
    {
      pat.push_back(ret[atidx]);
      atidx += 2;
    }
  }
  else
  {
    pat.push_back(L"");
  }
  for(unsigned int i = atidx; i < ret.size(); i++)
  {
    if(ret[i] == L"$")
    {
      inVar = true;
    }
    else if(inVar)
    {
      for(unsigned int n = 0; n < rule->resultVars.size(); n++)
      {
        for(unsigned int v = 0; v < rule->resultVars[n].size(); v++)
        {
          if(ret[i] == rule->resultVars[n][v])
          {
            rule->variableGrabs[n].push_back(
                pair<int, wstring>(rule->patternLength+1, ret[i]));
          }
        }
      }
      inVar = false;
    }
    else
    {
      pat.push_back(ret[i]);
    }
  }
  rule->pattern.push_back(pat);
  rule->patternLength++;
  eatSpaces();
}

void
RTXReader::parseOutputElement(Rule* rule)
{
  ResultNode* ret = new ResultNode;
  ret->getall = false;
  ret->dontoverwrite = false;
  if(source.peek() == L'%')
  {
    source.get();
    ret->getall = true;
    if(source.peek() == L'%')
    {
      source.get();
      ret->dontoverwrite = true;
    }
  }
  if(source.peek() == L'_')
  {
    if(ret->getall)
    {
      die(L"% cannot be used on blanks");
    }
    ret->mode = L"_";
    source.get();
    if(isdigit(source.peek()))
    {
      source >> ret->pos;
      if(ret->pos < 1 || ret->pos >= rule->patternLength)
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
    source >> ret->pos;
    if(ret->pos < 1 || ret->pos > rule->patternLength)
    {
      die(L"output index is out of bounds");
    }
  }
  else
  {
    ret->lemma = nextToken();
    ret->mode = nextToken(L"@");
    while(true)
    {
      wstring cur = nextToken();
      VarUpdate* vu = new VarUpdate;
      if(cur == L"$")
      {
        vu->src = -1;
        vu->destvar = nextToken();
        vector<wstring>::iterator loc = find(rule->resultVars[0].begin(), rule->resultVars[0].end(), vu->destvar);
        vu->srcvar += L'<';
        vu->srcvar += to_wstring(distance(rule->resultVars[0].begin(), loc)+2);
        vu->srcvar += L'>';
      }
      else if(cur == L"[")
      {
        if(!isdigit(source.peek()))
        {
          die(L"expected number after [");
        }
        source >> vu->src;
        nextToken(L".");
        vu->destvar = nextToken();
        vu->srcvar = vu->destvar;
        if(nextToken(L"]", L"/") == L"/")
        {
          vu->side = nextToken();
          nextToken(L"]");
        }
      }
      else
      {
        vu->src = 0;
        vu->srcvar = cur;
      }
      ret->updates.push_back(vu);
      if(source.peek() == L'.')
      {
        source.get();
      }
      else
      {
        break;
      }
    }
  }
  if(ret->getall)
  {
    vector<wstring> vars = rule->resultVars[rule->resultContents.size()-1];
    for(unsigned int v = 0; v < vars.size(); v++)
    {
      VarUpdate* vu = new VarUpdate;
      vu->src = 0;
      vu->dest = ret->pos;
      vu->srcvar = L"<" + to_wstring(v+2) + L">";
      vu->destvar = vars[v];
      ret->updates.push_back(vu);
    }
  }
  if(source.peek() == L'(')
  {
    nextToken();
    VarUpdate* vu = new VarUpdate;
    vu->dest = ret->pos;
    while(!source.eof() && source.peek() != L')')
    {
      vu->destvar = nextToken();
      nextToken(L"=");
      eatSpaces();
      if(isdigit(source.peek()))
      {
        source >> vu->src;
        nextToken(L".");
      }
      else if(source.peek() == L'$')
      {
        vu->src = -1;
        nextToken(L"$");
      }
      else
      {
        vu->src = 0;
      }
      vu->srcvar = nextToken();
      if(source.peek() == L'/')
      {
        source.get();
        vu->side = nextToken();
      }
      ret->updates.push_back(vu);
      eatSpaces();
      if(source.peek() == L',')
      {
        source.get();
      }
      else
      {
        break;
      }
    }
    nextToken(L")");
  }
  rule->resultContents.back().push_back(ret);
  eatSpaces();
}

void
RTXReader::parseReduceRule(vector<wstring> output, wstring next)
{
  vector<vector<wstring>> output_stuff;
  output_stuff.push_back(output);
  if(next != L"->")
  {
    wstring cur = next;
    while(cur != L"->")
    {
      output_stuff.push_back(parseIdentGroup(cur));
      cur = nextToken();
    }
  }
  vector<wstring> outNodes;
  vector<vector<wstring>> outVars;
  for(unsigned int n = 0; n < output_stuff.size(); n++)
  {
    outNodes.push_back(output_stuff[n][0]);
    outVars.push_back(vector<wstring>());
    for(unsigned int i = 1; i < output_stuff[n].size(); i++)
    {
      outVars[n].push_back(output_stuff[n][i]);
    }
  }
  Rule* rule;
  wstring endToken = L"";
  while(endToken != L";")
  {
    rule = new Rule();
    rule->resultNodes = outNodes;
    rule->resultVars = outVars;
    rule->patternLength = 0;
    rule->variableGrabs = vector<vector<pair<int, wstring>>>(
                            outNodes.size(), vector<pair<int, wstring>>());
    rule->grab_all = -1;
    eatSpaces();
    if(!isdigit(source.peek()))
    {
      die(L"Rule is missing weight");
    }
    source >> rule->weight;
    nextToken(L":");
    eatSpaces();
    while(!source.eof() && source.peek() != L'{')
    {
      parsePatternElement(rule);
    }
    for(unsigned int i = 0; i < outNodes.size(); i++)
    {
      rule->resultContents.push_back(vector<ResultNode*>());
      nextToken(L"{");
      while(!source.eof() && source.peek() != L'}')
      {
        parseOutputElement(rule);
      }
      nextToken(L"}");
    }
    reductionRules.push_back(rule);
    endToken = nextToken(L"|", L";");
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

/*
  struct Rule
  {
    int ID;
    int grab_all;
    float weight;
    int patternLength;
    vector<vector<wstring>> pattern;
    vector<wstring> resultNodes;
    vector<vector<wstring>> resultVars;
    vector<vector<wstring>> resultContents;
    vector<vector<pair<int, wstring>>> variableGrabs;
    vector<pair<pair<int, wstring>, pair<int, wstring>>> variableUpdates;
    wstring compiled;
  };
  n: _.gender.number
  adj: _.<sint>.gender
  NP.gender.number -> %n adj.sint {%2 _1 1};
*/

void
RTXReader::makePattern(int ruleid)
{
  int epsilon = td.getAlphabet()(0, 0);
  Rule* rule = reductionRules[ruleid];
  if(rule->pattern.size() > longestPattern)
  {
    longestPattern = rule->pattern.size();
  }
  // to make my life simpler, I'm going to start by only supporting 1 output chunk
  // TODO: fix this
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
  loc = td.getTransducer().insertSingleTransduction(symbol, loc);
  td.getTransducer().setFinal(loc);
}

wstring
RTXReader::compileString(wstring s)
{
  wstring ret;
  ret += STRING;
  ret += s.size();
  ret += s;
  return ret;
}

wstring
RTXReader::compileClip(wstring part, int pos, wstring side = L"")
{
  wstring c = compileString(part);
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
  else
  {
    ret += TARGETCLIP;
    ret += DUP;
    ret += compileString(L"");
    ret += EQUAL;
    ret += JUMPONFALSE;
    ret += (2*c.size() + 12);
    ret += c;
    ret += INT;
    ret += pos;
    ret += REFERENCECLIP;
    ret += DUP;
    ret += compileString(L"");
    ret += EQUAL;
    ret += JUMPONFALSE;
    ret += (c.size() + 3);
    ret += c;
    ret += INT;
    ret += pos;
    ret += SOURCECLIP;
  }
  // check if has default value
  return ret;
}

/*
  struct VarUpdate
  {
    int src;
    int dest;
    wstring srcvar;
    wstring destvar;
    wstring side;
  };
  struct ResultNode
  {
    wstring mode;
    int pos;
    wstring lemma;
    map<wstring, pair<int, wstring>> clips;
    vector<wstring> tags;
    bool getall;
    bool dontoverwrite;
    vector<VarUpdate*> updates;
  };
*/

wstring
RTXReader::processOutput(Rule* rule, ResultNode* r)
{
  wstring ret;
  if(r->mode == L"_")
  {
    ret += INT;
    ret += (wchar_t)r->pos;
    ret += BLANK;
  }
  else if(r->mode == L"#")
  {
    map<wstring, wstring> grab;
    for(unsigned int i = 0; i < r->updates.size(); i++)
    {
      VarUpdate* up = r->updates[i];
      if(up->src == -1)
      {
        grab[up->destvar] = up->srcvar;
        continue;
      }
      else if(up->src == 0)
      {
        ret += compileString(up->srcvar);
      }
      else
      {
        ret += compileClip(up->srcvar, up->src, up->side);
      }
      ret += compileString(up->destvar);
      ret += INT;
      ret += (wchar_t)r->pos;
      ret += SETCLIP;
    }
    if(rule->pattern[r->pos-1].size() == 1)
    {
      for(unsigned int i = 0; i < r->updates.size(); i++)
      {
        VarUpdate* up = r->updates[i];
        if(up->src == -1)
        {
          wstring tag;
          tag += L'<';
          tag += rule->varMap[0][up->srcvar];
          tag += L'>';
          ret += compileString(tag);
          ret += compileString(up->destvar);
          ret += INT;
          ret += (wchar_t)r->pos;
          ret += SETCLIP;
        }
      }
      ret += compileClip(L"whole", r->pos, L"tl");
    }
    else
    {
      wstring pos = rule->pattern[r->pos-1][1];
      vector<wstring> rl;
      for(unsigned int i = 0; i < outputRules.size(); i++)
      {
        // TODO: assumes output patterns are 1 tag long
        // also, is this really what we want the semantics of output
        // patterns to be? if not, what do we do with them instead?
        if(outputRules[i].first[0] == pos)
        {
          rl = outputRules[i].second;
          break;
        }
      }
      if(rl.size() == 0)
      {
        ret += compileClip(L"whole", r->pos, L"tl");
      }
      else
      {
        ret += CHUNK;
      }
      for(unsigned int p = 0; p < rl.size(); p++)
      {
        if(rl[p] == L"_")
        {
          ret += compileClip(L"lem", r->pos, L"tl");
          ret += APPENDSURFACE;
          wstring tag;
          tag += L'<';
          tag += pos;
          tag += L'>';
          ret += compileString(tag);
          ret += APPENDSURFACE;
        }
        else if(rl[p][0] == L'<')
        {
          ret += compileString(rl[p]);
          ret += APPENDSURFACE;
        }
        else if(grab.find(rl[p]) != grab.end())
        {
          wstring tag;
          tag += L'<';
          tag += rule->varMap[0][grab[rl[p]]];
          tag += L'>';
          ret += compileString(tag);
          ret += APPENDSURFACE;
        }
        else
        {
          ret += compileClip(rl[p], r->pos, L"tl");
          ret += APPENDSURFACE;
        }
      }
      if(rl.size() != 0)
      {
        ret += compileClip(L"whole", r->pos, L"tl");
        ret += APPENDALLCHILDREN;
      }
    }
  }
  else
  {
    ret += CHUNK;
    ret += compileString(r->lemma);
    ret += APPENDSURFACE;
    for(unsigned int i = 0; i < r->updates.size(); i++)
    {
      VarUpdate* up = r->updates[i];
      if(up->src == -1)
      {
        wstring tag;
        tag += L'<';
        tag += rule->varMap[0][up->srcvar];
        tag += L'>';
        ret += compileString(tag);
      }
      else if(up->src == 0)
      {
        ret += compileString(up->srcvar);
      }
      else
      {
        ret += compileClip(up->srcvar, up->src, up->side);
      }
      ret += APPENDSURFACE;
    }
  }
  return ret;
}

void
RTXReader::processRules()
{
  Rule* rule;
  for(unsigned int ruleid = 0; ruleid < reductionRules.size(); ruleid++)
  {
    rule = reductionRules[ruleid];
    makePattern(ruleid);
    wstring comp;
    comp += CHUNK;
    comp += compileString(L"unk<" + rule->resultNodes[0] + L">");
    comp += APPENDSURFACE;
    rule->varMap.push_back(map<wstring, int>());
    for(unsigned int vidx = 0; vidx < rule->resultVars[0].size(); vidx++)
    {
      wstring v = rule->resultVars[0][vidx];
      rule->varMap[0][v] = vidx + 2;
      bool foundvar = false;
      for(unsigned int g = 0; g < rule->variableGrabs[0].size(); g++)
      {
        if(rule->variableGrabs[0][g].second == v)
        {
          comp += compileClip(v, rule->variableGrabs[0][g].first, L"tl");
          foundvar = true;
          break;
        }
      }
      if(!foundvar)
      {
        if(rule->grab_all == -1)
        {
          comp += compileString(L"<unk>"); // TODO
        }
        else
        {
          comp += compileClip(v, rule->grab_all+1, L"tl");
        }
      }
      comp += APPENDSURFACE;
    }
    for(unsigned int oidx = 0; oidx < rule->resultContents[0].size(); oidx++)
    {
      comp += processOutput(rule, rule->resultContents[0][oidx]);
      comp += APPENDCHILD;
    }
    comp += OUTPUT;
    rule->compiled = comp;
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
