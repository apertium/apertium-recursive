#include <rtx_config.h>
#include <apertium/trx_reader.h>
#include <lttoolbox/xml_parse_util.h>
#include <lttoolbox/compression.h>
#include <bytecode.h>
#include <trx_compiler.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <lttoolbox/string_utils.h>
#include <lttoolbox/xml_walk_util.h>

using namespace std;

TRXCompiler::TRXCompiler()
{
  longestPattern = 0;
}

TRXCompiler::~TRXCompiler()
{
  // TODO
}

UString
name(xmlNode* node)
{
  return to_ustring((const char*) node->name);
}

bool
nameIs(xmlNode* node, const char* name)
{
  return !xmlStrcmp(node->name, (const xmlChar*) name);
}

void
TRXCompiler::die(xmlNode* node, const char* fmt, ...)
{
  UFILE* out = u_finit(stderr, NULL, NULL);
  u_fprintf(out, "Error in %S on line %d: ",
            to_ustring((char*) curDoc->URL).c_str(), node->line);
  va_list argptr;
  va_start(argptr, fmt);
  u_vfprintf(out, fmt, argptr);
  va_end(argptr);
  u_fputc('\n', out);
  exit(EXIT_FAILURE);
}

void
TRXCompiler::warn(xmlNode* node, const char* fmt, ...)
{
  UFILE* out = u_finit(stderr, NULL, NULL);
  u_fprintf(out, "Warning in %S on line %d: ",
            to_ustring((char*) curDoc->URL).c_str(), node->line);
  va_list argptr;
  va_start(argptr, fmt);
  u_vfprintf(out, fmt, argptr);
  va_end(argptr);
  u_fputc('\n', out);
}

void
TRXCompiler::compile(string file)
{
  curDoc = xmlReadFile(file.c_str(), NULL, 0);
  if(curDoc == NULL)
  {
    cerr << "Error: Could not parse file '" << file << "'." << endl;
    exit(EXIT_FAILURE);
  }
  processFile(xmlDocGetRootElement(curDoc));
}

void
TRXCompiler::processFile(xmlNode* node)
{
  for (auto i : children(node)) {
    if(nameIs(i, "section-def-cats")) {
      processCats(i);
    } else if(nameIs(i, "section-def-attrs")) {
      processAttrs(i);
    } else if(nameIs(i, "section-def-vars")) {
      processVars(i);
    } else if(nameIs(i, "section-def-lists")) {
      processLists(i);
    } else if(nameIs(i, "section-def-macros")) {
      gatherMacros(i);
    } else if(nameIs(i, "section-rules")) {
      processRules(i);
    }
  }
}

UString
TRXCompiler::requireAttr(xmlNode* node, const char* attr)
{
  for (xmlAttr* a = node->properties; a != NULL; a = a->next) {
    if (!xmlStrcmp(a->name, (const xmlChar*) attr)) {
      return to_ustring((const char*) a->children->content);
    }
  }
  die(node, "Expected attribute '%S'", to_ustring(attr).c_str());
  return ""_u; // since die() exits, this will not be returned
  // but we each do our part to keep the typechecker happy...
}

int
TRXCompiler::getPos(xmlNode* node, bool isBlank = false)
{
  UString v;
  if(nameIs(node, "b")) {
    v = getattr(node, "pos");
    if (v.empty()) {
      return 0;
    }
  } else {
    v = requireAttr(node, "pos");
  }
  if(v.empty())
  {
    if(isBlank)
    {
      return 0;
    }
    else
    {
      die(node, "Cannot interpret empty pos attribute.");
    }
  }
  for(unsigned int i = 0; i < v.size(); i++)
  {
    if(!isdigit(v[i]))
    {
      if(isBlank)
      {
        warn(node, "Disregarding non-integer position.");
        return 0;
      }
      die(node, "Position must be an integer.");
    }
  }
  int ret = StringUtils::stoi(v);
  if(inOutput && ret == 0)
  {
    return ret;
  }
  if(inOutput && macroPosShift.size() == 0 && ret >= 0)
  {
    return ret;
  }
  int limit = macroPosShift.size() > 0 ? macroPosShift.back().size() : curPatternSize;
  if(ret <= 0 || ret > limit || (ret == limit && isBlank))
  {
    if(isBlank)
    {
      warn(node, "Disregarding out-of-bounds position.");
      return 0;
    }
    die(node, "Position %d is out of bounds.", ret);
  }
  if(macroPosShift.size() > 0)
  {
    ret = macroPosShift.back()[ret-1];
  }
  return ret;
}

