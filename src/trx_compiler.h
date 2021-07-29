#ifndef __TRXCOMPILER__
#define __TRXCOMPILER__

#include <apertium/transfer_data.h>
#include <apertium/xml_reader.h>
#include <pattern.h>

#include <libxml/xmlreader.h>
#include <map>
#include <string>
#include <cstdarg>

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
  map<UString, pair<int, xmlNode*>> macros;

  /**
   * Patterns defined in the current file
   */
  map<UString, vector<PatternElement*>> patterns;

  /**
   * Global string variables
   * name => initial value
   */
  map<UString, UString> vars;

  /**
   * Rule-specific string variable names
   */
  set<UString> localVars;

  /**
   * All lists
   */
  map<UString, set<UString>> lists;

  /**
   * Ids of rules which should not be compiled
   */
  set<UString> excludedRules;

  /**
   * Bytecode for non-postchunk rules
   */
  vector<UString> inputRules;

  /**
   * Sizes of patterns for non-postchunk rules
   */
  vector<int> inputRuleSizes;

  /**
   * Bytecode for postchunk rules
   */
  vector<UString> outputRules;

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
  void die(xmlNode* node, const char* fmt, ...);

  /**
   * Report a non-fatal error
   * @param node - xml element closest to the error
   */
  void warn(xmlNode* node, const char* fmt, ...);

  //////////
  // PARSING UTILITIES
  //////////

  /**
   * getAttr(), but calls die() if attribute isn't found
   * @param node - xml element
   * @param attr - name of attribute
   * @return attribute value
   */
  UString requireAttr(xmlNode* node, const char* attr);

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
  UString insertAttr(UString name, set<UString> ats);

  /**
   * Pass a list to PatternBuilder, name-mangling if necessary
   * @param name - list name
   * @param ats - list elements
   * @return inserted name, may or may not be equal to name
   */
  UString insertList(UString name, set<UString> ats);

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
  UString processStatement(xmlNode* node);

  /**
   * Parse and compile one of
   * <b>, <clip>, <lit>, <lit-tag>, <var>, <get-case-from>, <case-of>,
   * <concat>, <lu>, <mlu>, <chunk>, <lu-count>
   * @return bytecode
   */
  UString processValue(xmlNode* node);

  /**
   * Parse and compile one of
   * <and>, <or>, <not>, <equal>, <begins-with>, <begins-with-list>,
   * <ends-with>, <ends-with-list>, <contains-substring>, <in>
   * @return bytecode
   */
  UString processCond(xmlNode* node);

  /**
   * Parse and compile <choose>
   * @return bytecode
   */
  UString processChoose(xmlNode* node);

public:
  TRXCompiler();
  ~TRXCompiler();
  void loadLex(const string& fname);
  void compile(string file);
  void write(const char* binfile);
  void excludeRule(UString name)
  {
    excludedRules.insert(name);
  }
  void printStats();
};

#endif
