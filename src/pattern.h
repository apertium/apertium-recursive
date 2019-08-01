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
  map<wstring, wstring, Ltstr> attr_items;
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;
  map<wstring, wstring, Ltstr> variables;
  set<int> final_symbols;

  map<int, int> seen_rules;
  map<int, int> rules_to_states;

  Alphabet alphabet;
  Transducer transducer;

  int insertLemma(int const base, wstring const &lemma);
  int insertTags(int const base, const vector<wstring>& tags);
  int countToFinalSymbol(const int count);

  struct TrieNode
  {
    wchar_t self;
    vector<TrieNode*> next;
  };
  vector<TrieNode*> buildTrie(vector<wstring> parts);
  wstring unbuildTrie(TrieNode* t);
  wstring trie(vector<wstring> parts);

public:
  // false: * = 1 or more tags, true: * = 0 or more tags
  bool starCanBeEmpty;
  vector<wstring> inRuleNames;
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
