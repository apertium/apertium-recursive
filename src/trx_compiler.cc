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
TRXCompiler::compile(vector<string> files)
{
  vector<pair<xmlNode*, xmlDoc*>> post;
  vector<pair<xmlNode*, xmlDoc*>> nonpost;
  for(unsigned int i = 0; i < files.size(); i++)
  {
    xmlDoc* doc = xmlReadFile(files[i].c_str(), NULL, 0);
    if(doc == NULL)
    {
      wcerr << "Error: Could not parse file '" << files[i] << "'." << endl;
      exit(EXIT_FAILURE);
    }
    xmlNode* root = xmlDocGetRootElement(doc);
    if(!xmlStrcmp(root->name, (const xmlChar*) "postchunk"))
    {
      post.push_back(make_pair(root, doc));
    }
    else
    {
      nonpost.push_back(make_pair(root, doc));
      // TODO: <transfer default="chunk"> vs <transfer default="lu">
    }
  }
  // We compile postchunk first so that non-postchunk knows what rules exist
  inOutput = true;
  for(unsigned int i = 0; i < post.size(); i++)
  {
    curDoc = post[i].second;
    processFile(post[i].first);
  }
  makeDefaultOutputRule();
  inOutput = false;
  for(unsigned int i = 0; i < nonpost.size(); i++)
  {
    curDoc = nonpost[i].second;
    processFile(nonpost[i].first);
  }
}

void
TRXCompiler::makeDefaultOutputRule()
{
  wstring cl;
  cl += INT;
  cl += (wchar_t)0;
  cl += PUSHINPUT;
  cl += STRING;
  cl += (wchar_t)3;
  cl += L"lem";
  cl += TARGETCLIP;
  vector<wstring> cond;
  cond.resize(outputRules.size());
  for(map<wstring, int, Ltstr>::iterator it = outputMap.begin(),
          limit = outputMap.end(); it != limit; it++)
  {
    wstring eq;
    if(cond[it->second].size() > 0)
    {
      eq += OVER;
    }
    else
    {
      eq += DUP;
    }
    eq += STRING;
    eq += (wchar_t)it->first.size();
    eq += it->first;
    eq += EQUALCL;
    if(cond[it->second].size() > 0)
    {
      eq += OR;
    }
    cond[it->second] += eq;
  }
  wstring ret;
  ret += INT;
  ret += (wchar_t)0;
  ret += PUSHINPUT;
  ret += STRING;
  ret += (wchar_t)3;
  ret += L"lem";
  ret += TARGETCLIP;
  ret += GETCASE;
  ret += STRING;
  ret += (wchar_t)2;
  ret += L"Aa";
  ret += EQUAL;

  wstring ifblock;
  ifblock += INT;
  ifblock += (wchar_t)1;
  ifblock += PUSHINPUT;
  ifblock += STRING;
  ifblock += (wchar_t)3;
  ifblock += L"lem";
  ifblock += TARGETCLIP;
  ifblock += STRING;
  ifblock += (wchar_t)2;
  ifblock += L"Aa";
  ifblock += SETCASE;
  ifblock += STRING;
  ifblock += (wchar_t)3;
  ifblock += L"lem";
  ifblock += INT;
  ifblock += (wchar_t)1;
  ifblock += SETCLIP;

  ret += JUMPONFALSE;
  ret += (wchar_t)ifblock.size();
  ret += ifblock;
  ret += OUTPUTALL;
  for(vector<wstring>::reverse_iterator c = cond.rbegin(), limit = cond.rend(), a = outputRules.rbegin();
          c != limit; c++, a++)
  {
    ret = *a + wstring(1, JUMP) + wstring(1, ret.size()) + ret;
    ret = *c + wstring(1, JUMPONFALSE) + wstring(1, (*a).size()+2) + ret;
  }
  ret = cl + ret;
  outputRules.push_back(ret);
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
  patterns.clear();
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
        if(inOutput)
        {
          outputNames[name].push_back(toWstring(requireAttr(item, (const xmlChar*) "name")));
        }
        else
        {
          PatternElement* cur = new PatternElement;
          cur->lemma = toWstring(getAttr(item, (const xmlChar*) "lemma"));
          wstring tags = toWstring(requireAttr(item, (const xmlChar*) "tags"));
          if(tags == L"") tags = L"UNKNOWN:INTERNAL";
          cur->tags = StringUtils::split_wstring(tags, L".");
          pat.push_back(cur);
        }
      }
      if(patterns.find(name) != patterns.end())
      {
        warn(cat, L"Redefinition of pattern '" + name + L"', using later value");
      }
      if(!inOutput)
      {
        patterns[name] = pat;
      }
    }
  }
}

