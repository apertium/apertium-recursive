#include <rtx_config.h>
#include <apertium/trx_reader.h>
#include <lttoolbox/xml_parse_util.h>
#include <lttoolbox/compression.h>
#include <bytecode.h>
#include <trx_compiler.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <apertium/string_utils.h>

using namespace Apertium;
using namespace std;

TRXCompiler::TRXCompiler()
{
  longestPattern = 0;
}

TRXCompiler::~TRXCompiler()
{
  // TODO
}

void
TRXCompiler::die(xmlNode* node, wstring msg)
{
  wcerr << "Error in " << UtfConverter::fromUtf8((char*) curDoc->URL);
  wcerr << " on line " << node->line << ": " << msg << endl;
  exit(EXIT_FAILURE);
}

void
TRXCompiler::warn(xmlNode* node, wstring msg)
{
  wcerr << "Warning in " << UtfConverter::fromUtf8((char*) curDoc->URL);
  wcerr << " on line " << node->line << ": " << msg << endl;
}

void
TRXCompiler::compile(string file)
{
  curDoc = xmlReadFile(file.c_str(), NULL, 0);
  if(curDoc == NULL)
  {
    wcerr << "Error: Could not parse file '" << file << "'." << endl;
    exit(EXIT_FAILURE);
  }
  processFile(xmlDocGetRootElement(curDoc));
}

void
TRXCompiler::processFile(xmlNode* node)
{
  for(xmlNode* i = node->children; i != NULL; i = i->next)
  {
    if(i->type == XML_ELEMENT_NODE)
    {
      if(!xmlStrcmp(i->name, (const xmlChar*) "section-def-cats"))
      {
        processCats(i);
      }
      else if(!xmlStrcmp(i->name, (const xmlChar*) "section-def-attrs"))
      {
        processAttrs(i);
      }
      else if(!xmlStrcmp(i->name, (const xmlChar*) "section-def-vars"))
      {
        processVars(i);
      }
      else if(!xmlStrcmp(i->name, (const xmlChar*) "section-def-lists"))
      {
        processLists(i);
      }
      else if(!xmlStrcmp(i->name, (const xmlChar*) "section-def-macros"))
      {
        gatherMacros(i);
      }
      else if(!xmlStrcmp(i->name, (const xmlChar*) "section-rules"))
      {
        processRules(i);
      }
    }
  }
}

xmlChar*
TRXCompiler::requireAttr(xmlNode* node, const xmlChar* attr)
{
  for(xmlAttr* a = node->properties; a != NULL; a = a->next)
  {
    if(!xmlStrcmp(a->name, attr))
    {
      return a->children->content;
    }
  }
  die(node, L"Expected attribute '" + UtfConverter::fromUtf8((const char*) attr) + L"'");
  return NULL;
  // since die() ends the process, NULL will never be returned,
  // but this keeps the compiler from complaining about the lack of a return statement
}

xmlChar*
TRXCompiler::getAttr(xmlNode* node, const xmlChar* attr)
{
  for(xmlAttr* a = node->properties; a != NULL; a = a->next)
  {
    if(!xmlStrcmp(a->name, attr))
    {
      return a->children->content;
    }
  }
  return NULL;
}

inline wstring
TRXCompiler::toWstring(const xmlChar* s)
{
  return (s == NULL) ? L"" : UtfConverter::fromUtf8((char*) s);
}

