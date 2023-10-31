#include <rtx_config.h>
#include <rtx_compiler.h>
#include <lttoolbox/string_utils.h>
#include <algorithm>
#include <lttoolbox/i18n.h>

using namespace std;

RTXCompiler::RTXCompiler()
{
  PB.starCanBeEmpty = true;
  outputRules["UNKNOWN:INTERNAL"_u] = vector<UString>(1, "_"_u);
  clip_order.push_back(SOURCECLIP);
  clip_order.push_back(REFERENCECLIP);
  clip_order.push_back(TARGETCLIP);
}

UString const
RTXCompiler::SPECIAL_CHARS = "!@$%()={}[]|/:;<>,.→"_u;

void
RTXCompiler::die(UString message)
{
  if(errorsAreSyntax)
  {
    cerr << "Syntax error on line " << currentLine << " of ";
  }
  else
  {
    cerr << "Error in ";
    while(macroNameStack.size() > 0)
    {
      cerr << "macro '" << macroNameStack.back() << "', invoked by ";
      macroNameStack.pop_back();
    }
    cerr << "rule beginning on line " << currentRule->line << " of ";
  }
  cerr << sourceFile << ": " << message << endl;
  if(errorsAreSyntax && !source.eof())
  {
    UString arr = UString(recentlyRead.size()-2, ' ');
    recentlyRead += unreadbuf;
    while(!source.eof() && peekchar() != '\n')
    {
      recentlyRead += source.get();
    }
    cerr << recentlyRead << endl;
    cerr << arr << "^^^" << endl;
  }
  exit(EXIT_FAILURE);
}

void
RTXCompiler::die(icu::UnicodeString message)
{
  I18n i18n {ARC_I18N_DATA, "arc"};
  if(errorsAreSyntax)
  {
    cerr << i18n.format("in_file_on_line", {"file", "line"}, {sourceFile.c_str(), currentLine}) << endl;
    cerr << i18n.format("syntax_error", {"error"}, {message}) << endl;
  }
  else
  {
    icu::UnicodeString macros;
    while(macroNameStack.size() > 0)
    {
      macros += i18n.format("macro_invoked_by", {"macro"}, {icu::UnicodeString(macroNameStack.back().data())});
      macroNameStack.pop_back();
    }
    cerr << i18n.format("error_in_rule", {"macros", "line", "file", "error"},
      {macros, currentRule->line, sourceFile.c_str(), message})<< endl;
  }
  if(errorsAreSyntax && !source.eof())
  {
    UString arr = UString(recentlyRead.size()-2, ' ');
    recentlyRead += unreadbuf;
    while(!source.eof() && peekchar() != '\n')
    {
      recentlyRead += source.get();
    }
    cerr << recentlyRead << endl;
    cerr << arr << "^^^" << endl;
  }
  exit(EXIT_FAILURE);
}

UChar32
RTXCompiler::getchar()
{
  UChar32 c;
  if(unreadbuf.size() > 0)
  {
    int i = 0;
    U16_NEXT(unreadbuf, i, unreadbuf.size(), c);
    unreadbuf = unreadbuf.substr(i);
  }
  else c = source.get();
  recentlyRead += c;
  return c;
}

UChar32
RTXCompiler::peekchar()
{
  if (unreadbuf.empty()) {
    return source.peek();
  } else {
    UChar32 c;
    U16_GET(unreadbuf, 0, 0, unreadbuf.size(), c);
    return c;
  }
}

void
RTXCompiler::setUnreadMark()
{
  unreadmark = recentlyRead.size();
}

void
RTXCompiler::unread()
{
  unreadbuf = recentlyRead.substr(unreadmark) + unreadbuf;
  recentlyRead = recentlyRead.substr(0, unreadmark);
}

void
RTXCompiler::eatSpaces()
{
  UChar32 c;
  bool inComment = false;
  while(!source.eof())
  {
    c = peekchar();
    if(c == '\n')
    {
      getchar();
      inComment = false;
      currentLine++;
      recentlyRead.clear();
      unreadmark = 0;
    }
    else if(inComment || u_isspace(c))
    {
      getchar();
    }
    else if(c == '!')
    {
      getchar();
      inComment = true;
    }
    else
    {
      break;
    }
  }
}

UString
RTXCompiler::nextTokenNoSpace()
{
  if(source.eof())
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80060"));
  }
  UChar32 c = getchar();
  UChar32 next = peekchar();
  UString ret;
  if (c == u'\u2192') { // '→'
    ret = "->"_u;
  }
  else if(SPECIAL_CHARS.find(c) != string::npos)
  {
    ret += c;
  }
  else if(c == '-' && next == '>')
  {
    getchar();
    ret += c;
    ret += next;
  }
  else if(u_isspace(c))
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80070"));
  }
  else if(c == '!')
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80080"));
  }
  else if(c == '"')
  {
    next = getchar();
    while(!source.eof() && next != '"')
    {
      if(next == '\\') next = getchar();
      ret += next;
      if(source.eof()) die(I18n(ARC_I18N_DATA, "arc").format("ARC80060"));
      next = getchar();
    }
  }
  else
  {
    ret += c;
    while(!source.eof())
    {
      c = peekchar();
      if(c == '\\')
      {
        getchar();
        ret += getchar();
      }
      else if(SPECIAL_CHARS.find(c) == string::npos && !u_isspace(c))
      {
        ret += getchar();
      }
      else
      {
        break;
      }
    }
  }
  return ret;
}

bool
RTXCompiler::isNextToken(UChar32 c)
{
  if(peekchar() == c)
  {
    getchar();
    return true;
  }
  return false;
}

UString
RTXCompiler::nextToken(UString check1 = ""_u, UString check2 = ""_u)
{
  eatSpaces();
  UString tok = nextTokenNoSpace();
  if(tok == check1 || tok == check2 || (check1.empty() && check2.empty()))
  {
  }
  else if(!check1.empty() && !check2.empty())
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80090", {"check1", "check2", "token"},
      {icu::UnicodeString(check1.data()),
       icu::UnicodeString(check2.data()),
       icu::UnicodeString(tok.data())}));
  }
  else if(!check1.empty())
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80100", {"check", "token"},
      {icu::UnicodeString(check1.data()),
       icu::UnicodeString(tok.data())}));
  }
  else
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80100", {"check", "token"},
       {icu::UnicodeString(check2.data()),
        icu::UnicodeString(tok.data())}));
  }
  return tok;
}

UString
RTXCompiler::parseIdent(bool prespace = false)
{
  if(prespace)
  {
    eatSpaces();
  }
  UChar32 next = peekchar();
  UString ret = nextTokenNoSpace();
  if(next == '"')
  {
    // so that quoted special characters don't fail the next check
    return ret;
  }
  if(ret == "->"_u || (ret.size() == 1 && SPECIAL_CHARS.find(ret[0]) != string::npos))
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80110", {"token"},
       {icu::UnicodeString(ret.data())}));
  }
  return ret;
}

unsigned int
RTXCompiler::parseInt()
{
  UString ret;
  while(isdigit(peekchar()))
  {
    ret += getchar();
  }
  return StringUtils::stoi(ret);
}

float
RTXCompiler::parseWeight()
{
  UString ret;
  while(isdigit(peekchar()) || peekchar() == '.')
  {
    ret += getchar();
  }
  float r;
  try
  {
    r = StringUtils::stod(ret);
  }
  catch(const invalid_argument& ia)
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80120", {"token"},
       {icu::UnicodeString(ret.data())}));
  }
  return r;
}

void
RTXCompiler::parseRule()
{
  UString firstLabel = parseIdent();
  UString next = nextToken();
  if(next == ":"_u)
  {
    parseOutputRule(firstLabel);
  }
  else if(next == ">"_u)
  {
    parseRetagRule(firstLabel);
  }
  else if(next == "="_u)
  {
    if (firstLabel == "SIDE_SOURCES"_u) {
      clip_order.clear();
      eatSpaces();
      while (!isNextToken(';')) {
        UString side = parseIdent();
        if (side == "sl"_u) {
          clip_order.push_back(SOURCECLIP);
        } else if (side == "tl"_u) {
          clip_order.push_back(TARGETCLIP);
        } else if (side == "ref"_u) {
          clip_order.push_back(REFERENCECLIP);
        } else {
          die(I18n(ARC_I18N_DATA, "arc").format("ARC80130", {"side"},
            {icu::UnicodeString(side.data())}));
        }
        eatSpaces();
      }
      if (clip_order.empty()) {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80140"));
      }
      reverse(clip_order.begin(), clip_order.end());
    } else {
      parseAttrRule(firstLabel);
    }
  }
  else
  {
    parseReduceRule(firstLabel, next);
  }
}

