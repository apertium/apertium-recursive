#ifndef __TRXCOMPILER__
#define __TRXCOMPILER__

#include <rtx_config.h>
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

  //////////
  // DATA
  //////////

  /**
   * Macros defined in the current file
   * name => ( parameters, xml node )
   */
  map<wstring, pair<int, xmlNode*>, Ltstr> macros;

  /**
   * Patterns defined in the current file
   */
  map<wstring, vector<PatternElement*>, Ltstr> patterns;

  /**
   * Global string variables
   * name => initial value
   */
  map<wstring, wstring, Ltstr> vars;

  /**
   * Rule-specific string variable names
   */
  set<wstring, Ltstr> localVars;

  /**
   * All lists
   */
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;

  /**
   * Ids of rules which should not be compiled
   */
  set<wstring, Ltstr> excludedRules;

  /**
   * Bytecode for non-postchunk rules
   */
  vector<wstring> inputRules;

  /**
   * Sizes of patterns for non-postchunk rules
   */
  vector<int> inputRuleSizes;

  /**
   * Bytecode for postchunk rules
   */
  vector<wstring> outputRules;

  /**
   * Remapped positions within macros
   * vector<vector> because macros can call other macros
   */
  vector<vector<int>> macroPosShift;

  /**
   * Whether the rule currently being compiled is a postchunk rule or not
   */
  bool inOutput;

  /**
   * Index of output-time bytecode for this rule
   * set to -1 if there is no bytecode
   */
  int currentOutputRule;

  /**
   * Pointer to the current file for error message purposes
   */
  xmlDoc* curDoc;

  /**
   * Length of the pattern of the current rule
   */
  int curPatternSize;

  /**
   * Length of the longest pattern
   */
  int longestPattern;

  //////////
  // ERRORS
  //////////

  /**
   * Report a fatal error and exit
   * @param node - xml element closest to the error
   */
  void die(xmlNode* node, wstring msg);

  /**
   * Report a non-fatal error
   * @param node - xml element closest to the error
   */
  void warn(xmlNode* node, wstring msg);

  //////////
  // PARSING UTILITIES
  //////////

  /**
   * Return the value of an attribute or an empty string
   * @param node - xml element
   * @param attr - name of attribute
   * @return attribute value or empty string
   */
  xmlChar* getAttr(xmlNode* node, const xmlChar* attr);

  /**
   * getAttr(), but calls die() if attribute isn't found
   * @param node - xml element
   * @param attr - name of attribute
   * @return attribute value
   */
  xmlChar* requireAttr(xmlNode* node, const xmlChar* attr);

  /**
   * Convert a the libxml string format to std::wstring
   * @param s - libxml string
   * @return equivalent wstring
   */
  wstring toWstring(const xmlChar* s);

  /**
   * Parse pos attribute and convert appropriately if in a macro
   * @param node - the node
   * @param isBlank - true if the position being parsed if for a blank,
   *      false otherwise and by default
   * @return position as an interger
   */
  int getPos(xmlNode* node, bool isBlank);

  //////////
  // PATTERNBUILDER INTERACTION
  //////////

  /**
   * Concatenate all other postchunk
   * for chunks whose lemmas are not known at compile-time
   */
  void makeDefaultOutputRule();

  /**
   * Pass an attribute category to PatternBuilder, name-mangling if necessary
   * @param name - attribute name
   * @param ats - category elements
   * @return inserted name, may or may not be equal to name
   */
  wstring insertAttr(wstring name, set<wstring, Ltstr> ats);

  /**
   * Pass a list to PatternBuilder, name-mangling if necessary
   * @param name - list name
   * @param ats - list elements
   * @return inserted name, may or may not be equal to name
   */
  wstring insertList(wstring name, set<wstring, Ltstr> ats);

  //////////
  // XML PARSING
  //////////

  /**
   * Parse and compile <transfer>, <interchunk>, or <postchunk>
   * @param node - the root node of the file
   */
  void processFile(xmlNode* node);

  /**
   * Parse and compile <section-def-cats>
   */
  void processCats(xmlNode* node);

  /**
   * Parse and compile <section-def-attrs>
   */
  void processAttrs(xmlNode* node);

  /**
   * Parse and compile <section-def-vars>
   */
  void processVars(xmlNode* node);

  /**
   * Parse and compile <section-def-lists>
   */
  void processLists(xmlNode* node);

  /**
   * Iterate over <section-def-macros> and store for later use
   */
  void gatherMacros(xmlNode* node);

  /**
   * Parse and compile <section-rules>
   */
  void processRules(xmlNode* node);

  /**
   * Parse and compile one of
   * <let>, <out>, <choose>, <modify-case>, <call-macro>, <append>, <reject-current-rule>
   * @return bytecode
   */
  wstring processStatement(xmlNode* node);

  /**
   * Parse and compile one of
   * <b>, <clip>, <lit>, <lit-tag>, <var>, <get-case-from>, <case-of>,
   * <concat>, <lu>, <mlu>, <chunk>, <lu-count>
   * @return bytecode
   */
  wstring processValue(xmlNode* node);

  /**
   * Parse and compile one of
   * <and>, <or>, <not>, <equal>, <begins-with>, <begins-with-list>,
   * <ends-with>, <ends-with-list>, <contains-substring>, <in>
   * @return bytecode
   */
  wstring processCond(xmlNode* node);

  /**
   * Parse and compile <choose>
   * @return bytecode
   */
  wstring processChoose(xmlNode* node);

public:
  TRXCompiler();
  ~TRXCompiler();
  void loadLex(const string& fname);
  void compile(string file);
  void write(const char* binfile);
  void excludeRule(wstring name)
  {
    excludedRules.insert(name);
  }
  void printStats();
};

#endif