wstring
TRXCompiler::insertAttr(wstring name, set<wstring, Ltstr> ats)
{
  if(attrs.find(name) == attrs.end())
  {
    attrs[name] = ats;
    PB.addAttr(name, ats);
    return name;
  }
  else
  {
    if(attrs[name].size() != ats.size())
    {
      return insertAttr(L"*" + name, ats);
    }
    for(set<wstring, Ltstr>::iterator it = ats.begin(), limit = ats.end();
          it != limit; it++)
    {
      if(attrs[name].find(*it) == attrs[name].end())
      {
        return insertAttr(L"*" + name, ats);
      }
    }
    return name;
  }
}

void
TRXCompiler::processAttrs(xmlNode* node)
{
  attrMangle.clear();
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
    if(attrMangle.find(name) != attrMangle.end())
    {
      warn(cat, L"Redefinition of attribute '" + name + L"' - using later definition");
    }
    attrMangle[name] = insertAttr(name, ats);
  }
}

void
TRXCompiler::processVars(xmlNode* node)
{
  varMangle.clear();
  for(xmlNode* var = node->children; var != NULL; var = var->next)
  {
    if(var->type != XML_ELEMENT_NODE) continue;
    if(xmlStrcmp(var->name, (const xmlChar*) "def-var"))
    {
      warn(var, L"Unexpected tag in section-def-vars - ignoring");
      continue;
    }
    wstring name = toWstring(requireAttr(var, (const xmlChar*) "n"));
    wstring mang = name;
    // unlike lists and attributes, we don't want to deduplicate variables
    while(vars.find(mang) != vars.end()) mang += L"*";
    varMangle[name] = mang;
    vars[mang] = toWstring(getAttr(var, (const xmlChar*) "v"));
    PB.addVar(mang, vars[mang]);
  }
}

wstring
TRXCompiler::insertList(wstring name, set<wstring, Ltstr> ats)
{
  if(lists.find(name) == lists.end())
  {
    lists[name] = ats;
    PB.addList(name, ats);
    return name;
  }
  else
  {
    if(lists[name].size() != ats.size())
    {
      return insertList(L"*" + name, ats);
    }
    for(set<wstring, Ltstr>::iterator it = ats.begin(), limit = ats.end();
          it != limit; it++)
    {
      if(lists[name].find(*it) == lists[name].end())
      {
        return insertList(L"*" + name, ats);
      }
    }
    return name;
  }
}

void
TRXCompiler::processLists(xmlNode* node)
{
  listMangle.clear();
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
    if(listMangle.find(name) != listMangle.end())
    {
      warn(cat, L"Redefinition of list '" + name + L"' - using later definition");
    }
    listMangle[name] = insertList(name, ats);
  }
}