void
RTXCompiler::parseOutputRule(UString pattern)
{
  nodeIsSurface[pattern] = !isNextToken(':');
  eatSpaces();
  vector<UString> output;
  if(peekchar() == '(')
  {
    LocationType typewas = currentLocType;
    Location locwas = currentLoc;
    currentLoc = LocChunk;
    currentLocType = LocTypeMacro;
    macros[pattern] = parseOutputCond();
    output.push_back("macro"_u);
    currentLocType = typewas;
    currentLoc = locwas;
    nextToken(";"_u);
  }
  else if(peekchar() == '%')
  {
    output.push_back("%"_u);
    nextToken("%"_u);
    nextToken(";"_u);
  }
  else
  {
    UString cur;
    while(!source.eof())
    {
      cur = nextToken();
      if(cur == "<"_u)
      {
        cur = cur + parseIdent();
        cur += nextToken(">"_u);
      }
      output.push_back(cur);
      if(nextToken("."_u, ";"_u) == ";"_u)
      {
        break;
      }
    }
    if(output.size() == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80150"));
    }
  }
  outputRules[pattern] = output;
}

void
RTXCompiler::parseRetagRule(UString srcTag)
{
  UString destTag = parseIdent(true);
  nextToken(":"_u);
  vector<pair<UString, UString>> rule;
  rule.push_back(pair<UString, UString>(srcTag, destTag));
  while(!source.eof())
  {
    eatSpaces();
    bool list = isNextToken('[');
    UString cs = parseIdent(true);
    if(list)
    {
      nextToken("]"_u);
      cs = "[]"_u + cs;
    }
    UString cd = parseIdent(true);
    rule.push_back(pair<UString, UString>(cs, cd));
    if(nextToken(";"_u, ","_u) == ";"_u)
    {
      break;
    }
  }
  bool found = false;
  for(auto other : retagRules)
  {
    if(other[0].first == srcTag && other[0].second == destTag)
    {
      found = true;
      I18n(ARC_I18N_DATA, "arc").error("ARC60160", {"src_tag", "dest_tag"}, 
        {icu::UnicodeString(srcTag.data()),
         icu::UnicodeString(destTag.data())}, false);
      other.insert(other.begin()+1, rule.begin()+1, rule.end());
      break;
    }
  }
  if(!found)
  {
    retagRules.push_back(rule);
    if(altAttrs.find(destTag) == altAttrs.end())
    {
      altAttrs[destTag].push_back(destTag);
    }
    altAttrs[destTag].push_back(srcTag);
  }
}

void
RTXCompiler::parseAttrRule(UString categoryName)
{
  if(collections.find(categoryName) != collections.end()
     || PB.isAttrDefined(categoryName))
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80170", {"category"},
      {icu::UnicodeString(categoryName.data())}));
  }
  eatSpaces();
  if(isNextToken('('))
  {
    UString undef = parseIdent(true);
    UString def = parseIdent(true);
    attrDefaults[categoryName] = make_pair(undef, def);
    nextToken(")"_u);
  }
  vector<UString> members;
  vector<UString> noOver;
  while(true)
  {
    eatSpaces();
    if(isNextToken(';'))
    {
      break;
    }
    if(isNextToken('['))
    {
      UString other = parseIdent(true);
      if(collections.find(other) == collections.end())
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80180", {"category"},
          {icu::UnicodeString(other.data())}));
      }
      vector<UString> otherstuff = collections[other];
      for(unsigned int i = 0; i < otherstuff.size(); i++)
      {
        members.push_back(otherstuff[i]);
      }
      otherstuff = noOverwrite[other];
      for(unsigned int i = 0; i < otherstuff.size(); i++)
      {
        noOver.push_back(otherstuff[i]);
      }
      nextToken("]"_u);
    }
    else if(isNextToken('@'))
    {
      UString next = parseIdent();
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
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80190"));
  }
  collections.insert(pair<UString, vector<UString>>(categoryName, members));
  noOverwrite.insert(pair<UString, vector<UString>>(categoryName, noOver));
  if(noOver.size() > 0)
  {
    for(unsigned int i = 0; i < noOver.size(); i++)
    {
      noOver[i] = "<"_u + noOver[i] + ">"_u;
    }
  }
  collections.insert(make_pair(categoryName + " over"_u, noOver));
}

RTXCompiler::Clip*
RTXCompiler::parseClip(int src = -2)
{
  bool bounds = true;
  Clip* ret = new Clip;
  eatSpaces();
  if(src != -2 && src != -3)
  {
    ret->src = src;
  }
  else if(isNextToken('>'))
  {
    ret->src = parseInt();
    nextToken("."_u);
    bounds = false;
  }
  else if(isdigit(peekchar()))
  {
    ret->src = parseInt();
    nextToken("."_u);
  }
  else if(isNextToken('$'))
  {
    if(isNextToken('$'))
    {
      ret->src = ChunkVarClip;
      ret->varName = parseIdent();
      nextToken("."_u);
    }
    else if(isNextToken('%'))
    {
      ret->src = StringVarClip;
      ret->varName = parseIdent();
    }
    else
    {
      ret->src = ParentClip;
      if(currentLocType != LocTypeOutput)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80200"));
      }
    }
  }
  else if(peekchar() == '(')
  {
    OutputChunk* chunkwas = currentChunk;
    OutputChoice* choicewas = currentChoice;
    currentClip = ret;
    currentChunk = NULL;
    currentChoice = NULL;
    ret->src = ConditionClip;
    Location locwas = currentLoc;
    currentLoc = LocClip;
    ret->choice = parseOutputCond();
    currentLoc = locwas;
    currentChunk = chunkwas;
    currentChoice = choicewas;
    currentClip = NULL;
    return ret;
  }
  else
  {
    ret->src = ConstantClip;
  }
  if(currentLocType == LocTypeMacro)
  {
    if(ret->src == ParentClip || ret->src > 1)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80210"));
    }
  }
  else if(bounds && src == -2 && ret->src > (int)currentRule->pattern.size())
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80220", {"position", "elements"},
      {ret->src,
       to_string(currentRule->pattern.size()).c_str()}));
  }
  if(ret->src != StringVarClip)
  {
    ret->part = (src == -3) ? nextToken() : parseIdent();
  }
  if(isNextToken('/'))
  {
    if(ret->src == ConstantClip)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80230"));
    }
    else if(ret->src == StringVarClip)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80240"));
    }
    ret->side = parseIdent();
  }
  else if(ret->src == ParentClip)
  {
    ret->side = "tl"_u;
  }
  if(isNextToken('>'))
  {
    if(ret->src == ConstantClip)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80250"));
    }
    else if(ret->src == ParentClip || ret->src == StringVarClip)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80260"));
    }
    ret->rewrite.push_back(parseIdent());
  }
  return ret;
}

UChar
RTXCompiler::lookupOperator(UString op)
{
  UString key = StringUtils::tolower(op);
  key = StringUtils::substitute(key, "-"_u, ""_u);
  key = StringUtils::substitute(key, "_"_u, ""_u);
  for(unsigned int i = 0; i < OPERATORS.size(); i++)
  {
    if(key == OPERATORS[i].first)
    {
      return OPERATORS[i].second;
    }
  }
  return 0;
}

