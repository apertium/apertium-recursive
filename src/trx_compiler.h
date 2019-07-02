#ifndef __TRXCOMPILER__
#define __TRXCOMPILER__

#include <apertium/transfer_data.h>
#include <apertium/xml_reader.h>
#include <lttoolbox/ltstr.h>
#include <pattern.h>

#include <libxml/xmlreader.h>
#include <map>
#include <string>

using namespace std;

class TRXCompiler
{
private:
  PatternBuilder PB;

  map<string, pair<int, xmlNode*>, Ltstr> macros;
  map<string, vector<PatternElement*>, Ltstr> patterns;
  map<string, set<string, Ltstr>, Ltstr> attrs;
  map<string, string, Ltstr> attrMangle;
  map<string, string, Ltstr> vars;
  map<string, string, Ltstr> varMangle;
  map<string, set<string, Ltstr>, Ltstr> lists;
  map<string, string, Ltstr> listMangle;
  map<string, int, Ltstr> outputMap;
  map<string, vector<string>, Ltstr> outputNames;
  vector<wstring> inputRules;
  vector<wstring> outputRules;
  bool inOutput;
  xmlDoc* curDoc;
  int curPatternSize;

  void die(xmlNode* node, wstring msg);
  void warn(xmlNode* node, wstring msg);

  xmlChar* requireAttr(xmlNode* node, const xmlChar* attr);
  xmlChar* getAttr(xmlNode* node, const xmlChar* attr);
  string toString(const xmlChar* s);
  wstring toWstring(const xmlChar* s);

  int getPos(xmlNode* node, bool isBlank);

  void processFile(xmlNode* node);
  void processCats(xmlNode* node);
  string insertAttr(string name, set<string, Ltstr> ats);
  void processAttrs(xmlNode* node);
  void processVars(xmlNode* node);
  string insertList(string name, set<string, Ltstr> ats);
  void processLists(xmlNode* node);
  void gatherMacros(xmlNode* node);
  void processRules(xmlNode* node);

  wstring processStatement(xmlNode* node);
  wstring processValue(xmlNode* node);
  wstring processCond(xmlNode* node);
  wstring processChoose(xmlNode* node);
public:
  TRXCompiler();
  ~TRXCompiler();
  void compile(vector<string> files);
  void write(const string& binfile, const string& bytefile);
};

#endif
