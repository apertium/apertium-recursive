#include <rtx_reader.h>

#include <rtx_parser.h>

#include <vector>

using namespace std;

wstring const
RTXReader::ANY_TAG = L"<ANY_TAG>";

wstring const
RTXReader::ANY_CHAR = L"<ANY_CHAR>";

RTXReader::RTXReader()
{
  td.getAlphabet().includeSymbol(ANY_TAG);
  td.getAlphabet().includeSymbol(ANY_CHAR);
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
  bool inVar = false;
  for(unsigned int i = 0; i < ret.size(); i++)
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
  }
  rule->pattern.push_back(ret);
  rule->patternLength++;
  eatSpaces();
}

void
RTXReader::parseOutputElement(Rule* rule)
{
  vector<wstring> ret;
  if(source.peek() == L'%')
  {
    ret.push_back(L"%");
  }
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
    for(unsigned int i = 0; i < outNodes.size(); i++)
    {
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
*/

void
RTXReader::processRules()
{
  int maxLen = 0;
  Rule* cur;
  for(unsigned int ruleid = 0; ruleid < reductionRules.size(); ruleid++)
  {
    cur = reductionRules[ruleid];
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
}

void
RTXReader::write(const string &fname)
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
}