void
TRXCompiler::processCats(xmlNode* node)
{
  for (auto cat : children(node)) {
    if (!nameIs(cat, "def-cat")) {
      warn(cat, "Unexpected tag in section-def-cats - ignoring");
      continue;
    }
    UString pat_name = requireAttr(cat, "n");
    vector<PatternElement*> pat;
    for (auto item : children(cat)) {
      if (!nameIs(item, "cat-item")) {
        warn(cat, "Unexpected tag <%S> in def-cat - ignoring", name(item).c_str());
        continue;
      }
      PatternElement* cur = new PatternElement;
      cur->lemma = getattr(item, "lemma");
      UString tags = requireAttr(item, "tags");
      if(tags.empty()) tags = "UNKNOWN:INTERNAL"_u;
      cur->tags = StringUtils::split(tags, "."_u);
      pat.push_back(cur);
    }
    if(patterns.find(pat_name) != patterns.end()) {
      warn(cat, "Redefinition of pattern '%S', using later value", pat_name.c_str());
    }
    patterns[pat_name] = pat;
  }
}

void
TRXCompiler::processAttrs(xmlNode* node)
{
  for (auto cat : children(node)) {
    if (!nameIs(cat, "def-attr")) {
      warn(cat, "Unexpected tag in section-def-attrs - ignoring");
      continue;
    }
    UString name = getattr(cat, "n");
    set<UString> ats;
    for (auto item : children(cat)) {
      if (!nameIs(item, "attr-item")) {
        warn(item, "Unexpected tag in def-attr - ignoring");
        continue;
      }
      ats.insert(getattr(item, "tags"));
    }
    if(PB.isAttrDefined(name))
    {
      warn(cat, "Redefinition of attribute '%S' - using later definition", name.c_str());
    }
    PB.addAttr(name, ats);
  }
}

void
TRXCompiler::processVars(xmlNode* node)
{
  for (auto var : children(node)) {
    if (!nameIs(var, "def-var")) {
      warn(var, "Unexpected tag in section-def-vars - ignoring");
      continue;
    }
    UString name = requireAttr(var, "n");
    vars[name] = getattr(var, "v");
    PB.addVar(name, vars[name]);
  }
}

void
TRXCompiler::processLists(xmlNode* node)
{
  for (auto cat : children(node)) {
    if (!nameIs(cat, "def-list")) {
      warn(cat, "Unexpected tag in section-def-lists - ignoring");
      continue;
    }
    UString name = getattr(cat, "n");
    set<UString> ats;
    for (auto item : children(cat)) {
      if (!nameIs(item, "list-item")) {
        warn(item, "Unexpected tag in def-list - ignoring");
        continue;
      }
      ats.insert(getattr(item, "v"));
    }
    if(lists.find(name) != lists.end())
    {
      warn(cat, "Redefinition of list '%S' - using later definition", name.c_str());
    }
    lists[name] = ats;
    PB.addList(name, ats);
  }
}

void
TRXCompiler::gatherMacros(xmlNode* node)
{
  for (auto mac : children(node)) {
    if (!nameIs(mac, "def-macro")) {
      warn(mac, "Unexpected tag in section-def-macros - ignoring");
      continue;
    }
    UString name = requireAttr(mac, "n");
    int npar = StringUtils::stoi(requireAttr(mac, "npar"));
    if(macros.find(name) != macros.end())
    {
      warn(mac, "Redefinition of macro '%S' - using later definition", name.c_str());
    }
    macros[name] = make_pair(npar, mac);
  }
}

