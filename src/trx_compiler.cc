#include <apertium/trx_reader.h>
#include <lttoolbox/xml_parse_util.h>
#include <lttoolbox/compression.h>
#include <bytecode.h>
#include <trx_compiler.h>

#include <cstdlib>
#include <iostream>
#include <apertium/string_utils.h>

using namespace Apertium;
using namespace std;

TRXCompiler::TRXCompiler()
{
  // TODO
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
    }
  }
  inOutput = true;
  for(unsigned int i = 0; i < post.size(); i++)
  {
    curDoc = post[i].second;
    processFile(post[i].first);
  }
  inOutput = false;
  for(unsigned int i = 0; i < nonpost.size(); i++)
  {
    curDoc = nonpost[i].second;
    processFile(nonpost[i].first);
  }
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
      else if(!xmlStrcmp(i->name, (const xmlChar*) "section-def-rules"))
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

inline string
TRXCompiler::toString(const xmlChar* s)
{
  return (s == NULL) ? "" : string((char*) s);
}

inline wstring
TRXCompiler::toWstring(const xmlChar* s)
{
  return (s == NULL) ? L"" : UtfConverter::fromUtf8((char*) s);
}

int
TRXCompiler::getPos(xmlNode* node, bool isBlank = false)
{
  string v = toString(requireAttr(node, (const xmlChar*) "pos"));
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
  if(ret <= 0 || ret > curPatternSize || (ret == curPatternSize && isBlank))
  {
    if(isBlank)
    {
      warn(node, L"Disregarding out-of-bounds position.");
      return 0;
    }
    die(node, L"Position " + to_wstring(ret) + L" is out of bounds.");
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
      string name = toString(requireAttr(cat, (const xmlChar*) "n"));
      vector<PatternElement*> pat;
      for(xmlNode* item = cat->children; item != NULL; item = item->next)
      {
        if(xmlStrcmp(cat->name, (const xmlChar*) "cat-item"))
        {
          warn(cat, L"Unexpected tag in def-cat - ignoring");
          continue;
        }
        if(inOutput)
        {
          outputNames[name].push_back(toString(requireAttr(item, (const xmlChar*) "name")));
        }
        else
        {
          PatternElement* cur = new PatternElement;
          cur->lemma = toWstring(getAttr(item, (const xmlChar*) "lemma"));
          wstring tags = toWstring(requireAttr(item, (const xmlChar*) "tags"));
          cur->tags = StringUtils::split_wstring(tags, L".");
          pat.push_back(cur);
        }
      }
      if(patterns.find(name) != patterns.end())
      {
        warn(cat, L"Redefinition of pattern '" + UtfConverter::fromUtf8(name) + L"', using later value");
      }
      if(!inOutput)
      {
        patterns[name] = pat;
      }
    }
  }
}