int
TRXCompiler::getPos(xmlNode* node, bool isBlank = false)
{
  wstring v;
  if(!xmlStrcmp(node->name, (const xmlChar*) "b"))
  {
    v = toWstring(getAttr(node, (const xmlChar*) "pos"));
    if(v == L"")
    {
      return 0;
    }
  }
  else
  {
    v = toWstring(requireAttr(node, (const xmlChar*) "pos"));
  }
  if(v.size() == 0)
  {
    if(isBlank)
    {
      return 0;
    }
    else
    {
      die(node, L"Cannot interpret empty pos attribute.");
    }
  }
  for(unsigned int i = 0; i < v.size(); i++)
  {
    if(!isdigit(v[i]))
    {
      if(isBlank)
      {
        warn(node, L"Disregarding non-integer position.");
        return 0;
      }
      die(node, L"Position must be an integer.");
    }
  }
  int ret = stoi(v);
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
      warn(node, L"Disregarding out-of-bounds position.");
      return 0;
    }
    die(node, L"Position " + to_wstring(ret) + L" is out of bounds.");
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
  for(xmlNode* cat = node->children; cat != NULL; cat = cat->next)
  {
    if(cat->type == XML_ELEMENT_NODE)
    {
      if(xmlStrcmp(cat->name, (const xmlChar*) "def-cat"))
      {
        warn(cat, L"Unexpected tag in section-def-cats - ignoring");
        continue;
      }
      wstring name = toWstring(requireAttr(cat, (const xmlChar*) "n"));
      vector<PatternElement*> pat;
      for(xmlNode* item = cat->children; item != NULL; item = item->next)
      {
        if(item->type != XML_ELEMENT_NODE) continue;
        if(xmlStrcmp(item->name, (const xmlChar*) "cat-item"))
        {
          warn(cat, L"Unexpected tag <" + toWstring(item->name) + L"> in def-cat - ignoring");
          continue;
        }
        PatternElement* cur = new PatternElement;
        cur->lemma = toWstring(getAttr(item, (const xmlChar*) "lemma"));
        wstring tags = toWstring(requireAttr(item, (const xmlChar*) "tags"));
        if(tags == L"") tags = L"UNKNOWN:INTERNAL";
        cur->tags = StringUtils::split_wstring(tags, L".");
        pat.push_back(cur);
      }
      if(patterns.find(name) != patterns.end())
      {
        warn(cat, L"Redefinition of pattern '" + name + L"', using later value");
      }
      patterns[name] = pat;
    }
  }
}

void
TRXCompiler::processAttrs(xmlNode* node)
{
  for(xmlNode* cat = node->children; cat != NULL; cat = cat->next)
  {
    if(cat->type != XML_ELEMENT_NODE)
    {
      continue;
    }
    if(xmlStrcmp(cat->name, (const xmlChar*) "def-attr"))
    {
      warn(cat, L"Unexpected tag in section-def-attrs - ignoring");
      continue;
    }
    wstring name = toWstring(getAttr(cat, (const xmlChar*) "n"));
    set<wstring, Ltstr> ats;
    for(xmlNode* item = cat->children; item != NULL; item = item->next)
    {
      if(item->type != XML_ELEMENT_NODE) continue;
      if(xmlStrcmp(item->name, (const xmlChar*) "attr-item"))
      {
        warn(item, L"Unexpected tag in def-attr - ignoring");
        continue;
      }
      ats.insert(toWstring(getAttr(item, (const xmlChar*) "tags")));
    }
    if(PB.isAttrDefined(name))
    {
      warn(cat, L"Redefinition of attribute '" + name + L"' - using later definition");
    }
    PB.addAttr(name, ats);
  }
}

void
TRXCompiler::processVars(xmlNode* node)
{
  for(xmlNode* var = node->children; var != NULL; var = var->next)
  {
    if(var->type != XML_ELEMENT_NODE) continue;
    if(xmlStrcmp(var->name, (const xmlChar*) "def-var"))
    {
      warn(var, L"Unexpected tag in section-def-vars - ignoring");
      continue;
    }
    wstring name = toWstring(requireAttr(var, (const xmlChar*) "n"));
    vars[name] = toWstring(getAttr(var, (const xmlChar*) "v"));
    PB.addVar(name, vars[name]);
  }
}

void
TRXCompiler::processLists(xmlNode* node)
{
  for(xmlNode* cat = node->children; cat != NULL; cat = cat->next)
  {
    if(cat->type != XML_ELEMENT_NODE)
    {
      continue;
    }
    if(xmlStrcmp(cat->name, (const xmlChar*) "def-list"))
    {
      warn(cat, L"Unexpected tag in section-def-lists - ignoring");
      continue;
    }
    wstring name = toWstring(getAttr(cat, (const xmlChar*) "n"));
    set<wstring, Ltstr> ats;
    for(xmlNode* item = cat->children; item != NULL; item = item->next)
    {
      if(item->type != XML_ELEMENT_NODE) continue;
      if(xmlStrcmp(item->name, (const xmlChar*) "list-item"))
      {
        warn(item, L"Unexpected tag in def-list - ignoring");
        continue;
      }
      ats.insert(toWstring(getAttr(item, (const xmlChar*) "v")));
    }
    if(lists.find(name) != lists.end())
    {
      warn(cat, L"Redefinition of list '" + name + L"' - using later definition");
    }
    lists[name] = ats;
    PB.addList(name, ats);
  }
}