void
TRXCompiler::processRules(xmlNode* node)
{
  for (auto rule : children(node)) {
    if(xmlStrcmp(rule->name, (const xmlChar*) "rule"))
    {
      warn(rule, "Ignoring non-<rule> element in <section-rules>.");
      continue;
    }
    if (getattr(rule, "i") == "yes"_u) {
      continue;
    }
    curPatternSize = 0;
    localVars.clear();

    UString id = getattr(rule, "id");
    UString weight = getattr(rule, "weight");
    UString firstChunk = getattr(rule, "firstChunk");
    if(firstChunk.empty()) firstChunk = "*"_u;

    xmlNode* action = NULL;
    UString outputAction;
    bool pat = false;
    UString assertClause;
    for (auto part : children(rule)) {
      if(nameIs(part, "local"))
      {
        for (auto var : children(rule)) {
          if(nameIs(var, "var")) {
            localVars.insert(requireAttr(var, "n"));
          }
        }
      }
      else if(nameIs(part, "pattern"))
      {
        if(pat)
        {
          die(rule, "Rule cannot have multiple <pattern>s.");
        }
        pat = true;
        vector<vector<PatternElement*>> pls;
        for (auto pi : children(part)) {
          if (!nameIs(pi, "pattern-item")) {
            warn(pi, "Ignoring non-<pattern-item> in <pattern>.");
            continue;
          }
          curPatternSize++;
          UString name = requireAttr(pi, "n");
          if(patterns.find(name) == patterns.end())
          {
            die(pi, "Unknown pattern '%S'.", name.c_str());
          }
          else
          {
            pls.push_back(patterns[name]);
          }
        }
        if(curPatternSize == 0)
        {
          die(rule, "Rule cannot have empty pattern.");
        }
        if(curPatternSize > longestPattern)
        {
          longestPattern = curPatternSize;
        }
        if(excludedRules.find(id) == excludedRules.end())
        {
          PB.addRule(inputRules.size() + 1, (weight.size() > 0 ? StringUtils::stod(weight) : 0.0), pls, StringUtils::split(firstChunk, " "_u), id);
        }
        inputRuleSizes.push_back(pls.size());
      }
      else if(nameIs(part, "assert"))
      {
        bool firstAssert = (assertClause.size() == 0);
        for (auto clause : children(part)) {
          assertClause += processCond(clause);
          if(!firstAssert)
          {
            assertClause += AND;
          }
          firstAssert = false;
        }
      }
      else if(nameIs(part, "action"))
      {
        if(action != NULL)
        {
          die(rule, "Rule cannot have multiple <action>s.");
        }
        action = part;
      }
      else if(nameIs(part, "output-action"))
      {
        if(outputAction.size() > 0)
        {
          die(part, "Rule cannot have multiple <output-action>s.");
        }
        inOutput = true;
        for (auto state : children(part)) {
          outputAction += processStatement(state);
        }
      }
      else
      {
        warn(part, "Unknown element <%S> in <rule>, ignoring.", name(part).c_str());
      }
    }
    if(!pat)
    {
      die(rule, "Rule must have <pattern>.");
    }
    if(action == NULL)
    {
      die(rule, "Rule must have <action>.");
    }
    else
    {
      if(outputAction.size() > 0)
      {
        currentOutputRule = (int)outputRules.size();
        outputRules.push_back(outputAction);
      }
      else
      {
        currentOutputRule = -1;
      }
      inOutput = false;
      UString actionStr;
      if(assertClause.size() > 0)
      {
        actionStr = assertClause;
        actionStr += JUMPONTRUE;
        actionStr += (UChar)1;
        actionStr += REJECTRULE;
      }
      for (auto state : children(action)) {
        actionStr += processStatement(state);
      }
      inputRules.push_back(actionStr);
    }
  }
}

