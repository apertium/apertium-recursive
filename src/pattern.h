#ifndef __RTXPATTERNBUILDER__
#define __RTXPATTERNBUILDER__

#include <iostream>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/ustring.h>
#include <lttoolbox/transducer.h>

#include <string>
#include <vector>
#include <map>

using namespace std;

struct PatternElement
{
  UString lemma;
  vector<UString> tags;
};

class PatternBuilder
{
private:

  //////////
  // DATA
  //////////

  /**
   * Attribute categories
   * name => regex
   */
  map<UString, UString> attr_items;

  /**
   * Lists
   * name => { values }
   */
  map<UString, set<UString>> lists;

  /**
   * Global string variables
   * name => initial value
   */
  map<UString, UString> variables;

  /**
   * Symbols marking ends of rules in pattern transducer
   */
  set<int> final_symbols;

  map<int, vector<UString>> lookahead;
  map<UString, set<UString>> firstSet;

  /**
   * Alphabet of pattern transducer
   */
  Alphabet alphabet;

  /**
   * Pattern transducer
   */
  Transducer transducer;

  /**
   * Lexicalized weights for rules
   * rule id => [ ( weight, processed pattern ) ... ]
   */
  map<UString, vector<pair<double, vector<vector<PatternElement*>>>>> lexicalizations;

  map<int, pair<vector<UString>, vector<vector<PatternElement*>>>> rules;

  //////////
  // TRANSDUCER PATH BUILDING
  //////////

  /**
   * Starting from base, add path for lemma
   * @return end state
   */
  int insertLemma(int const base, UString const &lemma);

  /**
   * Starting from base, insert each tag in tags
   * @return end state
   */
  int insertTags(int const base, const vector<UString>& tags);

  /**
   * Generate symbol of the form "<RULE_NUMBER:count>" to mark rule end
   */
  int countToFinalSymbol(const int count);

  /**
   * Build complete path
   */
  void addPattern(const vector<vector<PatternElement*>>& pat, int rule, double weight, bool isLex);

  void buildLookahead();

  bool isPrefix(const vector<vector<PatternElement*>>& rule, const vector<vector<PatternElement*>>& prefix);

  void buildFallback();

public:

  //////////
  // PUBLIC SETTINGS
  //////////

  // false: * = 1 or more tags, true: * = 0 or more tags
  /**
   * If false, "*" must match at least one tag, otherwise it can match 0
   */
  bool starCanBeEmpty = false;

  /**
   * Number of global Chunk* variables to allocate space for
   */
  unsigned int chunkVarCount = 0;

  /**
   * Debug names for input-time rules
   */
  vector<UString> inRuleNames;

  /**
   * Debug names for output-time rules
   */
  vector<UString> outRuleNames;

  PatternBuilder();

  void addRule(int rule, double weight, const vector<vector<PatternElement*>>& pattern, const vector<UString>& firstChunk, const UString& name);
  void addList(const UString& name, const set<UString>& vals);
  void addAttr(const UString& name, const set<UString>& vals);
  bool isAttrDefined(const UString& name);
  void addVar(const UString& name, const UString& val);
  void loadLexFile(const string& fname);
  void write(FILE* output, int longest, vector<pair<int, UString>> inputBytecode, vector<UString> outputBytecode);

  //////////
  // BYTECODE CONSTRUCTION
  //////////
  UString BCstring(const UString& s);
  UString BCifthenelse(const UString& cond, const UString& yes, const UString& no);
};

#endif