void
TRXCompiler::gatherMacros(xmlNode* node)
{
  for(xmlNode* mac = node->children; mac != NULL; mac = mac->next)
  {
    if(mac->type != XML_ELEMENT_NODE) continue;
    if(xmlStrcmp(mac->name, (const xmlChar*) "def-macro"))
    {
      warn(mac, L"Unexpected tag in section-def-macros - ignoring");
      continue;
    }
    wstring name = toWstring(requireAttr(mac, (const xmlChar*) "n"));
    int npar = atoi((const char*) requireAttr(mac, (const xmlChar*) "npar"));
    if(macros.find(name) != macros.end())
    {
      warn(mac, L"Redefinition of macro '" + name + L"' - using later definition");
    }
    macros[name] = make_pair(npar, mac);
  }
}

void
TRXCompiler::processRules(xmlNode* node)
{
  for(xmlNode* rule = node->children; rule != NULL; rule = rule->next)
  {
    if(rule->type != XML_ELEMENT_NODE) continue;
    if(xmlStrcmp(rule->name, (const xmlChar*) "rule"))
    {
      warn(rule, L"Ignoring non-<rule> element in <section-rules>.");
      continue;
    }
    if(!xmlStrcmp(getAttr(rule, (const xmlChar*) "i"), (const xmlChar*) "yes"))
    {
      continue;
    }
    curPatternSize = 0;
    localVars.clear();

    wstring id = toWstring(getAttr(rule, (const xmlChar*) "id"));
    wstring weight = toWstring(getAttr(rule, (const xmlChar*) "weight"));
    wstring firstChunk = toWstring(getAttr(rule, (const xmlChar*) "firstChunk"));
    if(firstChunk == L"") firstChunk = L"*";

    xmlNode* action = NULL;
    wstring outputAction;
    bool pat = false;
    wstring assertClause = L"";
    for(xmlNode* part = rule->children; part != NULL; part = part->next)
    {
      if(part->type != XML_ELEMENT_NODE) continue;
      if(!xmlStrcmp(part->name, (const xmlChar*) "local"))
      {
        for(xmlNode* var = rule->children; var != NULL; var = var->next)
        {
          if(var->type == XML_ELEMENT_NODE &&
             !xmlStrcmp(var->name, (const xmlChar*) "var"))
          {
            localVars.insert(toWstring(requireAttr(var, (const xmlChar*) "n")));
          }
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "pattern"))
      {
        if(pat)
        {
          die(rule, L"Rule cannot have multiple <pattern>s.");
        }
        pat = true;
        vector<vector<PatternElement*>> pls;
        for(xmlNode* pi = part->children; pi != NULL; pi = pi->next)
        {
          if(pi->type != XML_ELEMENT_NODE) continue;
          if(xmlStrcmp(pi->name, (const xmlChar*) "pattern-item"))
          {
            warn(pi, L"Ignoring non-<pattern-item> in <pattern>.");
            continue;
          }
          curPatternSize++;
          wstring name = toWstring(requireAttr(pi, (const xmlChar*) "n"));
          if(patterns.find(name) == patterns.end())
          {
            die(pi, L"Unknown pattern '" + name + L"'.");
          }
          else
          {
            pls.push_back(patterns[name]);
          }
        }
        if(curPatternSize == 0)
        {
          die(rule, L"Rule cannot have empty pattern.");
        }
        if(curPatternSize > longestPattern)
        {
          longestPattern = curPatternSize;
        }
        if(excludedRules.find(id) == excludedRules.end())
        {
          PB.addRule(inputRules.size() + 1, (weight.size() > 0 ? stod(weight) : 0.0), pls, StringUtils::split_wstring(firstChunk, L" "), id);
        }
        inputRuleSizes.push_back(pls.size());
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "assert"))
      {
        bool firstAssert = (assertClause.size() == 0);
        for(xmlNode* clause = part->children; clause != NULL; clause = clause->next)
        {
          if(clause->type != XML_ELEMENT_NODE) continue;
          assertClause += processCond(clause);
          if(!firstAssert)
          {
            assertClause += AND;
          }
          firstAssert = false;
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "action"))
      {
        if(action != NULL)
        {
          die(rule, L"Rule cannot have multiple <action>s.");
        }
        action = part;
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "output-action"))
      {
        if(outputAction.size() > 0)
        {
          die(part, L"Rule cannot have multiple <output-action>s.");
        }
        inOutput = true;
        for(xmlNode* state = part->children; state != NULL; state = state->next)
        {
          if(state->type == XML_ELEMENT_NODE) outputAction += processStatement(state);
        }
      }
      else
      {
        warn(part, L"Unknown element <" + toWstring(part->name) + L"> in <rule>, ignoring.");
      }
    }
    if(!pat)
    {
      die(rule, L"Rule must have <pattern>.");
    }
    if(action == NULL)
    {
      die(rule, L"Rule must have <action>.");
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
      wstring actionStr;
      if(assertClause.size() > 0)
      {
        actionStr = assertClause;
        actionStr += JUMPONTRUE;
        actionStr += (wchar_t)1;
        actionStr += REJECTRULE;
      }
      for(xmlNode* state = action->children; state != NULL; state = state->next)
      {
        if(state->type != XML_ELEMENT_NODE) continue;
        actionStr += processStatement(state);
      }
      inputRules.push_back(actionStr);
    }
  }
}

