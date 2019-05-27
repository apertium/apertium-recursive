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
Parser::nextToken(wstring check = L"")
{
  eatSpaces();
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
        ret += source.get();
      }
      else
      {
        break;
      }
    }
  }
  if(ret == check || check == L"")
  {
    return ret;
  }
  else
  {
    die(L"Expected '" + check + L"' found '" + ret + L"' instead");
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
      cur = cur + nextToken() + nextToken();
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
    //@TODO: error checking
    next = nextToken();
    if(next == L";")
    {
      break;
    }
    else if(next == L",")
    {
    }
    else
    {
      die(L"Unexpected '" + next + L"'");
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
    cur = nextToken();
  }
  attributeRules.insert(pair<wstring, vector<wstring>>(categoryName, members));
}

void
Parser::parseReduceRule(vector<wstring> output, wstring next)
{
  vector<vector<wstring>> outNodes;
  outNodes.push_back(output);
  if(next != L"->")
  {
    wstring cur = next;
    while(cur != L"->")
    {
      outNodes.push_back(parseIdentGroup(cur));
      cur = nextToken();
    }
  }
  die(L"see rtx_parser.cc - reduction rules not implemented yet");
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