UString
TRXCompiler::processStatement(xmlNode* node)
{
  if (getattr(node, "i") == "yes"_u) {
    return ""_u;
  }
  UString ret;
  if(nameIs(node, "let") || nameIs(node, "modify-case")) {
    xmlNode* var = NULL;
    UString val;
    bool val_is_clip = false;
    for (auto n : children(node)) {
      if(var == NULL)
      {
        var = n;
      }
      else if(val.size() == 0)
      {
        val_is_clip = (nameIs(n, "clip"));
        val = processValue(n);
      }
      else {
        die(node, "<%S> cannot have more than two children.", name(node).c_str());
      }
    }
    if(val.size() == 0)
    {
      die(node, "<%S> must have two children.", name(node).c_str());
    }
    if(nameIs(var, "var"))
    {
      UString vname = requireAttr(var, "n");
      if(vars.find(vname) == vars.end())
      {
        die(var, "Undefined variable '%S'.", vname.c_str());
      }
      if(nameIs(node, "modify-case"))
      {
        ret += STRING;
        ret += (UChar)vname.size();
        ret += vname;
        ret += FETCHVAR;
        ret += val;
        ret += SETCASE;
      }
      else
      {
        ret += val;
      }
      ret += STRING;
      ret += (UChar)vname.size();
      ret += vname;
      ret += SETVAR;
    }
    else if(nameIs(var, "clip"))
    {
      UString side = getattr(var, "side");
      if(!(side.empty() || side == "tl"_u))
      {
        warn(var, "Cannot set side '%S', setting 'tl' instead.", side.c_str());
      }
      UString part = requireAttr(var, "part");
      if(!PB.isAttrDefined(part))
      {
        die(var, "Unknown attribute '%S'", part.c_str());
      }
      UString set_str;
      set_str += PB.BCstring(part);
      set_str += INT;
      set_str += (UChar)getPos(var);
      set_str += SETCLIP;
      if(nameIs(node, "modify-case"))
      {
        ret += INT;
        ret += (UChar)getPos(var);
        ret += PUSHINPUT;
        ret += STRING;
        ret += (UChar)part.size();
        ret += part;
        ret += TARGETCLIP;
        ret += val;
        ret += SETCASE;
        ret += set_str;
      }
      else
      {
        ret = val;
        if(val_is_clip)
        {
          UString cond;
          cond += DUP;
          cond += PB.BCstring(""_u);
          cond += EQUAL;
          ret += PB.BCifthenelse(cond, UString(1, DROP), set_str);
        }
        else
        {
          ret += set_str;
        }
      }
    }
    else
    {
      die(node, "Cannot set value of <%S>.", name(var).c_str());
    }
  }
  else if(nameIs(node, "out"))
  {
    for (auto o : children(node)) {
      ret += processValue(o);
      ret += OUTPUT;
    }
  }
  else if(nameIs(node, "choose"))
  {
    ret = processChoose(node);
  }
  else if(nameIs(node, "call-macro"))
  {
    // TODO: DTD implies number of arguments can be variable
    UString name = requireAttr(node, "n");
    if(macros.find(name) == macros.end())
    {
      die(node, "Unknown macro '%S'.", name.c_str());
    }
    vector<int> temp;
    for (auto param : children(node)) {
      if (nameIs(param, "with-param")) {
        temp.push_back(getPos(param));
      } else {
        warn(param, "Ignoring non-<with-param> in <call-macro>");
      }
    }
    unsigned int shouldbe = macros[name].first;
    if(shouldbe < temp.size())
    {
      die(node, "Too many parameters, macro '%S' expects %d, got %d.", name.c_str(), shouldbe, temp.size());
    }
    if(shouldbe > temp.size())
    {
      die(node, "Not enough parameters, macro '%S' expects %d, got %d.", name.c_str(), shouldbe, temp.size());
    }
    macroPosShift.push_back(temp);
    xmlNode* mac = macros[name].second;
    for (auto state : children(mac)) {
      ret += processStatement(state);
    }
    macroPosShift.pop_back();
  }
  else if(nameIs(node, "append"))
  {
    // TODO: DTD says this can append to a clip
    UString name = requireAttr(node, "n");
    if(vars.find(name) == vars.end() && localVars.find(name) == localVars.end())
    {
      die(node, "Unknown variable '%S'.", name.c_str());
    }
    ret += STRING;
    ret += (UChar)name.size();
    ret += name;
    ret += FETCHVAR;
    for (auto part : children(node)) {
      ret += processValue(part);
      ret += CONCAT;
    }
    ret += STRING;
    ret += (UChar)name.size();
    ret += name;
    ret += SETVAR;
  }
  else if(nameIs(node, "reject-current-rule"))
  {
    ret += REJECTRULE;
  }
  else
  {
    die(node, "Unrecognized statement '%S'", name(node).c_str());
  }
  return ret;
}