wstring
TRXCompiler::processStatement(xmlNode* node)
{
  if(!xmlStrcmp(getAttr(node, (const xmlChar*) "i"), (const xmlChar*) "yes"))
  {
    return L"";
  }
  wstring ret;
  if(!xmlStrcmp(node->name, (const xmlChar*) "let") ||
     !xmlStrcmp(node->name, (const xmlChar*) "modify-case"))
  {
    wstring name = toWstring(node->name);
    xmlNode* var = NULL;
    wstring val;
    bool val_is_clip = false;
    for(xmlNode* n = node->children; n != NULL; n = n->next)
    {
      if(n->type != XML_ELEMENT_NODE) continue;
      if(var == NULL)
      {
        var = n;
      }
      else if(val.size() == 0)
      {
        val_is_clip = (!xmlStrcmp(n->name, (const xmlChar*) "clip"));
        val = processValue(n);
      }
      else
      {
        die(node, L"<" + name + L"> cannot have more than two children.");
      }
    }
    if(val.size() == 0)
    {
      die(node, L"<" + name + L"> must have two children.");
    }
    if(!xmlStrcmp(var->name, (const xmlChar*) "var"))
    {
      wstring vname = toWstring(requireAttr(var, (const xmlChar*) "n"));
      if(vars.find(vname) == vars.end())
      {
        die(var, L"Undefined variable '" + vname + L"'.");
      }
      if(name == L"modify-case")
      {
        ret += STRING;
        ret += (wchar_t)vname.size();
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
      ret += (wchar_t)vname.size();
      ret += vname;
      ret += SETVAR;
    }
    else if(!xmlStrcmp(var->name, (const xmlChar*) "clip"))
    {
      wstring side = toWstring(getAttr(var, (const xmlChar*) "side"));
      if(!(side == L"" || side == L"tl"))
      {
        warn(var, L"Cannot set side '" + side + L"', setting 'tl' instead.");
      }
      wstring part = toWstring(requireAttr(var, (const xmlChar*) "part"));
      if(!PB.isAttrDefined(part))
      {
        die(var, L"Unknown attribute '" + part + L"'");
      }
      wstring set_str;
      set_str += PB.BCstring(part);
      set_str += INT;
      set_str += (wchar_t)getPos(var);
      set_str += SETCLIP;
      if(name == L"modify-case")
      {
        ret += INT;
        ret += (wchar_t)getPos(var);
        ret += PUSHINPUT;
        ret += STRING;
        ret += (wchar_t)part.size();
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
          wstring cond;
          cond += DUP;
          cond += PB.BCstring(L"");
          cond += EQUAL;
          ret += PB.BCifthenelse(cond, wstring(1, DROP), set_str);
        }
        else
        {
          ret += set_str;
        }
      }
    }
    else
    {
      die(node, L"Cannot set value of <" + toWstring(var->name) + L">.");
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "out"))
  {
    for(xmlNode* o = node->children; o != NULL; o = o->next)
    {
      if(o->type == XML_ELEMENT_NODE)
      {
        ret += processValue(o);
        ret += OUTPUT;
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "choose"))
  {
    ret = processChoose(node);
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "call-macro"))
  {
    // TODO: DTD implies number of arguments can be variable
    wstring name = toWstring(requireAttr(node, (const xmlChar*) "n"));
    if(macros.find(name) == macros.end())
    {
      die(node, L"Unknown macro '" + name + L"'.");
    }
    vector<int> temp;
    for(xmlNode* param = node->children; param != NULL; param = param->next)
    {
      if(param->type != XML_ELEMENT_NODE) continue;
      if(xmlStrcmp(param->name, (const xmlChar*) "with-param"))
      {
        warn(param, L"Ignoring non-<with-param> in <call-macro>");
      }
      else
      {
        temp.push_back(getPos(param));
      }
    }
    unsigned int shouldbe = macros[name].first;
    if(shouldbe < temp.size())
    {
      die(node, L"Too many parameters, macro '" + name + L"' expects " + to_wstring(shouldbe) + L", got " + to_wstring(temp.size()) + L".");
    }
    if(shouldbe > temp.size())
    {
      die(node, L"Not enough parameters, macro '" + name + L"' expects " + to_wstring(shouldbe) + L", got " + to_wstring(temp.size()) + L".");
    }
    macroPosShift.push_back(temp);
    xmlNode* mac = macros[name].second;
    for(xmlNode* state = mac->children; state != NULL; state = state->next)
    {
      if(state->type != XML_ELEMENT_NODE) continue;
      ret += processStatement(state);
    }
    macroPosShift.pop_back();
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "append"))
  {
    // TODO: DTD says this can append to a clip
    wstring name = toWstring(requireAttr(node, (const xmlChar*) "n"));
    if(vars.find(name) == vars.end() && localVars.find(name) == localVars.end())
    {
      die(node, L"Unknown variable '" + name + L"'.");
    }
    ret += STRING;
    ret += (wchar_t)name.size();
    ret += name;
    ret += FETCHVAR;
    for(xmlNode* part = node->children; part != NULL; part = part->next)
    {
      if(part->type == XML_ELEMENT_NODE)
      {
        ret += processValue(part);
        ret += CONCAT;
      }
    }
    ret += STRING;
    ret += (wchar_t)name.size();
    ret += name;
    ret += SETVAR;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "reject-current-rule"))
  {
    ret += REJECTRULE;
  }
  else
  {
    die(node, L"Unrecognized statement '" + toWstring(node->name) + L"'");
  }
  return ret;
}

