#include <rtx_config.h>
#include <lttoolbox/xml_parse_util.h>
#include <lttoolbox/compression.h>
#include <bytecode.h>
#include <trx_compiler.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <lttoolbox/string_utils.h>
#include <lttoolbox/xml_walk_util.h>
#include <i18n.h>

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
TRXCompiler::die(xmlNode* node, icu::UnicodeString message)
{
  cerr << I18n(APRC_I18N_DATA, "aprc").format("in_file_on_line", {"file", "line"},
    {(char*)curDoc->URL, node->line}) << endl;
  cerr << message << endl;
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
TRXCompiler::warn(xmlNode* node, icu::UnicodeString message)
{
  cerr << I18n(APRC_I18N_DATA, "aprc").format("in_file_on_line",
    {"file", "line"}, {(char*)curDoc->URL, node->line}) << endl;
  cerr << message << endl;
}

void
TRXCompiler::compile(string file)
{
  curDoc = xmlReadFile(file.c_str(), NULL, 0);
  if(curDoc == NULL)
  {
    I18n(APRC_I18N_DATA, "aprc").error("APRC1002", {"file"}, {file.c_str()}, true);
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
  die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1103", {"attr"}, {attr}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1104"));
    }
  }
  for(unsigned int i = 0; i < v.size(); i++)
  {
    if(!isdigit(v[i]))
    {
      if(isBlank)
      {
        warn(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1138"));
        return 0;
      }
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1105"));
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
      warn(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1139"));
      return 0;
    }
    die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1106", {"pos"}, {ret}));
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
      warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"section-def-cats"}));
      continue;
    }
    UString pat_name = requireAttr(cat, "n");
    vector<PatternElement*> pat;
    for (auto item : children(cat)) {
      if (!nameIs(item, "cat-item")) {
        warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1141", {"node"},
          {icu::UnicodeString(name(item).data())}));
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
      warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1142", {"pattern"},
        {icu::UnicodeString(pat_name.data())}));
    }
    patterns[pat_name] = pat;
  }
}

void
TRXCompiler::processAttrs(xmlNode* node)
{
  for (auto cat : children(node)) {
    if (!nameIs(cat, "def-attr")) {
      warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"section-def-attrs"}));
      continue;
    }
    UString name = getattr(cat, "n");
    set<UString> ats;
    for (auto item : children(cat)) {
      if (!nameIs(item, "attr-item")) {
        warn(item, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"def-attr"}));
        continue;
      }
      ats.insert(getattr(item, "tags"));
    }
    if(PB.isAttrDefined(name))
    {
      warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1143", {"attr"},
        {icu::UnicodeString(name.data())}));
    }
    PB.addAttr(name, ats);
  }
}