RTXCompiler::Cond*
RTXCompiler::parseCond()
{
  nextToken("("_u);
  eatSpaces();
  vector<Cond*> parts;
  while(!source.eof() && peekchar() != ')')
  {
    if(peekchar() == '(')
    {
      parts.push_back(parseCond());
    }
    else
    {
      Cond* p = new Cond;
      p->op = 0;
      p->val = parseClip(-3);
      parts.push_back(p);
    }
    eatSpaces();
  }
  nextToken(")"_u);
  if(parts.size() == 0) die(I18n(ARC_I18N_DATA, "arc").format("ARC80270"));
  vector<pair<bool, Cond*>> denot;
  bool negated = false;
  for(unsigned int i = 0; i < parts.size(); i++)
  {
    if(i != parts.size() - 1 && parts[i]->op == 0
       && parts[i]->val->src == 0)
    {
      UChar op = lookupOperator(parts[i]->val->part);
      if(op == NOT)
      {
        negated = !negated;
        continue;
      }
    }
    denot.push_back(make_pair(negated, parts[i]));
    negated = false;
  }
  vector<pair<bool, Cond*>> destring;
  for(unsigned int i = 0; i < denot.size(); i++)
  {
    if(i != 0 && i != denot.size() - 1 && denot[i].second->op == 0
       && denot[i].second->val->src == 0)
    {
      UChar op = lookupOperator(denot[i].second->val->part);
      if(op != 0 && op != AND && op != OR && op != NOT)
      {
        if(destring.back().second->op == 0 && denot[i+1].second->op == 0)
        {
          if(destring.back().first || denot[i+1].first)
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80280"));
          }
          denot[i].second->left = destring.back().second;
          denot[i].second->right = denot[i+1].second;
          destring.pop_back();
          denot[i].second->op = op;
          denot[i].second->val = NULL;
          destring.push_back(denot[i]);
          i++;
          continue;
        }
      }
    }
    destring.push_back(denot[i]);
  }
  Cond* ret;
  if(destring[0].first)
  {
    ret = new Cond;
    ret->op = NOT;
    ret->right = destring[0].second;
  }
  else ret = destring[0].second;
  if(destring.size() % 2 == 0) die(I18n(ARC_I18N_DATA, "arc").format("ARC80290"));
  for(unsigned int i = 1; i < destring.size(); i += 2)
  {
    if(destring[i].second->op != 0) die(I18n(ARC_I18N_DATA, "arc").format("ARC80300"));
    if(destring[i].second->val->src != 0) die(I18n(ARC_I18N_DATA, "arc").format("ARC80310"));
    UChar op = lookupOperator(destring[i].second->val->part);
    if(op == 0) die(I18n(ARC_I18N_DATA, "arc").format("ARC80320", {"operator"},
      {icu::UnicodeString(destring[i].second->val->part.data())}));
    Cond* temp = ret;
    ret = new Cond;
    ret->left = temp;
    ret->op = op;
    if(destring[i+1].first)
    {
      ret->right = new Cond;
      ret->right->right = destring[i+1].second;
      ret->op = NOT;
    }
    else ret->right = destring[i+1].second;
    if(destring[i].first)
    {
      temp = ret;
      ret = new Cond;
      ret->right = temp;
      ret->op = NOT;
    }
  }
  return ret;
}

void
RTXCompiler::parsePatternElement(Rule* rule)
{
  vector<UString> pat;
  if(isNextToken('%'))
  {
    if(rule->grab_all != -1) {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80330"));
    }
    rule->grab_all = rule->pattern.size()+1;
  }
  UString t1 = nextToken();
  if(t1 == "$"_u)
  {
    t1 += parseIdent();
  }
  else if(t1 == "["_u)
  {
    t1 = "$"_u + parseIdent();
    if(!isNextToken(']')) die(I18n(ARC_I18N_DATA, "arc").format("ARC80340"));
  }
  else if(t1 == "|"_u)
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80350"));
  }
  if(isNextToken('@'))
  {
    pat.push_back(t1);
    pat.push_back(parseIdent());
  }
  else if(t1[0] == '$')
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80360"));
  }
  else
  {
    pat.push_back(""_u);
    pat.push_back(t1);
  }
  while(!source.eof())
  {
    if(!isNextToken('.'))
    {
      break;
    }
    UString cur = nextToken();
    if(cur == "$"_u)
    {
      Clip* cl = parseClip(rule->pattern.size()+1);
      if(rule->vars.find(cl->part) != rule->vars.end())
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80370", {"attr"}, {icu::UnicodeString(cl->part.data())}));
      }
      rule->vars[cl->part] = cl;
    }
    else if(cur == "["_u)
    {
      pat.push_back("["_u + parseIdent() + "]"_u);
      nextToken("]"_u);
    }
    else
    {
      pat.push_back(cur);
    }
  }
  if(pat.size() == 2 && pat[1] == "*"_u)
  {
    pat[1] = "UNKNOWN:INTERNAL"_u;
  }
  rule->pattern.push_back(pat);
  eatSpaces();
}

RTXCompiler::OutputChoice*
RTXCompiler::chunkToCond(RTXCompiler::OutputChunk* ch)
{
  OutputChoice* ret = new OutputChoice;
  ret->nest.push_back(NULL);
  ret->clips.push_back(NULL);
  ret->chunks.push_back(ch);
  return ret;
}

RTXCompiler::OutputChunk*
RTXCompiler::parseOutputElement()
{
  OutputChunk* ret = new OutputChunk;
  ret->conjoined = isNextToken('+');
  ret->interpolated = false;
  if(!ret->conjoined) ret->interpolated = isNextToken('<');
  ret->nextConjoined = false;
  if(ret->conjoined || ret->interpolated)
  {
    UString verb = (ret->conjoined ? "conjoin"_u : "interpolate"_u);
    if(currentChunk == NULL)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80380", {"verb"}, {icu::UnicodeString(verb.data())}));
    }
    if(currentChunk->children.size() == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80390", {"verb"}, {icu::UnicodeString(verb.data())}));
    }
    if(currentChunk->children.back()->conds.size() > 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80400", {"verb"}, {icu::UnicodeString(verb.data())}));
    }
    if(currentChunk->children.back()->chunks.size() == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80410", {"verb"}, {icu::UnicodeString(verb.data())}));
    }
    if(currentChunk->children.back()->chunks[0]->mode == "_"_u)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80420", {"verb"}, {icu::UnicodeString(verb.data())}));
    }
    eatSpaces();
    if(ret->interpolated) currentChunk->children.back()->chunks[0]->nextConjoined = true;
  }
  bool isInterp = isNextToken('>');
  eatSpaces();
  ret->getall = isNextToken('%');
  if(peekchar() == '_')
  {
    if(ret->getall)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80430"));
    }
    ret->mode = "_"_u;
    getchar();
    if(isdigit(peekchar()))
    {
      ret->pos = parseInt();
      if(currentRule->pattern.size() == 1)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80440"));
      }
      if(ret->pos < 1 || ret->pos >= currentRule->pattern.size())
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80450", {"num"},
          {to_string(currentRule->pattern.size()-1).c_str()}));
      }
      I18n(ARC_I18N_DATA, "arc").error("ARC60460", {"line"}, {currentLine}, false);
    }
    else
    {
      ret->pos = 0;
    }
  }
  else if(isdigit(peekchar()))
  {
    ret->mode = "#"_u;
    ret->pos = parseInt();
    if(ret->pos == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80470"));
    }
    else if(currentLocType != LocTypeMacro && !isInterp && ret->pos > currentRule->pattern.size())
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80480", {"elements"},
        {to_string(currentRule->pattern.size()).c_str()}));
    }
    if(peekchar() == '(')
    {
      nextToken("("_u);
      ret->pattern = parseIdent();
      nextToken(")"_u);
    }
    else if(currentLocType == LocTypeMacro)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80490"));
    }
  }
  else if(isNextToken('*'))
  {
    if(peekchar() != '(')
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80500"));
    }
    nextToken("("_u);
    ret->pattern = parseIdent(true);
    nextToken(")"_u);
    ret->pos = 0;
    ret->mode = "#"_u;
  }
  else if(isNextToken('$'))
  {
    if(isInterp) die(I18n(ARC_I18N_DATA, "arc").format("ARC80510"));
    if(ret->getall) die(I18n(ARC_I18N_DATA, "arc").format("ARC80520"));
    nextToken("$"_u);
    ret->mode = "$$"_u;
    ret->pattern = parseIdent(true);
  }
  else
  {
    ret->lemma = parseIdent();
    ret->pos = 0;
    UString mode = nextToken("@"_u, "("_u);
    if(mode == "@"_u)
    {
      if(ret->getall)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80530"));
      }
      ret->mode = "@"_u;
      while(true)
      {
        UString cur = nextToken();
        UString var = StringUtils::itoa(ret->tags.size());
        ret->tags.push_back(var);
        Clip* cl = new Clip;
        if(cur == "$"_u)
        {
          cl->src = -1;
          cl->part = parseIdent();
        }
        else if(cur == "["_u)
        {
          cl = parseClip();
          nextToken("]"_u);
        }
        else if(cur == "{"_u)
        {
          ret->tags.pop_back();
          var = "lemcase"_u;
          cl = parseClip();
          nextToken("}"_u);
        }
        else
        {
          cl->src = 0;
          cl->part = cur;
        }
        ret->vars[var] = cl;
        if(!isNextToken('.'))
        {
          break;
        }
      }
    }
    else
    {
      ret->mode = "#@"_u;
      ret->pattern = parseIdent(true);
      nextToken(")"_u);
      Clip* pos = new Clip;
      pos->src = 0;
      pos->part = ret->pattern;
      pos->rewrite.push_back("pos_tag"_u);
      ret->vars["pos_tag"_u] = pos;
      unsigned int i = 0;
      for(; i < ret->lemma.size(); i++)
      {
        if(ret->lemma[i] == '#') break;
      }
      Clip* lemh = new Clip;
      lemh->part = ret->lemma.substr(0, i);
      lemh->src = 0;
      lemh->rewrite.push_back("lemh"_u);
      ret->vars["lemh"_u] = lemh;
      if(i < ret->lemma.size())
      {
        Clip* lemq = new Clip;
        lemq->part = ret->lemma.substr(i+2);
        lemq->src = 0;
        lemq->rewrite.push_back("lemq"_u);
        ret->vars["lemq"_u] = lemq;
      }
      Clip* lem = new Clip;
      lem->part = ret->lemma;
      lem->src = 0;
      lem->rewrite.push_back("lem"_u);
      ret->vars["lem"_u] = lem;
    }
  }
  if(isNextToken('['))
  {
    while(!source.eof() && peekchar() != ']')
    {
      eatSpaces();
      UString var = parseIdent();
      nextToken("="_u);
      eatSpaces();
      Clip* cl = parseClip();
      if(cl->part == "_"_u)
      {
        cl->part.clear();
      }
      if(cl->src != 0 && cl->src != -2)
      {
        cl->rewrite.push_back(var);
      }
      ret->vars[var] = cl;
      if(nextToken(","_u, "]"_u) == "]"_u)
      {
        break;
      }
    }
  }
  eatSpaces();
  return ret;
}