UString
TRXCompiler::processValue(xmlNode* node)
{
  UString ret;
  if(nameIs(node, "b"))
  {
    ret += INT;
    ret += (UChar)getPos(node);
    ret += BLANK;
  }
  else if(nameIs(node, "clip"))
  {
    ret += INT;
    ret += (UChar)getPos(node);
    ret += PUSHINPUT;
    ret += STRING;
    UString part = requireAttr(node, "part");
    if(!PB.isAttrDefined(part))
    {
      die(node, "Unknown attribute '%S'", part.c_str());
    }
    ret += (UChar)part.size();
    ret += part;
    UString side = getattr(node, "side");
    if(side == "sl"_u)
    {
      ret += SOURCECLIP;
    }
    else if(side == "tl"_u || side.empty())
    {
      ret += TARGETCLIP;
    }
    else if(side == "ref"_u)
    {
      ret += REFERENCECLIP;
    }
    else
    {
      warn(node, "Unknown clip side '%S', defaulting to 'tl'.", side.c_str());
      ret += TARGETCLIP;
    }
    UString link = getattr(node, "link-to");
    if(link.size() > 0)
    {
      ret += DUP;
      ret += STRING;
      ret += (UChar)0;
      ret += EQUAL;
      ret += JUMPONTRUE;
      ret += (UChar)(link.size() + 5);
      ret += DROP;
      ret += STRING;
      ret += (UChar)(link.size() + 2);
      ret += '<';
      ret += link;
      ret += '>';
    }
    // TODO: what does attribute "queue" do?
  }
  else if(nameIs(node, "lit"))
  {
    ret += STRING;
    UString v = requireAttr(node, "v");
    ret += (UChar)v.size();
    ret += v;
  }
  else if(nameIs(node, "lit-tag"))
  {
    ret += STRING;
    UString v = "<"_u + requireAttr(node, "v") + ">"_u;
    v = StringUtils::substitute(v, "."_u, "><"_u);
    if(v == "<>"_u)
    {
      v.clear();
    }
    ret += (UChar)v.size();
    ret += v;
  }
  else if(nameIs(node, "var"))
  {
    ret += STRING;
    UString v = requireAttr(node, "n");
    if(vars.find(v) == vars.end() && localVars.find(v) == localVars.end())
    {
      die(node, "Unknown variable '%S'.", v.c_str());
    }
    ret += (UChar)v.size();
    ret += v;
    ret += FETCHVAR;
  }
  else if(nameIs(node, "get-case-from"))
  {
    for (auto c : children(node)) {
      if (ret.empty()) {
        ret.append(processValue(c));
      } else {
        die(node, "<get-case-from> cannot have multiple children.");
      }
    }
    if(ret.size() == 0)
    {
      die(node, "<get-case-from> cannot be empty.");
    }
    ret += INT;
    ret += (UChar)getPos(node);
    ret += PUSHINPUT;
    ret += STRING;
    ret += (UChar)3;
    ret += "lem"_u;
    ret += (inOutput ? TARGETCLIP : SOURCECLIP);
    ret += SETCASE;
  }
  else if(nameIs(node, "case-of"))
  {
    ret += INT;
    ret += getPos(node);
    ret += PUSHINPUT;
    ret += STRING;
    UString part = requireAttr(node, "part");
    ret += (UChar)part.size();
    ret += part;
    UString side = getattr(node, "side");
    if(side == "sl"_u)
    {
      ret += SOURCECLIP;
    }
    else if(side == "tl"_u || side.empty())
    {
      ret += TARGETCLIP;
    }
    else if(side == "ref"_u)
    {
      ret += REFERENCECLIP;
    }
    else
    {
      warn(node, "Unknown side '%S', defaulting to target.", side.c_str());
      ret += TARGETCLIP;
    }
    ret += GETCASE;
  }
  else if(nameIs(node, "concat"))
  {
    for (auto c : children(node)) {
      unsigned int l = ret.size();
      ret += processValue(c);
      if(l > 0 && ret.size() > l) {
        ret += CONCAT;
      }
    }
  }
  else if(nameIs(node, "lu"))
  {
    ret += CHUNK;
    UString children_str;
    for (auto p : children(node)) {
      if(nameIs(p, "clip")) {
        UString part = getattr(p, "part");
        if(part == "whole"_u || part == "chcontent"_u || part == "content"_u) {
          children_str += INT;
          children_str += (UChar)getPos(p);
          children_str += PUSHINPUT;
          children_str += APPENDALLCHILDREN;
          if(part != "whole"_u) continue;
        }
      }
      ret += processValue(p);
      ret += APPENDSURFACE;
    }
    ret += children_str;
  }
  else if(nameIs(node, "mlu"))
  {
    ret += CHUNK;
    for (auto lu : children(node)) {
      if (!nameIs(lu, "lu")) {
        die(node, "<mlu> can only contain <lu>s.");
      }
      if(ret.size() > 1)
      {
        ret += CONJOIN;
        ret += APPENDCHILD;
        // apertium/transfer.cc has checks against appending '' UString or '+#'
        // TODO?
      }
      ret += processValue(lu);
      ret += APPENDCHILD;
    }
  }
  else if(nameIs(node, "chunk"))
  {
    ret += CHUNK;
    for (auto part : children(node)) {
      if(nameIs(part, "source"))
      {
        for (auto seg : children(part)) {
          ret += processValue(seg);
          ret += APPENDSURFACESL;
        }
      }
      else if(nameIs(part, "target"))
      {
        for (auto seg : children(part)) {
          ret += processValue(seg);
          ret += APPENDSURFACE;
        }
      }
      else if(nameIs(part, "reference"))
      {
        for (auto seg : children(part)) {
          ret += processValue(seg);
          ret += APPENDSURFACEREF;
        }
      }
      else if(nameIs(part, "contents"))
      {
        for (auto seg : children(part)) {
          ret += processValue(seg);
          ret += APPENDCHILD;
        }
      }
    }
    if(!inOutput && currentOutputRule != -1)
    {
      ret += (UChar)currentOutputRule;
      ret += (UChar)0;
      ret += SETRULE;
    }
  }
  else if(nameIs(node, "lu-count"))
  {
    ret += LUCOUNT;
  }
  else
  {
    die(node, "Unrecognized expression '%S'", name(node).c_str());
  }
  return ret;
}