void
TRXCompiler::processVars(xmlNode* node)
{
  for (auto var : children(node)) {
    if (!nameIs(var, "def-var")) {
      warn(var, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"section-def-vars"}));
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
      warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"section-def-lists"}));
      continue;
    }
    UString name = getattr(cat, "n");
    set<UString> ats;
    for (auto item : children(cat)) {
      if (!nameIs(item, "list-item")) {
        warn(item, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"def-list"}));
        continue;
      }
      ats.insert(getattr(item, "v"));
    }
    if(lists.find(name) != lists.end())
    {
      warn(cat, I18n(APRC_I18N_DATA, "aprc").format("APRC1144", {"list"},
        {icu::UnicodeString(name.data())}));
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
      warn(mac, I18n(APRC_I18N_DATA, "aprc").format("APRC1140", {"node"}, {"section-def-macros"}));
      continue;
    }
    UString name = requireAttr(mac, "n");
    int npar = StringUtils::stoi(requireAttr(mac, "npar"));
    if(macros.find(name) != macros.end())
    {
      warn(mac, I18n(APRC_I18N_DATA, "aprc").format("APRC1145", {"macro"},
        {icu::UnicodeString(name.data())}));
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
      warn(rule, I18n(APRC_I18N_DATA, "aprc").format("APRC1146", {"element", "node"},
        {"<rule>", "<section-rules>"}));
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
          die(rule, I18n(APRC_I18N_DATA, "aprc").format("APRC1107"));
        }
        pat = true;
        vector<vector<PatternElement*>> pls;
        for (auto pi : children(part)) {
          if (!nameIs(pi, "pattern-item")) {
            warn(pi, I18n(APRC_I18N_DATA, "aprc").format("APRC1146", {"element", "node"},
              {"<pattern-item>", "<section-rules>"}));
            continue;
          }
          curPatternSize++;
          UString name = requireAttr(pi, "n");
          if(patterns.find(name) == patterns.end())
          {
            die(pi, I18n(APRC_I18N_DATA, "aprc").format("APRC1108", {"pattern"},
              {icu::UnicodeString(name.data())}));
          }
          else
          {
            pls.push_back(patterns[name]);
          }
        }
        if(curPatternSize == 0)
        {
          die(rule, I18n(APRC_I18N_DATA, "aprc").format("APRC1109"));
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
          die(rule, I18n(APRC_I18N_DATA, "aprc").format("APRC1110"));
        }
        action = part;
      }
      else if(nameIs(part, "output-action"))
      {
        if(outputAction.size() > 0)
        {
          die(part, I18n(APRC_I18N_DATA, "aprc").format("APRC1111"));
        }
        inOutput = true;
        for (auto state : children(part)) {
          outputAction += processStatement(state);
        }
      }
      else
      {
        warn(part, I18n(APRC_I18N_DATA, "aprc").format("APRC1147", {"node"},
          {icu::UnicodeString(name(part).data())}));
      }
    }
    if(!pat)
    {
      die(rule, I18n(APRC_I18N_DATA, "aprc").format("APRC1112"));
    }
    if(action == NULL)
    {
      die(rule, I18n(APRC_I18N_DATA, "aprc").format("APRC1113"));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1114", {"node"},
          {icu::UnicodeString(name(node).data())}));
      }
    }
    if(val.size() == 0)
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"},
        {icu::UnicodeString(name(node).data())}));
    }
    if(nameIs(var, "var"))
    {
      UString vname = requireAttr(var, "n");
      if(vars.find(vname) == vars.end())
      {
        die(var, I18n(APRC_I18N_DATA, "aprc").format("APRC1116", {"var"},
          {icu::UnicodeString(vname.data())}));
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
        warn(var, I18n(APRC_I18N_DATA, "aprc").format("APRC1148", {"side"},
          {icu::UnicodeString(side.data())}));
      }
      UString part = requireAttr(var, "part");
      if(!PB.isAttrDefined(part))
      {
        die(var, I18n(APRC_I18N_DATA, "aprc").format("APRC1117", {"attr"},
          {icu::UnicodeString(part.data())}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1118", {"node"}, {icu::UnicodeString(name(var).data())}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1119",
        {"macro"}, {icu::UnicodeString(name.data())}));
    }
    vector<int> temp;
    for (auto param : children(node)) {
      if (nameIs(param, "with-param")) {
        temp.push_back(getPos(param));
      } else {
        warn(param, I18n(APRC_I18N_DATA, "aprc").format("APRC1146", {"element", "node"},
            {"<with-param>", "<call-macro>"}));
      }
    }
    unsigned int shouldbe = macros[name].first;
    if(shouldbe < temp.size())
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1120", {"macro", "expected", "given"},
        {icu::UnicodeString(name.data()),
         to_string(shouldbe).c_str(), to_string(temp.size()).c_str()}));
    }
    if(shouldbe > temp.size())
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1121", {"macro", "expected", "given"},
        {icu::UnicodeString(name.data()),
         to_string(shouldbe).c_str(), to_string(temp.size()).c_str()}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1122", {"var"},
        {icu::UnicodeString(name.data())}));
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
    die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1123", {"statement"},
      {icu::UnicodeString(name(node).data())}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1117", {"attr"},
        {icu::UnicodeString(part.data())}));
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
      warn(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1149", {"side"},
        {icu::UnicodeString(side.data())}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1122", {"var"},
        {icu::UnicodeString(v.data())}));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1124"));
      }
    }
    if(ret.size() == 0)
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1125"));
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
      warn(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1150", {"side"},
        {icu::UnicodeString(side.data())}));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1126"));
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
    die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1127", {"exp"},
      {icu::UnicodeString(name(node).data())}));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1128"));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<equal>"}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<begins-with>"}));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1130", {"node"}, {"<begins-with-list>"}));
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, I18n(APRC_I18N_DATA, "aprc").format("APRC1131", {"node"},
          {icu::UnicodeString(to_ustring((const char*)op->name).data())}));
      }
      else
      {
        UString name = requireAttr(op, "n");
        if(lists.find(name) == lists.end())
        {
          die(op, I18n(APRC_I18N_DATA, "aprc").format("APRC1132", {"list"},
            {icu::UnicodeString(name.data())}));
        }
        ret += STRING;
        ret += (UChar)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<begins-with-list>"}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<ends-with>"}));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1130", {"node"}, {"<ends-with-list>"}));
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, I18n(APRC_I18N_DATA, "aprc").format("APRC1131", {"node"},
          {icu::UnicodeString(name(op).data())}));
      }
      else
      {
        UString name = requireAttr(op, "n");
        if(lists.find(name) == lists.end())
        {
          die(op, I18n(APRC_I18N_DATA, "aprc").format("APRC1132", {"list"},
            {icu::UnicodeString(name.data())}));
        }
        ret += STRING;
        ret += (UChar)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<ends-with-list>"}));
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
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<contains-substring>"}));
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
        die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1130", {"node"}, {"<in>"}));
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, I18n(APRC_I18N_DATA, "aprc").format("APRC1131", {"node"},
          {icu::UnicodeString(name(op).data())}));
      }
      else
      {
        UString name = requireAttr(op, "n");
        if(lists.find(name) == lists.end())
        {
          die(op, I18n(APRC_I18N_DATA, "aprc").format("APRC1132", {"list"},
            {icu::UnicodeString(name.data())}));
        }
        ret += STRING;
        ret += (UChar)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1115", {"node"}, {"<in>"}));
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
    die(node, I18n(APRC_I18N_DATA, "aprc").format("APRC1133", {"condition"},
      {icu::UnicodeString(name(node).data())}));
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
        warn(cl, I18n(APRC_I18N_DATA, "aprc").format("APRC1151"));
        continue;
      }
      when++;
      UString test, block;
      for (auto n : children(cl)) {
        if(nameIs(n, "test"))
        {
          if(test.size() != 0)
          {
            die(n, I18n(APRC_I18N_DATA, "aprc").format("APRC1134"));
          }
          for (auto t : children(n)) {
            if(test.size() == 0)
            {
              test = processCond(t);
            }
            else
            {
              die(t, I18n(APRC_I18N_DATA, "aprc").format("APRC1135"));
            }
          }
          if(test.size() == 0)
          {
            die(n, I18n(APRC_I18N_DATA, "aprc").format("APRC1136"));
          }
        }
        else
        {
          if(test.size() == 0)
          {
            die(n, I18n(APRC_I18N_DATA, "aprc").format("APRC1137"));
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
        warn(cl, I18n(APRC_I18N_DATA, "aprc").format("APRC1152"));
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
        warn(cl, I18n(APRC_I18N_DATA, "aprc").format("APRC1153"));
      }
    }
    else
    {
      warn(cl, I18n(APRC_I18N_DATA, "aprc").format("APRC1154"));
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
    I18n(APRC_I18N_DATA, "aprc").error("APRC1002", {"file"}, {binfile}, true);
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