RTXCompiler::OutputChoice*
RTXCompiler::parseOutputCond()
{
  nextToken("("_u);
  OutputChoice* choicewas = currentChoice;
  OutputChunk* chunkwas = currentChunk;
  Clip* clipwas = currentClip;
  OutputChoice* ret = new OutputChoice;
  currentChoice = ret;
  currentChunk = NULL;
  currentClip = NULL;
  while(true)
  {
    UString mode = StringUtils::tolower(nextToken());
    mode = StringUtils::substitute(mode, "-"_u, ""_u);
    mode = StringUtils::substitute(mode, "_"_u, ""_u);
    if(ret->conds.size() == 0 && mode != "if"_u && mode != "always"_u)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80540"));
    }
    if(ret->conds.size() > 0 && mode == "always"_u)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80550"));
    }
    if(mode == "if"_u || mode == "elif"_u || mode == "elseif"_u)
    {
      ret->conds.push_back(parseCond());
    }
    else if(mode == ")"_u)
    {
      break;
    }
    else if(mode != "else"_u && mode != "otherwise"_u && mode != "always"_u)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80560", {"statement"}, {icu::UnicodeString(mode.data())}));
    }
    eatSpaces();
    if(peekchar() == '(')
    {
      ret->nest.push_back(parseOutputCond());
      ret->chunks.push_back(NULL);
      ret->clips.push_back(NULL);
    }
    else if(currentLoc == LocClip)
    {
      ret->clips.push_back(parseClip());
      ret->chunks.push_back(NULL);
      ret->nest.push_back(NULL);
    }
    else if(peekchar() == '{')
    {
      if(currentLoc == LocChunk)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80570"));
      }
      else if(currentLocType == LocTypeMacro)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80580"));
      }
      else if(currentLoc == LocVarSet)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80590"));
      }
      ret->nest.push_back(NULL);
      ret->clips.push_back(NULL);
      ret->chunks.push_back(parseOutputChunk());
    }
    else if(peekchar() == '[')
    {
      if(currentLoc == LocVarSet)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80600"));
      }
      ret->nest.push_back(NULL);
      ret->clips.push_back(NULL);
      ret->chunks.push_back(parseOutputChunk());
    }
    else
    {
      if(currentLoc != LocChunk && currentLoc != LocVarSet)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80610"));
      }
      ret->chunks.push_back(parseOutputElement());
      ret->nest.push_back(NULL);
      ret->clips.push_back(NULL);
    }
    if(mode == "else"_u || mode == "otherwise"_u || mode == "always"_u)
    {
      nextToken(")"_u);
      break;
    }
  }
  currentChunk = chunkwas;
  currentChoice = choicewas;
  currentClip = clipwas;
  if(ret->chunks.size() == 0)
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80620"));
  }
  if(ret->conds.size() == ret->nest.size())
  {
    if(currentLoc == LocChunk && currentLocType == LocTypeMacro)
    {
      I18n(ARC_I18N_DATA, "arc").error("ARC60630", {"line"}, {currentLine}, false);
    }
    //die("If statement has no else clause and thus could produce no output."_u);
    ret->nest.push_back(NULL);
    if(currentLoc == LocClip)
    {
      Clip* blank = new Clip;
      blank->src = 0;
      blank->part.clear();
      ret->clips.push_back(blank);
      ret->chunks.push_back(NULL);
    }
    else
    {
      OutputChunk* temp = new OutputChunk;
      temp->mode = "[]"_u;
      temp->pos = 0;
      temp->conjoined = false;
      ret->chunks.push_back(temp);
      ret->clips.push_back(NULL);
    }
  }
  eatSpaces();
  return ret;
}

RTXCompiler::OutputChunk*
RTXCompiler::parseOutputChunk()
{
  int end;
  OutputChunk* ch = new OutputChunk;
  ch->conjoined = false;
  if(nextToken("{"_u, "["_u) == "{"_u)
  {
    currentLoc = LocChunk;
    ch->mode = "{}"_u;
    end = '}';
  }
  else
  {
    if(currentLoc != LocChunk)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80640"));
    }
    ch->mode = "[]"_u;
    end = ']';
  }
  eatSpaces();
  OutputChunk* chunkwas = currentChunk;
  OutputChoice* choicewas = currentChoice;
  currentChunk = ch;
  currentChoice = NULL;
  ch->pos = 0;
  while(peekchar() != end)
  {
    if(peekchar() == '(')
    {
      ch->children.push_back(parseOutputCond());
    }
    else
    {
      ch->children.push_back(chunkToCond(parseOutputElement()));
    }
  }
  nextToken(UString(1, end));
  if(end == '}') currentLoc = LocTopLevel;
  eatSpaces();
  currentChunk = chunkwas;
  currentChoice = choicewas;
  return ch;
}

void
RTXCompiler::parseReduceRule(UString output, UString next)
{
  vector<UString> outNodes;
  outNodes.push_back(output);
  if(next != "->"_u)
  {
    UString cur = next;
    while(cur != "->"_u)
    {
      if(SPECIAL_CHARS.find(cur) != UString::npos)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80650"));
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
    rule->output_sl = vector<OutputChoice*>(outNodes.size(), NULL);
    rule->output_ref = vector<OutputChoice*>(outNodes.size(), NULL);
    eatSpaces();
    rule->line = currentLine;
    currentLocType = LocTypeInput;
    currentLoc = LocTopLevel;
    if(!source.eof() && peekchar() == '"')
    {
      setUnreadMark();
      UString nm = parseIdent();
      if(peekchar() == '@')
      {
        unread();
      }
      else
      {
        rule->name = nm;
        eatSpaces();
      }
    }
    if(isdigit(peekchar()))
    {
      rule->weight = parseWeight();
      nextToken(":"_u);
      eatSpaces();
    }
    else
    {
      rule->weight = 0;
    }
    while(!source.eof() && peekchar() != '{' && peekchar() != '(' && peekchar() != '?')
    {
      if(peekchar() == '[')
      {
        setUnreadMark();
        getchar();
        UChar32 next = peekchar();
        unread();
        if(next == '$' || u_isspace(next)) break;
      }
      parsePatternElement(rule);
    }
    if(rule->pattern.size() == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80660"));
    }
    eatSpaces();
    if(isNextToken('?'))
    {
      rule->cond = parseCond();
      eatSpaces();
    }
    if(isNextToken('['))
    {
      while(!source.eof())
      {
        eatSpaces();
        if(!isNextToken('$'))
        {
          unsigned int idx = 1;
          if(isdigit(peekchar()))
          {
            idx = parseInt();
          }
          if(idx == 0 || idx > outNodes.size())
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80670"));
          }
          nextToken("/"_u);
          bool sl = (nextToken("sl"_u, "ref"_u) == "sl"_u);
          nextToken("="_u);
          currentLoc = LocVarSet;
          OutputChoice* cond;
          if(peekchar() == '(') cond = parseOutputCond();
          else cond = chunkToCond(parseOutputElement());
          if(sl)
          {
            if(rule->output_sl[idx-1] != NULL)
            {
              die(I18n(ARC_I18N_DATA, "arc").format("ARC80680"));
            }
            rule->output_sl[idx-1] = cond;
          }
          else
          {
            if(rule->output_ref[idx-1] != NULL)
            {
              die(I18n(ARC_I18N_DATA, "arc").format("ARC80690"));
            }
            rule->output_ref[idx-1] = cond;
          }
        }
        else if(isNextToken('$'))
        {
          UString var = parseIdent();
          if(rule->globals.find(var) != rule->globals.end())
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80700", {"var"}, {icu::UnicodeString(var.data())}));
          }
          nextToken("="_u);
          currentLoc = LocVarSet;
          if(peekchar() == '(') rule->globals[var] = parseOutputCond();
          else rule->globals[var] = chunkToCond(parseOutputElement());
          currentLoc = LocTopLevel;
          if(globalVarNames.find(var) == globalVarNames.end())
          {
            int temp = globalVarNames.size();
            globalVarNames[var] = temp;
          }
        }
        else if(isNextToken('%'))
        {
          UString var = parseIdent();
          if(rule->stringGlobals.find(var) != rule->stringGlobals.end())
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80710", {"var"}, {icu::UnicodeString(var.data())}));
          }
          nextToken("="_u);
          rule->stringGlobals[var] = parseClip();
        }
        else
        {
          UString var = parseIdent();
          if(rule->vars.find(var) != rule->vars.end())
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80370", {"attr"}, {icu::UnicodeString(var.data())}));
          }
          nextToken("="_u);
          rule->vars[var] = parseClip();
        }
        if(nextToken(","_u, "]"_u) == "]"_u)
        {
          break;
        }
      }
      eatSpaces();
    }
    currentLocType = LocTypeOutput;
    if(rule->result.size() > 1)
    {
      nextToken("{"_u);
    }
    unsigned int chunk_count = 0;
    while(chunk_count < rule->result.size())
    {
      eatSpaces();
      if(source.eof()) die(I18n(ARC_I18N_DATA, "arc").format("ARC80060"));
      switch(peekchar())
      {
        case '(':
          rule->output.push_back(parseOutputCond());
          chunk_count++;
          break;
        case '{':
          rule->output.push_back(chunkToCond(parseOutputChunk()));
          chunk_count++;
          break;
        case '_':
          rule->output.push_back(chunkToCond(parseOutputElement()));
          break;
        case '}':
          if(rule->result.size() == 1)
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80730"));
          }
          else if(chunk_count < rule->result.size())
          {
            die(I18n(ARC_I18N_DATA, "arc").format("ARC80740"));
          }
          break;
        default:
          rule->output.push_back(chunkToCond(parseOutputElement()));
          chunk_count++;
          break;
      }
    }
    if(rule->result.size() > 1)
    {
      nextToken("}"_u);
    }
    reductionRules.push_back(rule);
    if(nextToken("|"_u, ";"_u) == ";"_u)
    {
      break;
    }
  }
  currentLocType = LocTypeNone;
}