wstring
TRXCompiler::processValue(xmlNode* node)
{
  wstring ret;
  if(!xmlStrcmp(node->name, (const xmlChar*) "b"))
  {
    ret += INT;
    ret += (wchar_t)getPos(node);
    ret += BLANK;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "clip"))
  {
    ret += INT;
    ret += (wchar_t)getPos(node);
    ret += PUSHINPUT;
    ret += STRING;
    wstring part = toWstring(requireAttr(node, (const xmlChar*) "part"));
    if(!PB.isAttrDefined(part))
    {
      die(node, L"Unknown attribute '" + part + L"'");
    }
    ret += (wchar_t)part.size();
    ret += part;
    wstring side = toWstring(getAttr(node, (const xmlChar*) "side"));
    if(side == L"sl")
    {
      ret += SOURCECLIP;
    }
    else if(side == L"tl" || side == L"")
    {
      ret += TARGETCLIP;
    }
    else if(side == L"ref")
    {
      ret += REFERENCECLIP;
    }
    else
    {
      warn(node, L"Unknown clip side '" + side + L"', defaulting to 'tl'.");
      ret += TARGETCLIP;
    }
    wstring link = toWstring(getAttr(node, (const xmlChar*) "link-to"));
    if(link.size() > 0)
    {
      ret += DUP;
      ret += STRING;
      ret += (wchar_t)0;
      ret += EQUAL;
      ret += JUMPONTRUE;
      ret += (wchar_t)(link.size() + 5);
      ret += DROP;
      ret += STRING;
      ret += (wchar_t)(link.size() + 2);
      ret += L'<';
      ret += link;
      ret += L'>';
    }
    // TODO: what does attribute "queue" do?
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lit"))
  {
    ret += STRING;
    wstring v = toWstring(requireAttr(node, (const xmlChar*) "v"));
    ret += (wchar_t)v.size();
    ret += v;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lit-tag"))
  {
    ret += STRING;
    wstring v = L"<" + toWstring(requireAttr(node, (const xmlChar*) "v")) + L">";
    v = StringUtils::substitute(v, L".", L"><");
    if(v == L"<>")
    {
      v = L"";
    }
    ret += (wchar_t)v.size();
    ret += v;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "var"))
  {
    ret += STRING;
    wstring v = toWstring(requireAttr(node, (const xmlChar*) "n"));
    if(vars.find(v) == vars.end() && localVars.find(v) == localVars.end())
    {
      die(node, L"Unknown variable '" + v + L"'.");
    }
    ret += (wchar_t)v.size();
    ret += v;
    ret += FETCHVAR;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "get-case-from"))
  {
    for(xmlNode* c = node->children; c != NULL; c = c->next)
    {
      if(c->type == XML_ELEMENT_NODE)
      {
        if(ret.size() > 0)
        {
          die(node, L"<get-case-from> cannot have multiple children.");
        }
        ret += processValue(c);
      }
    }
    if(ret.size() == 0)
    {
      die(node, L"<get-case-from> cannot be empty.");
    }
    ret += INT;
    ret += (wchar_t)getPos(node);
    ret += PUSHINPUT;
    ret += STRING;
    ret += (wchar_t)3;
    ret += L"lem";
    ret += (inOutput ? TARGETCLIP : SOURCECLIP);
    ret += SETCASE;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "case-of"))
  {
    ret += INT;
    ret += getPos(node);
    ret += PUSHINPUT;
    ret += STRING;
    wstring part = toWstring(requireAttr(node, (const xmlChar*) "part"));
    ret += (wchar_t)part.size();
    ret += part;
    wstring side = toWstring(getAttr(node, (const xmlChar*) "side"));
    if(side == L"sl")
    {
      ret += SOURCECLIP;
    }
    else if(side == L"tl" || side == L"")
    {
      ret += TARGETCLIP;
    }
    else if(side == L"ref")
    {
      ret += REFERENCECLIP;
    }
    else
    {
      warn(node, L"Unknown side '" + side + L"', defaulting to target.");
      ret += TARGETCLIP;
    }
    ret += GETCASE;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "concat"))
  {
    for(xmlNode* c = node->children; c != NULL; c = c->next)
    {
      unsigned int l = ret.size();
      if(c->type == XML_ELEMENT_NODE)
      {
        ret += processValue(c);
        if(l > 0 && ret.size() > l)
        {
          ret += CONCAT;
        }
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lu"))
  {
    ret += CHUNK;
    wstring children;
    for(xmlNode* p = node->children; p != NULL; p = p->next)
    {
      if(p->type == XML_ELEMENT_NODE)
      {
        if(!xmlStrcmp(p->name, (const xmlChar*) "clip"))
        {
          wstring part = toWstring(getAttr(p, (const xmlChar*) "part"));
          if(part == L"whole" || part == L"chcontent" || part == L"content")
          {
            children += INT;
            children += (wchar_t)getPos(p);
            children += PUSHINPUT;
            children += APPENDALLCHILDREN;
            if(part != L"whole") continue;
          }
        }
        ret += processValue(p);
        ret += APPENDSURFACE;
      }
    }
    ret += children;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "mlu"))
  {
    ret += CHUNK;
    for(xmlNode* lu = node->children; lu != NULL; lu = lu->next)
    {
      if(lu->type != XML_ELEMENT_NODE) continue;
      if(xmlStrcmp(lu->name, (const xmlChar*) "lu"))
      {
        die(node, L"<mlu> can only contain <lu>s.");
      }
      if(ret.size() > 1)
      {
        ret += CONJOIN;
        ret += APPENDCHILD;
        // apertium/transfer.cc has checks against appending '' wstring or '+#'
        // TODO?
      }
      ret += processValue(lu);
      ret += APPENDCHILD;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "chunk"))
  {
    ret += CHUNK;
    for(xmlNode* part = node->children; part != NULL; part = part->next)
    {
      if(part->type != XML_ELEMENT_NODE) continue;
      if(!xmlStrcmp(part->name, (const xmlChar*) "source"))
      {
        for(xmlNode* seg = part->children; seg != NULL; seg = seg->next)
        {
          if(seg->type != XML_ELEMENT_NODE) continue;
          ret += processValue(seg);
          ret += APPENDSURFACESL;
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "target"))
      {
        for(xmlNode* seg = part->children; seg != NULL; seg = seg->next)
        {
          if(seg->type != XML_ELEMENT_NODE) continue;
          ret += processValue(seg);
          ret += APPENDSURFACE;
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "reference"))
      {
        for(xmlNode* seg = part->children; seg != NULL; seg = seg->next)
        {
          if(seg->type != XML_ELEMENT_NODE) continue;
          ret += processValue(seg);
          ret += APPENDSURFACEREF;
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "contents"))
      {
        for(xmlNode* seg = part->children; seg != NULL; seg = seg->next)
        {
          if(seg->type != XML_ELEMENT_NODE) continue;
          ret += processValue(seg);
          ret += APPENDCHILD;
        }
      }
    }
    if(!inOutput && currentOutputRule != -1)
    {
      ret += (wchar_t)currentOutputRule;
      ret += (wchar_t)0;
      ret += SETRULE;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lu-count"))
  {
    ret += LUCOUNT;
  }
  else
  {
    die(node, L"Unrecognized expression '" + toWstring(node->name) + L"'");
  }
  return ret;
}

wstring
TRXCompiler::processCond(xmlNode* node)
{
  wstring ret;
  if(!xmlStrcmp(node->name, (const xmlChar*) "and"))
  {
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type != XML_ELEMENT_NODE) continue;
      unsigned int len = ret.size();
      ret += processCond(op);
      if(len > 0 && ret.size() > len)
      {
        ret += AND;
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "or"))
  {
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type != XML_ELEMENT_NODE) continue;
      unsigned int len = ret.size();
      ret += processCond(op);
      if(len > 0 && ret.size() > len)
      {
        ret += OR;
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "not"))
  {
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type != XML_ELEMENT_NODE) continue;
      if(ret.size() > 0)
      {
        die(node, L"<not> cannot have multiple children");
      }
      else
      {
        ret = processCond(op);
        ret += NOT;
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "equal"))
  {
    int i = 0;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type == XML_ELEMENT_NODE)
      {
        ret += processValue(op);
        i++;
      }
    }
    if(i != 2)
    {
      die(node, L"<equal> must have exactly two children");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
    {
      ret += EQUALCL;
    }
    else
    {
      ret += EQUAL;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "begins-with"))
  {
    int i = 0;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type == XML_ELEMENT_NODE)
      {
        ret += processValue(op);
        i++;
      }
    }
    if(i != 2)
    {
      die(node, L"<begins-with> must have exactly two children");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
    {
      ret += ISPREFIXCL;
    }
    else
    {
      ret += ISPREFIX;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "begins-with-list"))
  {
    bool list = false;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type != XML_ELEMENT_NODE) continue;
      if(ret.size() == 0)
      {
        ret += processValue(op);
      }
      else if(list)
      {
        die(node, L"<begins-with-list> cannot have more than two children.");
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, L"Expected <list>, found <" + toWstring(op->name) + L"> instead.");
      }
      else
      {
        wstring name = toWstring(requireAttr(op, (const xmlChar*) "n"));
        if(lists.find(name) == lists.end())
        {
          die(op, L"Unknown list '" + name + L"'.");
        }
        ret += STRING;
        ret += (wchar_t)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, L"<begins-with-list> must have two children.");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
    {
      ret += HASPREFIXCL;
    }
    else
    {
      ret += HASPREFIX;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "ends-with"))
  {
    int i = 0;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type == XML_ELEMENT_NODE)
      {
        ret += processValue(op);
        i++;
      }
    }
    if(i != 2)
    {
      die(node, L"<ends-with> must have exactly two children");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
    {
      ret += ISSUFFIXCL;
    }
    else
    {
      ret += ISSUFFIX;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "ends-with-list"))
  {
    bool list = false;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type != XML_ELEMENT_NODE) continue;
      if(ret.size() == 0)
      {
        ret += processValue(op);
      }
      else if(list)
      {
        die(node, L"<ends-with-list> cannot have more than two children.");
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, L"Expected <list>, found <" + toWstring(op->name) + L"> instead.");
      }
      else
      {
        wstring name = toWstring(requireAttr(op, (const xmlChar*) "n"));
        if(lists.find(name) == lists.end())
        {
          die(op, L"Unknown list '" + name + L"'.");
        }
        ret += STRING;
        ret += (wchar_t)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, L"<ends-with-list> must have two children.");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
    {
      ret += HASSUFFIXCL;
    }
    else
    {
      ret += HASSUFFIX;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "contains-substring"))
  {
    int i = 0;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type == XML_ELEMENT_NODE)
      {
        ret += processValue(op);
        i++;
      }
    }
    if(i != 2)
    {
      die(node, L"<contains-substring> must have exactly two children");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
    {
      ret += ISSUBSTRINGCL;
    }
    else
    {
      ret += ISSUBSTRING;
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "in"))
  {
    bool list = false;
    for(xmlNode* op = node->children; op != NULL; op = op->next)
    {
      if(op->type != XML_ELEMENT_NODE) continue;
      if(ret.size() == 0)
      {
        ret += processValue(op);
      }
      else if(list)
      {
        die(node, L"<in> cannot have more than two children.");
      }
      else if(xmlStrcmp(op->name, (const xmlChar*) "list"))
      {
        die(op, L"Expected <list>, found <" + toWstring(op->name) + L"> instead.");
      }
      else
      {
        wstring name = toWstring(requireAttr(op, (const xmlChar*) "n"));
        if(lists.find(name) == lists.end())
        {
          die(op, L"Unknown list '" + name + L"'.");
        }
        ret += STRING;
        ret += (wchar_t)name.size();
        ret += name;
        list = true;
      }
    }
    if(!list)
    {
      die(node, L"<in> must have two children.");
    }
    if(toWstring(getAttr(node, (const xmlChar*) "caseless")) == L"yes")
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
    die(node, L"Unrecognized condition '" + toWstring(node->name) + L"'");
  }
  return ret;
}