string
TRXCompiler::insertAttr(string name, set<string, Ltstr> ats)
{
  if(attrs.find(name) == attrs.end())
  {
    attrs[name] = ats;
    return name;
  }
  else
  {
    if(attrs[name].size() != ats.size())
    {
      return insertAttr("*" + name, ats);
    }
    for(set<string, Ltstr>::iterator it = ats.begin(), limit = ats.end();
          it != limit; it++)
    {
      if(attrs[name].find(*it) == attrs[name].end())
      {
        return insertAttr("*" + name, ats);
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
    string name = toString(getAttr(cat, (const xmlChar*) "n"));
    set<string, Ltstr> ats;
    for(xmlNode* item = cat->children; item != NULL; item = item->next)
    {
      if(item->type != XML_ELEMENT_NODE) continue;
      if(xmlStrcmp(item->name, (const xmlChar*) "attr-item"))
      {
        warn(item, L"Unexpected tag in def-attr - ignoring");
        continue;
      }
      ats.insert(toString(getAttr(item, (const xmlChar*) "tags")));
    }
    if(attrMangle.find(name) != attrMangle.end())
    {
      warn(cat, L"Redefinition of attribute '" + UtfConverter::fromUtf8(name) + L"' - using later definition");
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
    string name = toString(requireAttr(var, (const xmlChar*) "n"));
    string mang = name;
    while(vars.find(mang) != vars.end()) mang += "*";
    varMangle[name] = mang;
    vars[mang] = toString(getAttr(var, (const xmlChar*) "v"));
  }
}

string
TRXCompiler::insertList(string name, set<string, Ltstr> ats)
{
  if(lists.find(name) == lists.end())
  {
    lists[name] = ats;
    return name;
  }
  else
  {
    if(lists[name].size() != ats.size())
    {
      return insertList("*" + name, ats);
    }
    for(set<string, Ltstr>::iterator it = ats.begin(), limit = ats.end();
          it != limit; it++)
    {
      if(lists[name].find(*it) == lists[name].end())
      {
        return insertList("*" + name, ats);
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
    string name = toString(getAttr(cat, (const xmlChar*) "n"));
    set<string, Ltstr> ats;
    for(xmlNode* item = cat->children; item != NULL; item = item->next)
    {
      if(item->type != XML_ELEMENT_NODE) continue;
      if(xmlStrcmp(item->name, (const xmlChar*) "list-item"))
      {
        warn(item, L"Unexpected tag in def-list - ignoring");
        continue;
      }
      ats.insert(toString(getAttr(item, (const xmlChar*) "v")));
    }
    if(listMangle.find(name) != listMangle.end())
    {
      warn(cat, L"Redefinition of list '" + UtfConverter::fromUtf8(name) + L"' - using later definition");
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
    string name = toString(requireAttr(mac, (const xmlChar*) "n"));
    int npar = atoi((const char*) requireAttr(mac, (const xmlChar*) "npar"));
    if(macros.find(name) != macros.end())
    {
      warn(mac, L"Redefinition of macro '" + UtfConverter::fromUtf8(name) + L"' - using later definition");
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
          string name = toString(requireAttr(pi, (const xmlChar*) "n"));
          if(inOutput)
          {
            if(curPatternSize > 1)
            {
              die(part, L"Postchunk patterns must be exactly one item long.");
            }
            if(outputNames.find(name) == outputNames.end())
            {
              die(pi, L"Unknown pattern '" + UtfConverter::fromUtf8(name) + L"'.");
            }
            vector<string>& vec = outputNames[name];
            int curRule = outputRules.size();
            for(unsigned int i = 0; i < vec.size(); i++)
            {
              if(outputMap.find(vec[i]) == outputMap.end())
              {
                outputMap[vec[i]] = curRule;
              }
              else
              {
                int other = outputMap[vec[i]];
                warn(rule, L"Rules " + to_wstring(other) + L" and " + to_wstring(curRule) + L" both match '" + UtfConverter::fromUtf8(vec[i]) + L"', the earliest one will be used.");
              }
            }
          }
          else if(patterns.find(name) == patterns.end())
          {
            die(pi, L"Unknown pattern '" + UtfConverter::fromUtf8(name) + L"'.");
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
        if(!inOutput)
        {
          PB.addPattern(pls, inputRules.size() + 1);
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
        for(xmlNode* state = part->children; state != NULL; state = state->next)
        {
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
      if(varMangle.find(UtfConverter::toUtf8(vname)) == varMangle.end())
      {
        die(var, L"Undefined variable '" + vname + L"'.");
      }
      ret += STRING;
      ret += (wchar_t)vname.size();
      ret += vname;
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
      if(name == L"modify-case")
      {
        ret += STRING;
        ret += (wchar_t)part.size();
        ret += part;
        ret += INT;
        ret += (wchar_t)getPos(var);
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
      // TODO: if this is <let> modifying "lem"
      // then we might have to change the output rule
      // which will require modifying SETRULE to look at input nodes
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
    // TODO
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "append"))
  {
    // TODO
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "reject-current-rule"))
  {
    if(toString(getAttr(node, (const xmlChar*) "shifting")) == "yes")
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
    // TODO
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
    ret += STRING;
    ret += (wchar_t)3;
    ret += L"lem";
    ret += INT;
    ret += (wchar_t)getPos(node);
    ret += SOURCECLIP;
    ret += SETCASE;
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "case-of"))
  {
    ret += STRING;
    wstring part = toWstring(requireAttr(node, (const xmlChar*) "part"));
    ret += (wchar_t)part.size();
    ret += part;
    ret += INT;
    ret += getPos(node);
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
        ret += STRING;
        ret += (wchar_t)1;
        ret += L'+';
        ret += APPENDSURFACE;
        // apertium/transfer.cc has checks against appending '' string or '+#'
        // TODO?
      }
      for(xmlNode* p = lu->children; p != NULL; p = p->next)
      {
        if(p->type == XML_ELEMENT_NODE)
        {
          ret += processValue(p);
          ret += APPENDSURFACE;
        }
      }
    }
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "chunk"))
  {
    // TODO
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
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
      else if(xmlStrcmp(node->name, (const xmlChar*) "list"))
      {
        die(op, L"Expected <list>, found <" + toWstring(op->name) + L"> instead.");
      }
      else
      {
        string name = toString(requireAttr(node, (const xmlChar*) "n"));
        if(listMangle.find(name) == listMangle.end())
        {
          die(op, L"Unknown list '" + UtfConverter::fromUtf8(name) + L"'.");
        }
        ret += STRING;
        ret += (wchar_t)name.size();
        ret += UtfConverter::fromUtf8(name);
        list = true;
      }
    }
    if(!list)
    {
      die(node, L"<begins-with-list> must have two children.");
    }
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
      else if(xmlStrcmp(node->name, (const xmlChar*) "list"))
      {
        die(op, L"Expected <list>, found <" + toWstring(op->name) + L"> instead.");
      }
      else
      {
        string name = toString(requireAttr(node, (const xmlChar*) "n"));
        if(listMangle.find(name) == listMangle.end())
        {
          die(op, L"Unknown list '" + UtfConverter::fromUtf8(name) + L"'.");
        }
        ret += STRING;
        ret += (wchar_t)name.size();
        ret += UtfConverter::fromUtf8(name);
        list = true;
      }
    }
    if(!list)
    {
      die(node, L"<ends-with-list> must have two children.");
    }
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
      else if(xmlStrcmp(node->name, (const xmlChar*) "list"))
      {
        die(op, L"Expected <list>, found <" + toWstring(op->name) + L"> instead.");
      }
      else
      {
        string name = toString(requireAttr(node, (const xmlChar*) "n"));
        if(listMangle.find(name) == listMangle.end())
        {
          die(op, L"Unknown list '" + UtfConverter::fromUtf8(name) + L"'.");
        }
        ret += STRING;
        ret += (wchar_t)name.size();
        ret += UtfConverter::fromUtf8(name);
        list = true;
      }
    }
    if(!list)
    {
      die(node, L"<in> must have two children.");
    }
    if(toString(getAttr(node, (const xmlChar*) "caseless")) == "yes")
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
TRXCompiler::write(const string& binfile, const string& bytefile)
{
  // TODO
}
