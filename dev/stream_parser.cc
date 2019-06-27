#include <stream_parser.h>
#include <parser_node.h>

StreamParser::StreamParser(wistream* input, map<wstring, vector<wstring>> attrs)
{
  source = input;
  for(map<wstring, vector<wstring>>::iterator it = attrs.begin();
        it != attrs.end(); ++it)
  {
    for(int i = 0; i < it->second.size(); i++)
    {
      if(attributes.find(it->second[i]) != attributes.end())
      {
        attributes[it->second[i]] = vector<wstring>();
      }
      attributes[it->second[i]].push_back(it->first);
    }
  }
}

StreamParser::~StreamParser()
{
}

void
StreamParser::readLU(ParserNode* node, int pos)
{
  wstring wholeString = L"";
  wstring lemma = L"";
  while(source->peek() != L'/' && source->peek() != L'<')
  {
    if(source->peek() == L'\\')
    {
      lemma.append(1, source->get());
    }
    lemma.append(1, source->get());
  }
  wholeString = lemma;
  node->setVar(L" lemma", lemma, pos);
  wstring tag;
  bool isFirst = true;
  while(source->peek() == L'<')
  {
    wholeString.append(1, source->get());
    tag = L"";
    while(source->peek() != L'>')
    {
      tag.append(1, source->get());
    }
    wholeString += tag;
    wholeString.append(1, source->get());
    if(attributes.find(tag) != attributes.end())
    {
      for(int i = 0; i < attributes[tag].size(); i++)
      {
         node->setVar(attributes[tag][i], tag, pos);
      }
    }
    if(pos == 0 && isFirst)
    {
      node->nodeType = L"@" + tag;
      isFirst = false;
    }
  }
  if(pos == 0)
  {
    node->defaultOutput = wholeString;
    if(isFirst)
    {
      node->nodeType = L"*";
    }
  }
}

ParserNode*
StreamParser::nextToken()
{
  //@TODO: check for eof
  ParserNode* ret = new ParserNode();
  if(source->peek() != L'^')
  {
    ret->nodeType = L"_";
    ret->defaultOutput = L"";
    while(source->peek() != L'^')
    {
      ret->defaultOutput.append(1, source->get());
    }
  }
  else
  {
    source->get(); // L'^'
    readLU(ret, 2);
    source->get(); // should be L'/' @TODO: check
    readLU(ret, 0);
    if(source->peek() == L'/')
    {
      source->get();
      readLU(ret, 1);
    }
    source->get(); // should be L'$' @TODO: check
  }
  return ret;
}
