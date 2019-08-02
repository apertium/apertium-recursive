#ifndef __RTXPATTERNBUILDER__
#define __RTXPATTERNBUILDER__

#include <iostream>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/ltstr.h>
#include <lttoolbox/transducer.h>

#include <string>
#include <vector>
#include <map>

using namespace std;

struct PatternElement
{
  wstring lemma;
  vector<wstring> tags;
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
  map<wstring, wstring, Ltstr> attr_items;

  /**
   * Lists
   * name => { values }
   */
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;

  /**
   * Global variables
   * name => initial value
   */
  map<wstring, wstring, Ltstr> variables;

  /**
   * Symbols marking ends of rules in pattern transducer
   */
  set<int> final_symbols;

  /**
   * Locations of ends of rules, to check for conflicts
   * Note: addPattern() currently forces distinct paths,
   * so this isn't very useful
   * state => rule number
   */
  map<int, int> seen_rules;

  /**
   * Inverse of seen_rules, used by addLookahead()
   * rule number => state
   */
  map<int, vector<int>> rules_to_states;

  /**
   * Alphabet of pattern transducer
   */
  Alphabet alphabet;

  /**
   * Pattern transducer
   */
  Transducer transducer;

  //////////
  // TRANSDUCER PATH BUILDING
  //////////

  /**
   * Starting from base, add path for lemma
   * @return end state
   */
  int insertLemma(int const base, wstring const &lemma);

  /**
   * Starting from base, insert each tag in tags
   * @return end state
   */
  int insertTags(int const base, const vector<wstring>& tags);

  /**
   * Generate symbol of the form L"<RULE_NUMBER:count>" to mark rule end
   */
  int countToFinalSymbol(const int count);

  //////////
  // ATTRIBUTE COMPRESSION
  //////////

  struct TrieNode
  {
    wchar_t self;
    vector<TrieNode*> next;
  };

  /**
   * Construct tries for a set of inputs, return one for each initial character
   */
  vector<TrieNode*> buildTrie(vector<wstring> parts);

  /**
   * Convert trie to regex
   */
  wstring unbuildTrie(TrieNode* t);

  /**
   * Wrapper around buildTrie() and unbuildTrie()
   */
  wstring trie(vector<wstring> parts);

public:

  //////////
  // PUBLIC SETTINGS
  //////////

  // false: * = 1 or more tags, true: * = 0 or more tags
  /**
   * If false, L"*" must match at least one tag, otherwise it can match 0
   * Default: false
   */
  bool starCanBeEmpty;

  /**
   * Debug names for input-time rules
   */
  vector<wstring> inRuleNames;

  /**
   * Debug names for output-time rules
   */
  vector<wstring> outRuleNames;

  PatternBuilder();

  int addPattern(vector<vector<PatternElement*>> pat, int rule, double weight = 0.0);
  void addList(wstring name, set<wstring, Ltstr> vals);
  void addAttr(wstring name, set<wstring, Ltstr> vals);
  bool isAttrDefined(wstring name);
  void addVar(wstring name, wstring val);
  void addLookahead(const int rule, const vector<PatternElement*>& options);
  void write(FILE* output, int longest, vector<pair<int, wstring>> inputBytecode, vector<wstring> outputBytecode);
};

#endif