UString
TRXCompiler::processCond(xmlNode* node)
{
  UString ret;
  if(nameIs(node, "and"))
  {
    for (auto op : children(node)) {
      unsigned int len = ret.size();
      ret += processCond(op);
      if(len > 0 && ret.size() > len)
      {
        ret += AND;
      }
    }
  }
  else if(nameIs(node, "or"))
  {
    for (auto op : children(node)) {
      unsigned int len = ret.size();
      ret += processCond(op);
      if(len > 0 && ret.size() > len)
      {
        ret += OR;
      }
    }
  }
  else if(nameIs(node, "not"))
  {
    for (auto op : children(node)) {
      if(ret.size() > 0)
      {
        die(node, "<not> cannot have multiple children");
      }
      else
      {
        ret = processCond(op);
        ret += NOT;
      }
    }
  }
  else if(nameIs(node, "equal"))
  {
    int i = 0;
    for (auto op : children(node)) {
      ret += processValue(op);
      i++;
    }
    if(i != 2)
    {
      die(node, "<equal> must have exactly two children");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += EQUALCL;
    }
    else
    {
      ret += EQUAL;
    }
  }
  else if(nameIs(node, "begins-with"))
  {
    int i = 0;
    for (auto op : children(node)) {
      ret += processValue(op);
      i++;
    }
    if(i != 2)
    {
      die(node, "<begins-with> must have exactly two children");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += ISPREFIXCL;
    }
    else
    {
      ret += ISPREFIX;
    }
  }
  else if(nameIs(node, "begins-with-list"))
  {
    bool list = false;
    for (auto op : children(node)) {
      if(ret.size() == 0)
      {
        ret += processValue(op);
      }
      else if(list)
      {
        die(node, "<begins-with-list> cannot have more than two children.");
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, "Expected <list>, found <%S> instead.", to_ustring((const char*)op->name).c_str());
      }
      else
      {
        UString name = requireAttr(op, "n");
        if(lists.find(name) == lists.end())
        {
          die(op, "Unknown list '%S'.", name.c_str());
        }
        ret += STRING;
        ret += (UChar)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, "<begins-with-list> must have two children.");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += HASPREFIXCL;
    }
    else
    {
      ret += HASPREFIX;
    }
  }
  else if(nameIs(node, "ends-with"))
  {
    int i = 0;
    for (auto op : children(node)) {
      ret += processValue(op);
      i++;
    }
    if(i != 2)
    {
      die(node, "<ends-with> must have exactly two children");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += ISSUFFIXCL;
    }
    else
    {
      ret += ISSUFFIX;
    }
  }
  else if(nameIs(node, "ends-with-list"))
  {
    bool list = false;
    for (auto op : children(node)) {
      if(ret.size() == 0)
      {
        ret += processValue(op);
      }
      else if(list)
      {
        die(node, "<ends-with-list> cannot have more than two children.");
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, "Expected <list>, found <%S> instead.", name(op).c_str());
      }
      else
      {
        UString name = requireAttr(op, "n");
        if(lists.find(name) == lists.end())
        {
          die(op, "Unknown list '%S'.", name.c_str());
        }
        ret += STRING;
        ret += (UChar)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, "<ends-with-list> must have two children.");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += HASSUFFIXCL;
    }
    else
    {
      ret += HASSUFFIX;
    }
  }
  else if(nameIs(node, "contains-substring"))
  {
    int i = 0;
    for (auto op : children(node)) {
      ret += processValue(op);
      i++;
    }
    if(i != 2)
    {
      die(node, "<contains-substring> must have exactly two children");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += ISSUBSTRINGCL;
    }
    else
    {
      ret += ISSUBSTRING;
    }
  }
  else if(nameIs(node, "in"))
  {
    bool list = false;
    for (auto op : children(node)) {
      if(ret.size() == 0)
      {
        ret += processValue(op);
      }
      else if(list)
      {
        die(node, "<in> cannot have more than two children.");
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, "Expected <list>, found <%S> instead.",
            name(op).c_str());
      }
      else
      {
        UString name = requireAttr(op, "n");
        if(lists.find(name) == lists.end())
        {
          die(op, "Unknown list '%S'.", name.c_str());
        }
        ret += STRING;
        ret += (UChar)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, "<in> must have two children.");
    }
    if(getattr(node, "caseless") == "yes"_u)
    {
      ret += INCL;
    }
    else
    {
      ret += IN;
    }
  }
  else
  {
    die(node, "Unrecognized condition '%S'", name(node).c_str());
  }
  return ret;
}