void
RTXCompiler::processRetagRules()
{
  for(auto rule : retagRules)
  {
    map<UString, vector<UString>> vals;
    UString src = rule[0].first;
    UString dest = rule[0].second;
    if(!PB.isAttrDefined(src) && collections.find(src) == collections.end())
    {
      I18n(ARC_I18N_DATA, "arc").error("ARC60750", {"src", "dest"},
        {icu::UnicodeString(src.data()), icu::UnicodeString(dest.data())}, false);
      continue;
    }
    if(!PB.isAttrDefined(dest) && collections.find(dest) == collections.end())
    {
      I18n(ARC_I18N_DATA, "arc").error("ARC60760", {"src", "dest"},
        {icu::UnicodeString(src.data()), icu::UnicodeString(dest.data())}, false);
      continue;
    }
    if(collections.find(src) == collections.end() || collections.find(dest) == collections.end()) continue;
    for(unsigned int i = 1; i < rule.size(); i++)
    {
      if(rule[i].first[0] == '[')
      {
        UString cat = rule[i].first.substr(2);
        if(collections.find(cat) == collections.end())
        {
          I18n(ARC_I18N_DATA, "arc").error("ARC60770", {"src", "dest", "cat"},
            {icu::UnicodeString(src.data()), icu::UnicodeString(dest.data()), icu::UnicodeString(cat.data())}, false);
          continue;
        }
        for(auto v : collections[cat]) vals[v].push_back(rule[i].second);
      }
      else
      {
        vals[rule[i].first].push_back(rule[i].second);
      }
    }
    if(src != dest)
    {
      for(auto a : collections[src])
      {
        if(vals.find(a) == vals.end())
        {
          bool found = false;
          for(auto b : collections[dest])
          {
            if(a == b)
            {
              found = true;
              break;
            }
          }
          if(!found)
          {
            I18n(ARC_I18N_DATA, "arc").error("ARC60780", {"src", "dest", "a"},
              {icu::UnicodeString(src.data()), icu::UnicodeString(dest.data()), icu::UnicodeString(a.data())}, false);
          }
        }
        else if(vals[a].size() > 1)
        {
          icu::UnicodeString temp;
          I18n(ARC_I18N_DATA, "arc").error("ARC60790", {"src", "dest", "a"},
            {icu::UnicodeString(src.data()), icu::UnicodeString(dest.data()), icu::UnicodeString(a.data())}, false);
          for(auto b : vals[a])
            temp += icu::UnicodeString(("\""_u +  b + "\", "_u).data());
          cerr << I18n(ARC_I18N_DATA, "arc").format("defaulting_to", {"vals", "def"},
            {temp, icu::UnicodeString(vals[a][0].data())}) << endl;
        }
      }
    }
  }
}

void
RTXCompiler::makePattern(int ruleid)
{
  Rule* rule = reductionRules[ruleid];
  if(rule->pattern.size() > longestPattern)
  {
    longestPattern = rule->pattern.size();
  }
  vector<vector<PatternElement*>> pat;
  for(unsigned int i = 0; i < rule->pattern.size(); i++)
  {
    vector<vector<UString>> tags;
    tags.push_back(vector<UString>());
    for(unsigned int j = 1; j < rule->pattern[i].size(); j++)
    {
      UString tg = rule->pattern[i][j];
      if(rule->pattern[i][j][0] == '[')
      {
        tg = tg.substr(1, tg.size()-2);
        if(collections.find(tg) == collections.end())
        {
          die(I18n(ARC_I18N_DATA, "arc").format("ARC80800", {"cat"}, {icu::UnicodeString(tg.data())}));
        }
        vector<vector<UString>> tmp;
        for(auto tls : tags)
        {
          for(auto t : collections[tg])
          {
            vector<UString> tmp2;
            tmp2.assign(tls.begin(), tls.end());
            tmp2.push_back(t);
            tmp.push_back(tmp2);
          }
        }
        tags.swap(tmp);
      }
      else
      {
        for(unsigned int t = 0; t < tags.size(); t++)
        {
          tags[t].push_back(tg);
        }
      }
    }
    for(unsigned int t = 0; t < tags.size(); t++) tags[t].push_back("*"_u);
    UString lem = rule->pattern[i][0];
    if(lem.size() == 0 || lem[0] != '$')
    {
      vector<PatternElement*> pel;
      for(auto tls : tags)
      {
        PatternElement* p = new PatternElement;
        p->lemma = lem;
        p->tags = tls;
        pel.push_back(p);
      }
      pat.push_back(pel);
    }
    else
    {
      vector<UString> lems = collections[lem.substr(1)];
      vector<PatternElement*> el;
      for(unsigned int j = 0; j < lems.size(); j++)
      {
        for(auto tls : tags)
        {
          PatternElement* p = new PatternElement;
          p->lemma = lems[j];
          p->tags = tls;
          el.push_back(p);
        }
      }
      pat.push_back(el);
    }
  }
  if(excluded.find(rule->name) == excluded.end())
  {
    PB.addRule(ruleid+1, rule->weight, pat, vector<UString>(1, rule->result[0]), rule->name);
  }
}

UString
RTXCompiler::compileString(UString s)
{
  UString ret;
  ret += STRING;
  ret += (UChar)s.size();
  ret += s;
  return ret;
}

UString
RTXCompiler::compileTag(UString s)
{
  if(s.size() == 0)
  {
    return compileString(s);
  }
  UString tag;
  tag += '<';
  tag += s;
  tag += '>';
  return compileString(StringUtils::substitute(tag, "."_u, "><"_u));
}

