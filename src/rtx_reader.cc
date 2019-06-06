#include <rtx_reader.h>

#include <rtx_parser.h>

#include <vector>
#include <algorithm>

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
  vector<wstring> ret;
  bool getall = false;
  if(source.peek() == L'%')
  {
    source.get();
    getall = true;
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
    ret.push_back(to_wstring(pos));
    if(pos < 1 || pos > rule->patternLength)
    {
      die(L"output index is out of bounds");
    }
    if(getall)
    {
      vector<wstring> vars = rule->resultVars[rule->resultContents.size()-1];
      for(unsigned int v = 0; v < vars.size(); v++)
      {
        VarUpdate* vu = new VarUpdate;
        vu->src = 0;
        vu->dest = pos;
        vu->srcvar = L"<" + to_wstring(v+2) + L">";
        vu->destvar = vars[v];
      }
    }
    if(source.peek() == L'(')
    {
      nextToken();
      VarUpdate* vu = new VarUpdate;
      vu->dest = pos;
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
        rule->variableUpdates.push_back(vu);
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
  else
  {
    die(L"unexpected character: " + source.get());
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
      rule->resultContents.push_back(vector<vector<wstring>>());
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
    int another = td.getTransducer().insertSingleTransduction(L'\\', retval);
    td.getTransducer().linkStates(another, retval, any_char);
  }
  else
  {
    for(unsigned int i = 0, limit = lemma.size();  i != limit; i++)
    {
      if(lemma[i] == L'\\')
      {
        retval = td.getTransducer().insertSingleTransduction(L'\\', retval);
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
RTXReader::processRules()
{
  int epsilon = td.getAlphabet()(0, 0);
  Rule* rule;
  for(unsigned int ruleid = 0; ruleid < reductionRules.size(); ruleid++)
  {
    rule = reductionRules[ruleid];
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
        td.getTransducer().linkStates(loc, loc, td.getAlphabet()(L" "));
      }
      pat = rule->pattern[i];
      if(pat[0].size() > 0 && pat[0][0] == L'$')
      {
        int lemend;
        vector<wstring> lems = collections[pat[0].substr(1)];
        for(unsigned int l = 0; l < lems.size(); l++)
        {
          lemend = insertLemma(loc, lems[l]);
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
        tags += L'<' + pat[t] + L'>';
      }
      loc = insertTags(loc, tags);
      td.getTransducer().linkStates(loc, loc, td.getAlphabet()(ANY_TAG));
    }
    const int symbol = td.countToFinalSymbol(ruleid);
    const int fin = td.getTransducer().insertSingleTransduction(symbol, loc);
    td.getTransducer().setFinal(fin);
    wstring comp;
    int output_pieces = 0;
    wchar_t c;
    c = rule->resultNodes[0].size();
    comp += L's' + c;
    comp += rule->resultNodes[0];
    output_pieces++;
    for(unsigned int vidx = 0; vidx < rule->resultVars[0].size(); vidx++)
    {
      wstring v = rule->resultVars[0][vidx];
      c = v.size();
      bool foundvar = false;
      for(unsigned int g = 0; g < rule->variableGrabs[0].size(); g++)
      {
        if(rule->variableGrabs[0][g].second == v)
        {
          comp += L's';
          comp += c;
          comp += v;
          comp += L'T';
          comp += rule->variableGrabs[0][g].first;
          foundvar = true;
        }
      }
      if(!foundvar)
      {
        if(rule->grab_all == -1)
        {
          comp += L's';
          comp += 5;
          comp += L"<unk>"; // TODO
        }
        else
        {
          comp += L's';
          comp += c;
          comp += v;
          comp += L'T';
          comp += rule->grab_all+1;
        }
      }
      output_pieces++;
    }
    for(unsigned int oidx = 0; oidx < rule->resultContents[0].size(); oidx++)
    {
      vector<wstring> cur = rule->resultContents[0][oidx];
      if(cur[0] == L"_")
      {
        comp += cur[0];
        if(cur.size() > 1)
        {
          comp += L'_';
          comp += stoi(cur[1]);
        }
        output_pieces++;
      }
      else
      {
        int i = stoi(cur[0]);
        vector<VarUpdate*> updates;
        for(unsigned int u = 0; u < rule->variableUpdates.size(); u++)
        {
          if(rule->variableUpdates[u]->dest == i)
          {
            updates.push_back(rule->variableUpdates[u]);
          }
        }
        for(unsigned int u = 0; u < updates.size(); u++)
        {
          if(updates[u]->src == -1)
          {
            continue;
          }
          comp += L's';
          comp += updates[u]->srcvar.size();
          comp += updates[u]->srcvar;
          if(updates[u]->src != 0)
          {
            if(updates[u]->side == L"sl")
            {
              comp += L'S';
            }
            else if(updates[u]->side == L"ref")
            {
              comp += L'R';
            }
            else
            {
              comp += L'T';
            }
            comp += updates[u]->src;
          }
          comp += L's';
          comp += updates[u]->destvar.size();
          comp += updates[u]->destvar;
          comp += L't';
          comp += i;
        }
        if(rule->pattern[i-1].size() == 1)
        {
          comp += L's';
          comp += 5;
          comp += L"whole";
          comp += L'T';
          comp += i;
          output_pieces++;
        }
        else
        {
          wstring pos = rule->pattern[i-1][1];
          bool foundoutput = false;
          for(unsigned int o = 0; o < outputRules.size(); o++)
          {
            // TODO: assumes output patterns are 1 tag long
            // also, is this really what we want the semantics of output
            // patterns to be? if not, what do we do with them instead?
            if(outputRules[o].first[0] == pos)
            {
              foundoutput = true;
              int ct = 0;
              vector<wstring> rl = outputRules[0].second;
              for(unsigned int p = 0; p < rl.size(); p++)
              {
                if(rl[p] == L"_")
                {
                  ct += 2;
                  comp += L's';
                  comp += 3;
                  comp += L"lem";
                  comp += L'T';
                  comp += i;
                  comp += L's';
                  comp += (pos.size()+2);
                  comp += L'<';
                  comp += pos;
                  comp += L'>';
                }
                else if(rl[p][0] == L'<')
                {
                  ct++;
                  comp += L's';
                  comp += rl[p].size();
                  comp += rl[p];
                }
                else
                {
                  ct++;
                  bool found = false;
                  for(unsigned int v = 0; v < rule->resultVars[0].size(); v++)
                  {
                    if(rule->resultVars[0][v] == rl[p])
                    {
                      wstring s = L"<" + to_wstring(v+2) + L">";
                      comp += L's';
                      comp += s.size();
                      comp += s;
                      found = true;
                      break;
                    }
                  }
                  if(!found)
                  {
                    comp += L's';
                    comp += rl[p].size();
                    comp += rl[p];
                    comp += L'T';
                    comp += i;
                  }
                }
              }
              if(ct > 0)
              {
                comp += L'{';
                comp += ct;
                output_pieces++;
              }
            }
          }
          if(!foundoutput)
          {
            comp += L's';
            comp += 5;
            comp += L"whole";
            comp += L'T';
            comp += i;
            output_pieces++;
          }
        }
      }
    }
    comp += L'{';
    comp += output_pieces;
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
