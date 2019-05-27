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
Parser::eatSpaces()
{
  const wchar_t c;
  bool inComment = false;
  while(!source.eof())
  {
    c = source.peek();
    if(inComment)
    {
      source.get();
      if(c == L"\n")
      {
        inComment = false;
      }
    }
    else if(isspace(c))
    {
      source.get();
    }
    else if(c == L"!")
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
Parser::parseIdent()
{
  eatSpaces();

  wstring ret = L"";
  const wchar_t cur;

  while(!source.eof())
  {
    cur = source.peek();
    if(SPECIAL_CHARS.find(cur) == string::npos && !isspace(cur))
    {
      ret += source.get();
    }
  }
  return ret; // @TODO: should be a check for empty identifiers somewhere
}

vector<wstring>
Parser::parseIdentGroup()
{
  eatSpaces();

  vector<wstring> ret;
  wstring cur;
  const wchar_t nextChar;
  while(!source.eof())
  {
    nextChar = source.peek();
    if(nextChar == L"$")
    {
      ret.push_back(source.get());
    }
    cur = parseIdent();
    if(cur == L"")
    {
      break; // @TODO: this is probably an error
    }
    else
    {
      ret.push_back(cur);
    }
    nextChar = source.peek();
    if(nextChar == L".")
    {
      source.get();
    }
  }
  return ret;
}

void
Parser::parseRule()
{
  eatSpaces();

  vector<wstring> firstLabel = parseIdentGroup();
  eatSpaces();
  const wchar_t next = source.get();
  switch (next) {
  case L":":
    parseOutputRule(firstLabel);
    break;
  case L">":
    parseRetagRule(firstLabel);
    break;
  case L"=":
    parseAttrRule(firstLabel);
    break;
  case L"-":
    const wchar_t next2 = source.peek();
    if(next2 == L">")
    {
      source.get();
      parseReduceRule(firstLabel, false);
      break;
    }
    else
    {
      source.putback(next);
    }
  default:
    parseReduceRule(firstLabel, true);
}

void
Parser::parseOutputRule(vector<wstring> pattern)
{
}

void
Parser::parseRetagRule(vector<wstring> source)
{
}

void
Parser::parseAttrRule(vector<string> name)
{
}

void
Parser::parseReduceRule(vector<string> output, bool isSingle)
{
}