wstring
TRXCompiler::processChoose(xmlNode* node)
{
  vector<pair<wstring, wstring>> clauses;
  int when = 0;
  int otherwise = 0;
  for(xmlNode* cl = node->children; cl != NULL; cl = cl->next)
  {
    if(cl->type != XML_ELEMENT_NODE) continue;
    if(!xmlStrcmp(cl->name, (const xmlChar*) "when"))
    {
      if(otherwise > 0)
      {
        warn(cl, L"Clauses after <otherwise> will not be executed.");
        continue;
      }
      when++;
      wstring test, block;
      for(xmlNode* n = cl->children; n != NULL; n = n->next)
      {
        if(n->type != XML_ELEMENT_NODE) continue;
        if(!xmlStrcmp(n->name, (const xmlChar*) "test"))
        {
          if(test.size() != 0)
          {
            die(n, L"Cannot have multiple <test>s in a <when> clause.");
          }
          for(xmlNode* t = n->children; t != NULL; t = t->next)
          {
            if(t->type != XML_ELEMENT_NODE) continue;
            if(test.size() == 0)
            {
              test = processCond(t);
            }
            else
            {
              die(t, L"<test> must have exactly one child.");
            }
          }
          if(test.size() == 0)
          {
            die(n, L"<test> cannot be empty.");
          }
        }
        else
        {
          if(test.size() == 0)
          {
            die(n, L"<when> clause must begin with <test>.");
          }
          block += processStatement(n);
        }
      }
      clauses.push_back(make_pair(test, block));
    }
    else if(!xmlStrcmp(cl->name, (const xmlChar*) "otherwise"))
    {
      otherwise++;
      if(otherwise > 1)
      {
        warn(cl, L"Multiple <otherwise> clauses will not be executed.");
        continue;
      }
      wstring block;
      for(xmlNode* state = cl->children; state != NULL; state = state->next)
      {
        if(state->type == XML_ELEMENT_NODE)
        {
          block += processStatement(state);
        }
      }
      if(block.size() > 0)
      {
        clauses.push_back(make_pair(L"", block));
      }
      else
      {
        warn(cl, L"Empty <otherwise> clause.");
      }
    }
    else
    {
      warn(cl, L"Ignoring unexpected clause in <choose>.");
    }
  }
  wstring ret;
  for(vector<pair<wstring, wstring>>::reverse_iterator it = clauses.rbegin(),
            limit = clauses.rend(); it != limit; it++)
  {
    wstring act = it->second;
    if(ret.size() > 0)
    {
      act += JUMP;
      act += (wchar_t)ret.size();
    }
    wstring test = it->first;
    if(test.size() > 0)
    {
      test += JUMPONFALSE;
      test += (wchar_t)act.size();
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
    wcerr << L"Error: Cannot open " << binfile << L" for writing." << endl;
    exit(EXIT_FAILURE);
  }
  vector<pair<int, wstring>> inRules;
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
  wcout << "Rules: " << inputRules.size() << endl;
  wcout << "Macros: " << macros.size() << endl;
}