UString
TRXCompiler::processChoose(xmlNode* node)
{
  vector<pair<UString, UString>> clauses;
  int when = 0;
  int otherwise = 0;
  for (auto cl : children(node)) {
    if(nameIs(cl, "when"))
    {
      if(otherwise > 0)
      {
        warn(cl, "Clauses after <otherwise> will not be executed.");
        continue;
      }
      when++;
      UString test, block;
      for (auto n : children(cl)) {
        if(nameIs(n, "test"))
        {
          if(test.size() != 0)
          {
            die(n, "Cannot have multiple <test>s in a <when> clause.");
          }
          for (auto t : children(n)) {
            if(test.size() == 0)
            {
              test = processCond(t);
            }
            else
            {
              die(t, "<test> must have exactly one child.");
            }
          }
          if(test.size() == 0)
          {
            die(n, "<test> cannot be empty.");
          }
        }
        else
        {
          if(test.size() == 0)
          {
            die(n, "<when> clause must begin with <test>.");
          }
          block += processStatement(n);
        }
      }
      clauses.push_back(make_pair(test, block));
    }
    else if(nameIs(cl, "otherwise"))
    {
      otherwise++;
      if(otherwise > 1)
      {
        warn(cl, "Multiple <otherwise> clauses will not be executed.");
        continue;
      }
      UString block;
      for (auto state : children(cl)) {
        block += processStatement(state);
      }
      if(block.size() > 0)
      {
        clauses.push_back(make_pair(""_u, block));
      }
      else
      {
        warn(cl, "Empty <otherwise> clause.");
      }
    }
    else
    {
      warn(cl, "Ignoring unexpected clause in <choose>.");
    }
  }
  UString ret;
  for(vector<pair<UString, UString>>::reverse_iterator it = clauses.rbegin(),
            limit = clauses.rend(); it != limit; it++)
  {
    UString act = it->second;
    if(ret.size() > 0)
    {
      act += JUMP;
      act += (UChar)ret.size();
    }
    UString test = it->first;
    if(test.size() > 0)
    {
      test += JUMPONFALSE;
      test += (UChar)act.size();
    }
    ret = test + act + ret;
  }
  return ret;
}

void
TRXCompiler::loadLex(const string& fname)
{
  PB.loadLexFile(fname);
}

void
TRXCompiler::write(const char* binfile)
{
  FILE* bin = fopen(binfile, "wb");
  if(bin == NULL)
  {
    cerr << "Error: Cannot open " << binfile << " for writing." << endl;
    exit(EXIT_FAILURE);
  }
  vector<pair<int, UString>> inRules;
  for(unsigned int i = 0; i < inputRules.size(); i++)
  {
    inRules.push_back(make_pair((inputRuleSizes[i]*2 - 1), inputRules[i]));
  }
  PB.write(bin, (longestPattern*2) - 1, inRules, outputRules);
  fclose(bin);
}

void
TRXCompiler::printStats()
{
  cout << "Rules: " << inputRules.size() << endl;
  cout << "Macros: " << macros.size() << endl;
}