UString
RTXCompiler::compileClip(Clip* c, UString _dest = ""_u)
{
  if(c->src == -1 && c->part == "lu-count"_u)
  {
    return UString(1, LUCOUNT);
  }
  if(c->src == -2)
  {
    UString ret = processOutputChoice(c->choice);
    if(_dest == "lem"_u || _dest == "lemh"_u || _dest == "lemq"_u || _dest == "lemcase"_u)
    {
      ret += DISTAG;
    }
    return ret;
  }
  if(c->src == -3)
  {
    return PB.BCstring(c->varName) + FETCHVAR;
  }
  if(c->src != 0 && !(c->part == "lemcase"_u ||
      collections.find(c->part) != collections.end() || PB.isAttrDefined(c->part)))
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80810", {"attr"}, {icu::UnicodeString(c->part.data())}));
  }
  int src = (c->src == -1) ? 0 : c->src;
  bool useReplace = (currentLocType == LocTypeOutput);
  UString cl;
  if(src == -4)
  {
    cl += INT;
    cl += (UChar)globalVarNames[c->varName];
    cl += FETCHCHUNK;
  }
  else
  {
    cl += INT;
    cl += src;
    cl += PUSHINPUT;
  }
  if(c->part == "whole"_u || c->part == "chcontent"_u) return cl;
  cl += (c->part == "lemcase"_u) ? compileString("lem"_u) : compileString(c->part);
  UString ret = cl;
  UString undeftag;
  UString deftag;
  UString thedefault;
  UString blank;
  blank += DUP;
  blank += compileString(""_u);
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
  if(c->src == 0)
  {
    if(_dest == "lem"_u || _dest == "lemh"_u || _dest == "lemq"_u || _dest == "lemcase"_u ||
       (c->rewrite.size() > 0 &&
        (c->rewrite.back() == "lem"_u || c->rewrite.back() == "lemh"_u ||
         c->rewrite.back() == "lemq"_u || c->rewrite.back() == "lemcase"_u)))
    {
      return compileString(c->part);
    }
    else return compileTag(c->part);
  }
  else if(c->side == "sl"_u)
  {
    ret += SOURCECLIP;
    ret += blank;
    ret += (UChar)thedefault.size();
    ret += thedefault;
  }
  else if(c->side == "ref"_u)
  {
    ret += REFERENCECLIP;
    ret += blank;
    ret += (UChar)thedefault.size();
    ret += thedefault;
  }
  else if(c->side == "tl"_u || c->part == "lemcase"_u ||
          (c->src > 0 && !nodeIsSurface[currentRule->pattern[c->src-1][1]]))
  {
    ret += TARGETCLIP;
    ret += blank;
    ret += (UChar)thedefault.size();
    ret += thedefault;
  }
  else
  {
    UString fallback = thedefault;
    for (size_t i = 0; i < clip_order.size(); i++) {
      UString temp;
      temp += clip_order[i];
      temp += blank;
      if (i != 0) {
        temp += (UChar)(1 + cl.size() + fallback.size());
        temp += DROP;
        temp += cl;
      } else {
        temp += (UChar)(fallback.size());
      }
      fallback = temp + fallback;
    }
    ret += fallback;
  }
  if(c->part == "lemcase"_u)
  {
    ret += GETCASE;
  }
  UString src_cat = c->part;
  vector<UString> rewrite = c->rewrite;
  if(_dest.size() > 0 && rewrite.size() == 0 && currentLocType == LocTypeOutput)
  {
    rewrite.push_back(_dest);
  }
  else if(!_dest.empty() && ((!rewrite.empty() && _dest != rewrite.back()) ||
                             c->part != _dest))
  {
    rewrite.push_back(_dest);
  }
  for(auto dest : rewrite)
  {
    bool found = false;
    vector<pair<UString, UString>> rule;
    for(unsigned int i = 0; i < retagRules.size(); i++)
    {
      if(retagRules[i][0].first == src_cat && retagRules[i][0].second == dest)
      {
        found = true;
        rule = retagRules[i];
        break;
      }
    }
    if(!found && dest != src_cat)
    {
      if(dest == "lem"_u || dest == "lemh"_u || dest == "lemq"_u)
      {
        ret += DISTAG;
        return ret;
      }
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80820", {"src", "dest"},
        {icu::UnicodeString(src_cat.data()), icu::UnicodeString(dest.data())}));
    }
    UString check;
    for(unsigned int i = 1; i < rule.size(); i++)
    {
      UString cur;
      cur += DUP;
      cur += DISTAG;
      if(rule[i].first.size() > 2 &&
         rule[i].first[0] == '[' && rule[i].first[1] == ']')
      {
        cur += compileString(rule[i].first.substr(2));
        cur += IN;
      }
      else
      {
        cur += compileString(rule[i].first);
        cur += EQUAL;
      }
      cur += JUMPONFALSE;
      cur += (UChar)(rule[i].second.size() + (i == 1 ? 5 : 7));
      cur += DROP;
      cur += compileTag(rule[i].second);
      if(i != 1)
      {
        cur += JUMP;
        cur += (UChar)check.size();
      }
      check = cur + check;
    }
    ret += check;
    if(dest == "lemh"_u || dest == "lem"_u || dest == "lemq"_u)
    {
      if(dest != dest) ret += DISTAG;
    }
  }
  if(_dest == "lem"_u || _dest == "lemh"_u || _dest == "lemq"_u || _dest == "lemcase"_u)
  {
    ret += DISTAG;
  }
  return ret;
}

UString
RTXCompiler::compileClip(UString part, int pos, UString side = ""_u)
{
  Clip cl;
  cl.part = part;
  cl.src = pos;
  cl.side = side;
  return compileClip(&cl);
}

RTXCompiler::Clip*
RTXCompiler::processMacroClip(Clip* mac, OutputChunk* arg)
{
  if(mac == NULL) return NULL;
  Clip* ret = new Clip;
  ret->part = mac->part;
  ret->side = mac->side;
  ret->rewrite = mac->rewrite;
  if(mac->src == 1)
  {
    if(arg->vars.find(mac->part) != arg->vars.end())
    {
      Clip* other = arg->vars[mac->part];
      ret->part = other->part;
      ret->side = other->side;
      ret->rewrite.insert(ret->rewrite.begin(), other->rewrite.begin(), other->rewrite.end());
      ret->src = other->src;
      if(other->src == -2) ret->choice = other->choice;
    }
    else if(arg->pos == 0)
    {
      if(currentRule->grab_all != -1)
      {
        ret->src = currentRule->grab_all;
      }
      else
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80830", {"attr"}, {icu::UnicodeString(mac->part.data())}));
      }
    }
    else ret->src = arg->pos;
  }
  else if(mac->src == -2)
  {
    ret->src = mac->src;
    ret->choice = processMacroChoice(mac->choice, arg);
  }
  else ret->src = mac->src;
  return ret;
}

RTXCompiler::Cond*
RTXCompiler::processMacroCond(Cond* mac, OutputChunk* arg)
{
  Cond* ret = new Cond;
  ret->op = mac->op;
  if(mac->op == 0)
  {
    ret->val = processMacroClip(mac->val, arg);
  }
  else
  {
    if(mac->op != NOT) ret->left = processMacroCond(mac->left, arg);
    ret->right = processMacroCond(mac->right, arg);
  }
  return ret;
}

RTXCompiler::OutputChunk*
RTXCompiler::processMacroChunk(OutputChunk* mac, OutputChunk* arg)
{
  if(mac == NULL) return NULL;
  OutputChunk* ret = new OutputChunk;
  ret->mode = mac->mode;
  ret->lemma = mac->lemma;
  ret->tags = mac->tags;
  ret->getall = mac->getall;
  ret->pattern = mac->pattern;
  ret->conjoined = mac->conjoined;
  ret->interpolated = mac->interpolated;
  ret->nextConjoined = mac->nextConjoined;
  for(unsigned int i = 0; i < mac->children.size(); i++)
  {
    ret->children.push_back(processMacroChoice(mac->children[i], arg));
  }
  for(map<UString, Clip*>::iterator it = mac->vars.begin(),
          limit = mac->vars.end(); it != limit; it++)
  {
    ret->vars[it->first] = processMacroClip(it->second, arg);
  }
  if(mac->pos == 1)
  {
    ret->pos = arg->pos;
    for(map<UString, Clip*>::iterator it = arg->vars.begin(),
            limit = arg->vars.end(); it != limit; it++)
    {
      if(ret->vars.find(it->first) == ret->vars.end() || arg->pos == 0)
      {
        ret->vars[it->first] = it->second;
      }
    }
  }
  else
  {
    ret->pos = mac->pos;
  }
  return ret;
}

RTXCompiler::OutputChoice*
RTXCompiler::processMacroChoice(OutputChoice* mac, OutputChunk* arg)
{
  if(mac == NULL) return NULL;
  OutputChoice* ret = new OutputChoice;
  for(unsigned int i = 0; i < mac->conds.size(); i++)
  {
    ret->conds.push_back(processMacroCond(mac->conds[i], arg));
  }
  for(unsigned int i = 0; i < mac->nest.size(); i++)
  {
    ret->nest.push_back(processMacroChoice(mac->nest[i], arg));
  }
  for(unsigned int i = 0; i < mac->chunks.size(); i++)
  {
    ret->chunks.push_back(processMacroChunk(mac->chunks[i], arg));
  }
  for(unsigned int i = 0; i < mac->clips.size(); i++)
  {
    ret->clips.push_back(processMacroClip(mac->clips[i], arg));
  }
  return ret;
}

