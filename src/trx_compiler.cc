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
}

TRXCompiler::~TRXCompiler()
{
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
        PatternElement* cur = new PatternElement;
        cur->lemma = toWstring(getAttr(item, (const xmlChar*) "lemma"));
        wstring tags = toWstring(requireAttr(item, (const xmlChar*) "tags"));
        cur->tags = StringUtils::split_wstring(tags, L".");
        pat.push_back(cur);
      }
      if(patterns.find(name) != patterns.end())
      {
        warn(cat, L"Redefinition of pattern '" + UtfConverter::fromUtf8(name) + L"', using later value");
      }
      patterns[name] = pat;
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
  //pattern
  //action
  // TODO!
}

wstring
TRXCompiler::processStatement(xmlNode* node)
{
  wstring ret;
  if(!xmlStrcmp(node->name, (const xmlChar*) "let"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "out"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "choose"))
  {
    ret = processChoose(node);
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "modify-case"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "call-macro"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "append"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "reject-current-rule"))
  {
    ret = wstring(1, REJECTRULE); 
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
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "clip"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lit"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lit-tag"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "var"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "get-case-from"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "case-of"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "concat"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "lu"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "mlu"))
  {
  }
  else if(!xmlStrcmp(node->name, (const xmlChar*) "chunk"))
  {
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
}

void
TRXCompiler::write(const string& binfile, const string& bytefile)
{
}
