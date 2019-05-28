#include <rtx_parser.h>

#include <vector>

using namespace std;

Parser::Parser()
{
}

Parser::~Parser()
{
}

wstring const Parser::SPECIAL_CHARS = L"!@$%()={}[]|\\/:;<>,.";

void
Parser::die(wstring message)
{
  wcerr << L"Syntax error on line " << currentLine << L" of ";
  wstring fname;
  fname.assign(sourceFile.begin(), sourceFile.end());
  wcerr << fname;
  wcerr <<L": " << message << endl;
  exit(EXIT_FAILURE);
}

void
Parser::eatSpaces()
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
Parser::nextTokenNoSpace()
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
      if(SPECIAL_CHARS.find(c) == string::npos && !isspace(c))
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
Parser::nextToken(wstring check1 = L"", wstring check2 = L"")
{
  eatSpaces();
  wstring tok = nextTokenNoSpace();
  if(tok == check1 || tok == check2 || (check1 == L"" && check2 == L""))
  {
    return tok;
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
}

vector<wstring>
Parser::parseIdentGroup(wstring first = L"")
{
  vector<wstring> ret;
  if(first != L"")
  {
    ret.push_back(first);
    if(source.peek() == L'.')
    {
      nextToken();
    }
    else
    {
      return ret;
    }
  }
  while(!source.eof())
  {
    if(source.peek() == L'$')
    {
      ret.push_back(nextToken());
    }
    ret.push_back(nextToken());
    if(source.peek() == L'.')
    {
      nextToken();
    }
    else
    {
      break;
    }
  }
  return ret;
}

void
Parser::parseRule()
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
Parser::parseOutputRule(vector<wstring> pattern)
{
  vector<wstring> output;
  wstring cur;
  while(!source.eof())
  {
    cur = nextToken();
    if(cur == L";")
    {
      break;
    }
    if(cur == L"<")
    {
      cur = cur + nextToken() + nextToken(L">");
    }
    output.push_back(cur);
  }
  outputRules.push_back(pair<vector<wstring>, vector<wstring>>(pattern, output));
}

void
Parser::parseRetagRule(vector<wstring> srcTags)
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
Parser::parseAttrRule(vector<wstring> name)
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
    allAttributes[cur] = true;
    cur = nextToken();
  }
  attributeRules.insert(pair<wstring, vector<wstring>>(categoryName, members));
}

void
Parser::parsePatternElement(ReductionRule* rule)
{
  vector<wstring> ret;
  if(source.peek() == L'_')
  {
    if(rule->patternLength == 0)
    {
      die(L"cannot match blank at start of pattern");
    }
    ret.push_back(L"_");
    source.get();
    rule->patternLength--;
  }
  else if(source.peek() == L'+')
  {
    if(rule->patternLength == 0)
    {
      die(L"cannot match conjoin mark at start of pattern");
    }
    ret.push_back(L"+");
    source.get();
    rule->patternLength--;
  }
  else if(source.peek() == L'@')
  {
    source.get();
    ret = parseIdentGroup();
    ret.insert(ret.begin(), L"@");
    bool inVar = false;
    for(int i = 0; i < ret.size(); i++)
    {
      if(ret[i] == L"$")
      {
        inVar = true;
      }
      else if(inVar)
      {
        for(int n = 0; n < rule->resultVars.size(); n++)
        {
          for(int v = 0; v < rule->resultVars[n].size(); v++)
          {
            if(ret[i] == rule->resultVars[n][v])
            {
              rule->variableGrabs[n].push_back(
                  pair<int, wstring>(rule->patternLength+1, ret[i]));
            }
          }
        }
      }
    }
    //@TODO: lemmas
  }
  else
  {
    ret = parseIdentGroup();
  }
  rule->pattern.push_back(ret);
  rule->patternLength++;
  eatSpaces();
}

void
Parser::parseOutputElement(ReductionRule* rule)
{
  vector<wstring> ret;
  if(source.peek() == L'_')
  {
    ret.push_back(L"_");
    source.get();
    if(isdigit(source.peek()))
    {
      int pos;
      source >> pos;
      if(pos < 1 || pos >= rule->patternLength)
      {
        die(L"position index of blank out of bounds");
      }
      ret.push_back(to_wstring(pos));
    }
  }
  else if(isdigit(source.peek()))
  {
    int pos;
    source >> pos;
    if(pos < 1 || pos > rule->patternLength)
    {
      die(L"output index is out of bounds");
    }
    if(source.peek() == L'(')
    {
      nextToken();
      wstring var1;
      wstring var2;
      int pos2;
      while(!source.eof() && source.peek() != L')')
      {
        var1 = nextToken();
        nextToken(L"=");
        eatSpaces();
        if(isdigit(source.peek()))
        {
          source >> pos2;
          nextToken(L".");
        }
        else
        {
          pos2 = 0;
          nextToken(L"$");
        }
        var2 = nextToken();
        rule->variableUpdates.push_back(
                pair<pair<int, wstring>, pair<int, wstring>>(
                    pair<int, wstring>(pos, var1),
                    pair<int, wstring>(pos2, var2)));
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
  }
  rule->resultContents.push_back(ret);
  eatSpaces();
}

void
Parser::parseReduceRule(vector<wstring> output, wstring next)
{
  if(output.size() == 1 && output[0] == L"START" && next == L"->")
  {
    ReductionRule* rule = new ReductionRule();
    rule->resultNodes = output;
    rule->pattern = vector<vector<wstring>>();
    rule->pattern.push_back(vector<wstring>(1, nextToken()));
    nextToken(L";");
    rule->process();
    reductionRules.push_back(rule);
    return;
  }
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
  for(int n = 0; n < output_stuff.size(); n++)
  {
    outNodes.push_back(output_stuff[n][0]);
    outVars.push_back(vector<wstring>());
    for(int i = 1; i < output_stuff[n].size(); i++)
    {
      outVars[n].push_back(output_stuff[n][i]);
    }
  }
  ReductionRule* rule;
  wstring endToken = L"";
  while(endToken != L";")
  {
    rule = new ReductionRule();
    rule->resultNodes = outNodes;
    rule->resultVars = outVars;
    rule->patternLength = 0;
    rule->variableGrabs = vector<vector<pair<int, wstring>>>(
                            outNodes.size(), vector<pair<int, wstring>>());
    rule->resultContents = vector<vector<wstring>>(
                            outNodes.size(), vector<wstring>());
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
    for(int i = 0; i < outNodes.size(); i++)
    {
      nextToken(L"{");
      while(!source.eof() && source.peek() != L'}')
      {
        parseOutputElement(rule);
      }
      nextToken(L"}");
    }
    rule->process();
    reductionRules.push_back(rule);
    endToken = nextToken(L"|", L";");
  }
}

void
Parser::parse(string fname)
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
}