UString
RTXCompiler::processOutputChunk(OutputChunk* r)
{
  UString ret;
  if(r->conjoined && currentLocType == LocTypeOutput)
  {
    ret += CONJOIN;
    ret += OUTPUT;
  }
  if(r->mode == "_"_u)
  {
    ret += INT;
    ret += (UChar)r->pos;
    ret += BLANK;
    if(currentLocType == LocTypeOutput)
    {
      ret += OUTPUT;
    }
  }
  else if(r->mode == "$$"_u)
  {
    ret += INT;
    ret += (UChar)globalVarNames[r->pattern];
    ret += FETCHCHUNK;
    if(r->interpolated) ret += APPENDCHILD;
    if(currentLocType == LocTypeOutput)
    {
      ret += OUTPUT;
    }
  }
  else if(r->mode == "{}"_u || r->mode == "[]"_u || r->mode.empty())
  {
    for(unsigned int i = 0; i < r->children.size(); i++)
    {
      ret += processOutputChoice(r->children[i]);
    }
  }
  else if(r->mode == "#"_u || r->mode == "#@"_u)
  {
    bool interp = r->pos > currentRule->pattern.size();
    UString pos;
    if(!interp && r->pos != 0)
    {
      if(currentRule->pattern[r->pos-1].size() < 2)
      {
        die(I18n(ARC_I18N_DATA, "arc").format("ARC80840", {"element"}, {to_string(r->pos).c_str()}));
      }
      pos = currentRule->pattern[r->pos-1][1];
    }
    UString patname = (r->pattern.empty()) ? pos : r->pattern;
    pos = (pos.empty()) ? patname : pos;
    if(outputRules.find(patname) == outputRules.end())
    {
      if(interp)
      {
        ret += compileClip("whole"_u, r->pos, "tl"_u);
        if(r->interpolated) ret += APPENDCHILD;
        ret += OUTPUT;
        return ret;
      }
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80850", {"pattern"}, {icu::UnicodeString(patname.data())}));
    }
    vector<UString> pattern = outputRules[patname];

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

    if(r->interpolated)
    {
      ret += INT;
      ret += (UChar)0;
      ret += BLANK;
      ret += APPENDCHILD;
    }
    if(pattern.size() == 1 && pattern[0] == "macro"_u)
    {
      macroNameStack.push_back(patname);
      ret += processOutputChoice(processMacroChoice(macros[patname], r));
      macroNameStack.pop_back();
      return ret;
    }
    if(pattern.size() == 1 && pattern[0] == "%"_u)
    {
      ret += compileClip("whole"_u, r->pos, "tl"_u);
      if(currentLocType == LocTypeOutput && !r->nextConjoined)
      {
        ret += OUTPUT;
      }
      return ret;
    }
    if(currentSurface == APPENDSURFACE)
    {
      ret += CHUNK;
    }
    if(r->mode == "#@"_u)
    {
      unsigned int j;
      for(j = 0; j < r->lemma.size(); j++)
      {
        if(r->lemma[j] == '#') break;
      }
      if(j < r->lemma.size())
      {
        ret += compileString(r->lemma.substr(0, j));
        Clip* c = new Clip;
        c->part = r->lemma.substr(j);
        c->src = 0;
        c->rewrite.push_back("lemq"_u);
        r->vars["lemq"_u] = c;
      }
      else ret += compileString(r->lemma);
    }
    else if(r->vars.find("lem"_u) != r->vars.end())
    {
      ret += compileClip(r->vars["lem"_u], "lem"_u);
    }
    else if(r->vars.find("lemh"_u) != r->vars.end())
    {
      ret += compileClip(r->vars["lemh"_u], "lemh"_u);
    }
    else if(r->pos == 0)
    {
      if(currentRule->grab_all != -1)
      {
        ret += compileClip("lem"_u, currentRule->grab_all, "tl"_u);
      }
      else
      {
        ret += compileString("default"_u);
      }
    }
    else
    {
      Clip* c = new Clip;
      c->part = "lemh"_u;
      c->src = r->pos;
      c->side = "tl"_u;
      c->rewrite.push_back("lemh"_u);
      ret += compileClip(c, "lemh"_u);
    }
    if(r->vars.find("lemcase"_u) != r->vars.end())
    {
      ret += compileClip(r->vars["lemcase"_u], "lemcase"_u);
      ret += SETCASE;
    }
    ret += currentSurface;
    for(unsigned int i = 0; i < pattern.size(); i++)
    {
      if(pattern[i] == "_"_u)
      {
        if(r->vars.find("pos_tag"_u) != r->vars.end())
        {
          ret += compileClip(r->vars["pos_tag"_u]);
        }
        else if(r->pos != 0)
        {
          //ret += compileTag(currentRule->pattern[r->pos-1][1]);
          ret += compileClip("pos_tag"_u, r->pos, "tl"_u);
        }
        else
        {
          ret += compileTag(pos);
        }
        ret += currentSurface;
      }
      else if(pattern[i][0] == '<')
      {
        ret += compileString(pattern[i]);
        ret += currentSurface;
      }
      else
      {
        UString ret_temp;
        vector<UString> ops = altAttrs[pattern[i]];
        if(ops.size() == 0)
        {
          ops.push_back(pattern[i]);
        }
        UString var;
        for(unsigned int v = 0; v < ops.size(); v++)
        {
          if(r->vars.find(ops[v]) != r->vars.end())
          {
            var = ops[v];
            break;
          }
        }
        if(var.empty() && r->pos != 0)
        {
          Clip* cl = new Clip;
          cl->src = r->pos;
          cl->part = pattern[i];
          if(currentLocType == LocTypeOutput) cl->rewrite.push_back(pattern[i]);
          ret_temp += compileClip(cl, pattern[i]);
        }
        else if(var.empty())
        {
          bool found = false;
          for(unsigned int t = 0; t < parentTags.size(); t++)
          {
            if(parentTags[t] == pattern[i])
            {
              ret_temp += compileTag(StringUtils::itoa(t+1));
              found = true;
              break;
            }
          }
          if(!found)
          {
            if(r->pos == 0 && currentRule->grab_all != -1)
            {
              ret_temp += compileClip(pattern[i], currentRule->grab_all);
            }
            else if(attrDefaults.find(pattern[i]) != attrDefaults.end())
            {
              ret_temp += compileTag(attrDefaults[pattern[i]].first);
            }
            else if(r->pos == 0)
            {
              die(I18n(ARC_I18N_DATA, "arc").format("ARC80860", {"pattern"}, {icu::UnicodeString(pattern[i].data())}));
            }
            else
            {
              ret_temp += compileClip(pattern[i], r->pos);
            }
          }
        }
        else
        {
          ret_temp += compileClip(r->vars[var], pattern[i]);
        }
        if(currentLocType == LocTypeOutput && noOverwrite[pattern[i]].size() > 0)
        {
          ret += compileClip(pattern[i], r->pos, "tl"_u);
          ret += DUP;
          ret += compileString(pattern[i] + " over"_u);
          ret += IN;
          ret += JUMPONTRUE;
          ret += (UChar)(1+ret_temp.size());
          ret += DROP;
        }
        ret += ret_temp;
        ret += currentSurface;
      }
    }
    if(r->vars.find("lemq"_u) != r->vars.end())
    {
      ret += compileClip(r->vars["lemq"_u], "lemq"_u);
      ret += currentSurface;
    }
    else if(r->pos != 0)
    {
      ret += compileClip("lemq"_u, r->pos, "tl"_u);
      ret += currentSurface;
    }
    if(r->pos != 0)
    {
      ret += compileClip("whole"_u, r->pos, "tl"_u);
      ret += APPENDALLCHILDREN;
      ret += INT;
      ret += (UChar)r->pos;
      ret += GETRULE;
      ret += INT;
      ret += (UChar)0;
      ret += SETRULE;
    }
    if(r->interpolated) ret += APPENDCHILD;
    if(currentLocType == LocTypeOutput && !r->nextConjoined)
    {
      ret += OUTPUT;
    }
    else if(currentLoc == LocVarSet)
    {
      ret += INT;
      ret += (UChar)currentVar;
      ret += SETCHUNK;
    }
  }
  else
  {
    if(r->interpolated)
    {
      ret += INT;
      ret += (UChar)0;
      ret += BLANK;
      ret += APPENDCHILD;
    }
    ret += CHUNK;
    ret += compileString(r->lemma);
    if(r->vars.find("lemcase"_u) != r->vars.end())
    {
      ret += compileClip(r->vars["lemcase"_u]);
      ret += SETCASE;
    }
    ret += currentSurface;
    for(unsigned int i = 0; i < r->tags.size(); i++)
    {
      ret += compileClip(r->vars[r->tags[i]]);
      ret += currentSurface;
    }
    if(r->interpolated)
    {
      ret += APPENDCHILD;
    }
    if(currentLocType == LocTypeOutput)
    {
      ret += OUTPUT;
    }
    else if(currentLoc == LocVarSet)
    {
      ret += INT;
      ret += (UChar)currentVar;
      ret += SETCHUNK;
    }
  }
  return ret;
}