void
TRXCompiler::gatherMacros(xmlNode* node)
{
  macros.clear();
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
    curPatternSize = 0;
    if(rule->type != XML_ELEMENT_NODE) continue;
    if(xmlStrcmp(rule->name, (const xmlChar*) "rule"))
    {
      warn(rule, L"Ignoring non-<rule> element in <section-rules>.");
      continue;
    }
    wstring id = toWstring(getAttr(rule, (const xmlChar*) "id"));
    wstring weight = toWstring(getAttr(rule, (const xmlChar*) "weight"));
    wstring firstChunk = toWstring(getAttr(rule, (const xmlChar*) "firstChunk"));
    if(firstChunk == L"") firstChunk = L"*";
    bool pat = false;
    bool act = false;
    for(xmlNode* part = rule->children; part != NULL; part = part->next)
    {
      if(part->type != XML_ELEMENT_NODE) continue;
      if(!xmlStrcmp(part->name, (const xmlChar*) "pattern"))
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
          if(inOutput)
          {
            if(curPatternSize > 1)
            {
              die(part, L"Postchunk patterns must be exactly one item long.");
            }
            if(outputNames.find(name) == outputNames.end())
            {
              die(pi, L"Unknown pattern '" + name + L"'.");
            }
            vector<wstring>& vec = outputNames[name];
            int curRule = outputRules.size();
            for(unsigned int i = 0; i < vec.size(); i++)
            {
              wstring nm = StringUtils::tolower(vec[i]);
              if(outputMap.find(nm) == outputMap.end())
              {
                outputMap[nm] = curRule;
              }
              else
              {
                int other = outputMap[nm];
                warn(rule, L"Rules " + to_wstring(other) + L" and " + to_wstring(curRule) + L" both match '" + nm + L"', the earliest one will be used.");
              }
            }
          }
          else if(patterns.find(name) == patterns.end())
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
        if(!inOutput)
        {
          PB.addRule(inputRules.size() + 1, (weight.size() > 0 ? stod(weight) : 0.0), pls, StringUtils::split_wstring(firstChunk, L" "), id);
          inputRuleSizes.push_back(pls.size());
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "action"))
      {
        if(act)
        {
          die(rule, L"Rule cannot have multiple <action>s.");
        }
        act = true;
        wstring action;
        if(inOutput)
        {
          action += INT;
          action += (wchar_t)0;
          action += PUSHINPUT;
          action += STRING;
          action += (wchar_t)3;
          action += L"lem";
          action += TARGETCLIP;
          action += GETCASE;
          action += STRING;
          action += (wchar_t)2;
          action += L"Aa";
          action += EQUAL;

          wstring ifblock;
          ifblock += INT;
          ifblock += (wchar_t)1;
          ifblock += PUSHINPUT;
          ifblock += STRING;
          ifblock += (wchar_t)3;
          ifblock += L"lem";
          ifblock += TARGETCLIP;
          ifblock += STRING;
          ifblock += (wchar_t)2;
          ifblock += L"Aa";
          ifblock += SETCASE;
          ifblock += STRING;
          ifblock += (wchar_t)3;
          ifblock += L"lem";
          ifblock += INT;
          ifblock += (wchar_t)1;
          ifblock += SETCLIP;

          action += JUMPONFALSE;
          action += (wchar_t)ifblock.size();
          action += ifblock;
        }
        for(xmlNode* state = part->children; state != NULL; state = state->next)
        {
          if(state->type != XML_ELEMENT_NODE) continue;
          action += processStatement(state);
        }
        if(inOutput)
        {
          outputRules.push_back(action);
        }
        else
        {
          inputRules.push_back(action);
        }
      }
      else
      {
        warn(part, L"Ignorning non-<pattern> non-<action> content of <rule>.");
      }
    }
    if(!pat)
    {
      die(rule, L"Rule must have <pattern>.");
    }
    if(!act)
    {
      die(rule, L"Rule must have <action>.");
    }
  }
}

