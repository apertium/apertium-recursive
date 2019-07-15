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

  map<wstring, pair<int, xmlNode*>, Ltstr> macros;
  map<wstring, vector<PatternElement*>, Ltstr> patterns;
  map<wstring, set<wstring, Ltstr>, Ltstr> attrs;
  map<wstring, wstring, Ltstr> attrMangle;
  map<wstring, wstring, Ltstr> vars;
  map<wstring, wstring, Ltstr> varMangle;
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;
  map<wstring, wstring, Ltstr> listMangle;
  map<wstring, int, Ltstr> outputMap;
  map<wstring, vector<wstring>, Ltstr> outputNames;
  vector<wstring> inputRules;
  vector<int> inputRuleSizes;
  vector<wstring> outputRules;
  vector<int> macroPosShift;
  bool inOutput;
  xmlDoc* curDoc;
  int curPatternSize;
  int longestPattern;

  void die(xmlNode* node, wstring msg);
  void warn(xmlNode* node, wstring msg);

  xmlChar* requireAttr(xmlNode* node, const xmlChar* attr);
  xmlChar* getAttr(xmlNode* node, const xmlChar* attr);
  wstring toString(const xmlChar* s);
  wstring toWstring(const xmlChar* s);

  int getPos(xmlNode* node, bool isBlank);

  void processFile(xmlNode* node);
  void makeDefaultOutputRule();
  void processCats(xmlNode* node);
  wstring insertAttr(wstring name, set<wstring, Ltstr> ats);
  void processAttrs(xmlNode* node);
  void processVars(xmlNode* node);
  wstring insertList(wstring name, set<wstring, Ltstr> ats);
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
  void write(const char* binfile);
};

#endif