UString
RTXCompiler::processCond(Cond* cond)
{
  UString ret;
  if(cond == NULL)
  {
    ret += PUSHTRUE;
    return ret;
  }
  if(cond->op == AND)
  {
    if(cond->left->op == 0 || cond->right->op == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80870"));
    }
  }
  else if(cond->op == OR)
  {
    if(cond->left->op == 0 || cond->right->op == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80880"));
    }
  }
  else if(cond->op == NOT)
  {
    if(cond->right->op == 0)
    {
      die(I18n(ARC_I18N_DATA, "arc").format("ARC80890"));
    }
  }
  else if(cond->op != 0 && (cond->left->op != 0 || cond->right->op != 0))
  {
    die(I18n(ARC_I18N_DATA, "arc").format("ARC80900"));
  }
  else if(cond->op == EQUAL)
  {
    UString lit;
    UString attr;
    bool rew = false;
    Clip* l = cond->left->val;
    if(l->src == 0) lit = l->part;
    else
    {
      attr = l->part;
      if(l->rewrite.size() > 0) rew = true;
    }
    Clip* r = cond->right->val;
    if(r->src == 0) lit = r->part;
    else
    {
      attr = r->part;
      if(r->rewrite.size() > 0) rew = true;
    }
    if(lit.size() > 0 && attr.size() > 0 && !rew
        && collections.find(attr) != collections.end())
    {
      bool found = false;
      for(auto tag : collections[attr])
      {
        if(tag == lit)
        {
          found = true;
          break;
        }
      }
      if(!found) die(I18n(ARC_I18N_DATA, "arc").format("ARC80910", {"element", "attr"},
        {icu::UnicodeString(lit.data()), icu::UnicodeString(attr.data())}));
    }
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
      if(cond->val->part != "lem"_u && cond->val->part != "lemh"_u && cond->val->part != "lemq"_u)
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

UString
RTXCompiler::processOutputChoice(OutputChoice* choice)
{
  UString ret;
  if(choice->nest.back() != NULL)
  {
    ret += processOutputChoice(choice->nest.back());
  }
  else if(choice->clips.back() != NULL)
  {
    ret += compileClip(choice->clips.back());
  }
  else
  {
    ret += processOutputChunk(choice->chunks.back());
  }
  int n = choice->conds.size();
  for(int i = 1; i <= n; i++)
  {
    UString act;
    if(choice->nest[n-i] != NULL)
    {
      act = processOutputChoice(choice->nest[n-i]);
    }
    else if(choice->clips[n-i] != NULL)
    {
      act = compileClip(choice->clips[n-i]);
    }
    else
    {
      act = processOutputChunk(choice->chunks[n-i]);
    }
    act += JUMP;
    act += (UChar)ret.size();
    UString cond = processCond(choice->conds[n-i]);
    cond += JUMPONFALSE;
    cond += (UChar)act.size();
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
    if(summarizing)
    {
      if(rule->name.size() > 0) cerr << "\"" << rule->name << "\": ";
      for(auto it : rule->result) cerr << it << " ";
      cerr << "->";
      for(auto it : rule->pattern) cerr << " " << it[1];
      cerr << endl;
    }
    currentRule = rule;
    currentChunk = NULL;
    currentChoice = NULL;
    makePattern(ruleid);
    UString comp;
    if(rule->cond != NULL)
    {
      currentLocType = LocTypeInput;
      comp = processCond(rule->cond) + JUMPONTRUE + (UChar)1 + REJECTRULE;
    }
    for(auto it : rule->globals)
    {
      currentLocType = LocTypeInput;
      currentLoc = LocVarSet;
      currentVar = globalVarNames[it.first];
      currentSurface = APPENDSURFACE;
      comp += processOutputChoice(it.second);
    }
    for(auto it : rule->stringGlobals)
    {
      comp += compileClip(it.second);
      comp += compileString(it.first);
      comp += SETVAR;
    }
    currentLoc = LocTopLevel;
    vector<UString> outcomp;
    outcomp.resize(rule->pattern.size());
    parentTags.clear();
    unsigned int patidx = 0;
    for(unsigned int i = 0; i < rule->output.size(); i++)
    {
      currentLocType = LocTypeInput;
      OutputChoice* cur = rule->output[i];
      if(cur->chunks.size() == 1 && cur->chunks[0]->mode == "_"_u)
      {
        currentSurface = APPENDSURFACE;
        comp += processOutputChoice(cur);
      }
      else if(cur->chunks.size() == 1 && cur->chunks[0]->mode == "#"_u)
      {
        currentSurface = APPENDSURFACE;
        comp += processOutputChoice(cur);
        patidx++;
      }
      else
      {
        OutputChunk* ch = new OutputChunk;
        ch->mode = "#"_u;
        ch->pos = 0;
        ch->getall = true;
        ch->vars = rule->vars;
        if(ch->vars.find("lemcase"_u) == ch->vars.end())
        {
          Clip* lemcase = new Clip;
          lemcase->src = 1;
          lemcase->part = "lemcase"_u;
          lemcase->side = "tl"_u;
          ch->vars["lemcase"_u] = lemcase;
        }
        ch->conjoined = false;
        ch->interpolated = false;
        ch->nextConjoined = false;
        ch->pattern = rule->result[patidx];
        currentSurface = APPENDSURFACE;
        comp += processOutputChunk(ch);
        if(rule->output_sl.size() > patidx && rule->output_sl[patidx] != NULL)
        {
          currentSurface = APPENDSURFACESL;
          comp += processOutputChoice(rule->output_sl[patidx]);
        }
        if(rule->output_ref.size() > patidx && rule->output_ref[patidx] != NULL)
        {
          currentSurface = APPENDSURFACEREF;
          comp += processOutputChoice(rule->output_ref[patidx]);
        }
        comp += INT;
        comp += (UChar)outputBytecode.size();
        comp += INT;
        comp += (UChar)0;
        comp += SETRULE;
        comp += APPENDALLINPUT;
        parentTags = outputRules[ch->pattern];
        currentLocType = LocTypeOutput;
        currentSurface = APPENDSURFACE;
        outputBytecode.push_back(processOutputChoice(cur));
        if(rule->name.size() > 0)
        {
          PB.outRuleNames.push_back(rule->name + " - line "_u + StringUtils::itoa(rule->line));
        }
        else
        {
          PB.outRuleNames.push_back("line "_u + StringUtils::itoa(rule->line));
        }
        parentTags.clear();
        patidx++;
      }
      comp += OUTPUT;
    }
    rule->compiled = comp;
  }
}

void
RTXCompiler::loadLex(const string& fname)
{
  PB.loadLexFile(fname);
}

void
RTXCompiler::read(const string &fname)
{
  currentLine = 1;
  sourceFile = fname;
  source.open_or_exit(fname.c_str());
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
  processRetagRules();
  processRules();
  for (auto& it : collections) {
    set<UString> vals;
    for (auto& it2 : it.second) {
      vals.insert(it2);
    }
    PB.addList(it.first, vals);
    PB.addAttr(it.first, vals);
  }
}

void
RTXCompiler::write(const string &fname)
{
  FILE *out = fopen(fname.c_str(), "wb");
  if(out == NULL)
  {
    I18n(ARC_I18N_DATA, "arc").error("ARC80020", {"file"}, {fname.c_str()}, true);
  }

  vector<pair<int, UString>> inRules;
  for(unsigned int i = 0; i < reductionRules.size(); i++)
  {
    inRules.push_back(make_pair(2*reductionRules[i]->pattern.size() - 1,
                                reductionRules[i]->compiled));
    if(reductionRules[i]->name.size() > 0)
    {
      PB.inRuleNames.push_back(reductionRules[i]->name + " - line "_u + StringUtils::itoa(reductionRules[i]->line));
    }
    else
    {
      PB.inRuleNames.push_back("line "_u + StringUtils::itoa(reductionRules[i]->line));
    }
  }

  PB.chunkVarCount = globalVarNames.size();

  PB.write(out, longestPattern, inRules, outputBytecode);

  fclose(out);
}

void
RTXCompiler::printStats()
{
  cout << "Rules: " << reductionRules.size() << endl;
  cout << "Macros: " << macros.size() << endl;
  cout << "Global variables: " << globalVarNames.size() << endl;
}