wstring
TRXCompiler::processStatement(xmlNode* node)
{
  wstring ret;
  if(!xmlStrcmp(node->name, (const xmlChar*) "let") ||
     !xmlStrcmp(node->name, (const xmlChar*) "modify-case"))
  {
    wstring name = toWstring(node->name);
    xmlNode* var = NULL;
    wstring val;
    for(xmlNode* n = node->children; n != NULL; n = n->next)
    {
      if(n->type != XML_ELEMENT_NODE) continue;
      if(var == NULL)
      {
        var = n;
      }
      else if(val.size() == 0)
      {
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
      if(varMangle.find(vname) == varMangle.end())
      {
        die(var, L"Undefined variable '" + vname + L"'.");
      }
      else
      {
        vname = varMangle[vname];
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
      if(attrMangle.find(part) != attrMangle.end())
      {
        part = attrMangle[part];
        // there's some checking to do here...
      }
      if(!PB.isAttrDefined(part))
      {
        die(var, L"Unknown attribute '" + part + L"'");
      }
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
      }
      else
      {
        ret = val;
      }
      ret += STRING;
      ret += (wchar_t)part.size();
      ret += part;
      ret += INT;
      ret += (wchar_t)getPos(var);
      ret += SETCLIP;
      if(!inOutput && (part == L"lem" || part == L"lemh" || part == L"lemq"))
      {
        ret += INT;
        ret += (wchar_t)(outputRules.size()-1);
        ret += INT;
        ret += (wchar_t)getPos(var);
        ret += SETRULE;
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
    if(varMangle.find(name) != varMangle.end())
    {
      name = varMangle[name];
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
    if(toWstring(getAttr(node, (const xmlChar*) "shifting")) == L"yes")
    {
      warn(node, L"Bytecode VM cannot shift after rejecting a rule - disregarding.");
    }
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
    if(attrMangle.find(part) != attrMangle.end())
    {
      part = attrMangle[part];
    }
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
    ret += (wchar_t)v.size();
    ret += v;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "var"))
  {
    ret += STRING;
    wstring v = toWstring(requireAttr(node, (const xmlChar*) "n"));
    if(varMangle.find(v) != varMangle.end())
    {
      v = varMangle[v];
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
    for(xmlNode* p = node->children; p != NULL; p = p->next)
    {
      if(p->type == XML_ELEMENT_NODE)
      {
        ret += processValue(p);
        ret += APPENDSURFACE;
      }
    }
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
      for(xmlNode* p = lu->children; p != NULL; p = p->next)
      {
        if(p->type == XML_ELEMENT_NODE)
        {
          ret += processValue(p);
          ret += APPENDCHILD;
        }
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "chunk"))
  {
    ret += CHUNK;
    bool inLitChunk = false;
    bool surface = true;
    wstring whole;
    int partct = 0;
    for(xmlNode* part = node->children; part != NULL; part = part->next)
    {
      if(part->type != XML_ELEMENT_NODE) continue;
      partct++;
      if(!xmlStrcmp(part->name, (const xmlChar*) "tags"))
      {
        surface = false;
        if(ret.size() > 1)
        {
          die(part, L"<tags> must be the first child of <chunk>.");
        }
        wstring name = toWstring(getAttr(node, (const xmlChar*) "name"));
        wstring namefrom = toWstring(getAttr(node, (const xmlChar*) "namefrom"));
        if(varMangle.find(namefrom) != varMangle.end())
        {
          namefrom = varMangle[namefrom];
        }
        wstring casevar = toWstring(getAttr(node, (const xmlChar*) "case"));
        if(varMangle.find(casevar) != varMangle.end())
        {
          casevar = varMangle[casevar];
        }
        wstring csadd;
        if(casevar.size() != 0)
        {
          csadd += STRING;
          csadd += (wchar_t)casevar.size();
          csadd += casevar;
          csadd += FETCHVAR;
          csadd += SETCASE;
        }
        // TODO: what happens when name and namefrom are both empty?
        if(namefrom.size() == 0)
        {
          wstring lowname = StringUtils::tolower(name);
          ret += STRING;
          if(name.size() == 0)
          {
            ret += (wchar_t)7;
            ret += L"default";
            lowname = L"default";
          }
          else
          {
            ret += (wchar_t)name.size();
            ret += name;
          }
          ret += csadd;
          ret += APPENDSURFACE;
          ret += INT;
          if(outputMap.find(lowname) != outputMap.end())
          {
            ret += (wchar_t)(outputMap[lowname]);
          }
          else
          {
            ret += (wchar_t)(outputRules.size()-1);
          }
          ret += INT;
          ret += (wchar_t)0;
          ret += SETRULE;
        }
        else
        {
          ret += STRING;
          ret += (wchar_t)namefrom.size();
          ret += namefrom;
          ret += FETCHVAR;
          ret += csadd;
          ret += APPENDSURFACE;
          ret += INT;
          ret += (wchar_t)(outputRules.size()-1);
          ret += INT;
          ret += (wchar_t)0;
          ret += SETRULE;
        }
        for(xmlNode* tag = part->children; tag != NULL; tag = tag->next)
        {
          if(tag->type != XML_ELEMENT_NODE) continue;
          if(xmlStrcmp(tag->name, (const xmlChar*) "tag"))
          {
            warn(tag, L"Ignoring non-<tag> material in <tags>.");
            continue;
          }
          for(xmlNode* v = tag->children; v != NULL; v = v->next)
          {
            if(v->type == XML_ELEMENT_NODE)
            {
              ret += processValue(v);
              ret += APPENDSURFACE;
              break;
            }
          }
        }
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "var"))
      {
        ret += CHUNK;
        ret += STRING;
        wstring name = toWstring(requireAttr(part, (const xmlChar*) "n"));
        if(varMangle.find(name) != varMangle.end())
        {
          name = varMangle[name];
        }
        ret += (wchar_t)name.size();
        ret += name;
        ret += FETCHVAR;
        ret += APPENDSURFACE;
        ret += APPENDCHILD;
      }
      else if(surface && !xmlStrcmp(part->name, (const xmlChar*) "lit"))
      {
        wstring val = toWstring(requireAttr(part, (const xmlChar*) "v"));
        wstring app;
        if(inLitChunk)
        {
          if(val.size() >= 2 && val[val.size()-2] == L'$' && val[val.size()-1] == L'}')
          {
            app += APPENDCHILD;
            val = val.substr(0, val.size()-2);
            inLitChunk = false;
            if(val.size() == 0)
            {
              continue;
            }
          }
          if(val.find(L"}") != wstring::npos)
          {
            warn(part, L"Possible end of literal chunk detected. Unable to properly compile if so.");
          }
        }
        else
        {
          if(val.size() >= 2 && val[0] == L'{' && val[1] == L'^')
          {
            ret += CHUNK;
            val = val.substr(2);
            inLitChunk = true;
            if(val.size() == 0)
            {
              continue;
            }
          }
          if(val.find(L"{") != wstring::npos)
          {
            warn(part, L"Possible start of literal chunk detected. Unable to properly compile if so.");
          }
        }
        ret += STRING;
        ret += (wchar_t)val.size();
        ret += val;
        ret += APPENDSURFACE;
        ret += app;
      }
      else if(!xmlStrcmp(part->name, (const xmlChar*) "clip"))
      {
        // TODO: maybe there should be some error checking here
        // also, generally ensure that we're not getting chunks and wstrings mixed up
        wstring temp = processValue(part);
        ret += temp;
        wstring prt = toWstring(getAttr(part, (const xmlChar*) "part"));
        if(prt == L"whole")
        {
          whole = temp;
          ret += APPENDCHILD;
        }
        else if(prt == L"content" || prt == L"chcontent")
        {
          ret += APPENDALLCHILDREN;
        }
        else
        {
          ret += APPENDSURFACE;
          if(!inOutput && (prt == L"lem" || prt == L"lemh"))
          {
            ret += INT;
            ret += (wchar_t)getPos(part);
            ret += GETRULE;
            ret += INT;
            ret += (wchar_t)0;
            ret += SETRULE;
          }
        }
      }
      else
      {
        ret += processValue(part);
        ret += surface ? APPENDSURFACE : APPENDCHILD;
      }
    }
    if(whole.size() > 0 && partct == 1)
    {
      ret = whole;
    }
  }
  // <pseudolemma> seems not to have been implemented
  // so I can't actually determine what it's supposed to do
  //else if(!xmlStrcmp(node->name, (const xmlChar*) "pseudolemma"))
  //{
  //}
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
        if(listMangle.find(name) == listMangle.end())
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
        if(listMangle.find(name) == listMangle.end())
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
        if(listMangle.find(name) == listMangle.end())
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
    inRules.push_back(make_pair(inputRuleSizes[i], inputRules[i]));
  }
  PB.write(bin, longestPattern, inRules, outputRules);
  fclose(bin);
}
