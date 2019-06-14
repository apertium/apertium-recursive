#ifndef _RTXREADER_
#define _RTXREADER_

#include <apertium/transfer_data.h>
#include <lttoolbox/ltstr.h>
#include <apertium/utf_converter.h>
#include <bytecode.h>

#include <map>
#include <string>
#include <fstream>
#include <vector>

using namespace std;

class RTXReader
{
private:
  struct LemmaTags
  {
    wstring lemma;
    wstring tags;
  };
  
  struct VarUpdate
  {
    int src;
    int dest;
    wstring srcvar;
    wstring destvar;
    wstring side;
  };

  struct Cond
  {
    wchar_t op;
    VarUpdate* val;
    Cond* left;
    Cond* right;
  };

  struct ResultNode
  {
    wstring mode;
    int pos;
    wstring lemma;
    map<wstring, pair<int, wstring>> clips;
    vector<wstring> tags;
    bool getall;
    bool dontoverwrite;
    vector<VarUpdate*> updates;
  };

  struct OutputChunk
  {
    Cond* cond;
    int pos;
    vector<ResultNode*> children;
  };

  struct Rule
  {
    int line;
    int grab_all;
    float weight;
    int patternLength;
    vector<vector<wstring>> pattern;
    vector<wstring> resultNodes;
    vector<OutputChunk*> resultContents;
    vector<VarUpdate*> variableGrabs;
    vector<VarUpdate*> variableUpdates;
    wstring compiled;
    vector<map<wstring, int>> varMap;
    Cond* cond;
  };

  multimap<wstring, LemmaTags, Ltstr> cat_items;
  TransferData td;

  void destroy();
  void clearTagIndex();

  void insertCatItem(wstring const &name, wstring const &lemma,
		     wstring const &tags);
  void insertAttrItem(wstring const &name, wstring const &tags);
  void createVar(wstring const &name, wstring const &initial_value);
  void insertListItem(wstring const &name, wstring const &value);

  int insertLemma(int const base, wstring const &lemma);
  int insertTags(int const base, wstring const &tags);

  /**
   * Rules file
   */
  wifstream source;
  string sourceFile;
  
  // for generating error messages
  int currentLine;
  wstring recentlyRead;
  
  void die(wstring message);
  void die(int line, wstring message);

  /**
   * Consume all space characters and comments at front of stream
   */
  void eatSpaces();

  /**
   * Parse an identifier
   * @return identifier
   */
  wstring nextTokenNoSpace();
  
  wstring nextToken(wstring check1, wstring check2);

  /**
   * Parse an identifier
   * @return identifier
   */
  wstring parseIdent(bool prespace);
  int parseInt();
  float parseWeight();
  // return whether the next token is special character c
  // consume it if so
  bool isNextToken(wchar_t c);

  /**
   * Parse an identifier
   * @return identifier
   */
  vector<wstring> parseIdentGroup(wstring first);
  VarUpdate* parseVal();
  Cond* parseCond();

  /**
   * Parse an rule
   */
  void parseRule();

  /**
   * Parse an rule
   */
  void parseOutputRule(vector<wstring> pattern);

  /**
   * Parse an rule
   */
  void parseRetagRule(vector<wstring> srcTags);

  /**
   * Parse an rule
   */
  void parseAttrRule(vector<wstring> name);

  /**
   * Parse an rule
   */
  void parsePatternElement(Rule* rule);

  /**
   * Parse an rule
   */
  void parseOutputElement(Rule* rule, OutputChunk* chunk);
  void parseOutputChunk(Rule* rule, bool recursing);

  /**
   * Parse an rule
   */
  void parseReduceRule(vector<wstring> output, wstring next);

  /**
   * All characters not allowed in identifiers
   */
  static wstring const SPECIAL_CHARS;

  // comparisons with these operator names are done lowercase with '-' and '_' removed
  const vector<pair<wstring, wchar_t>> OPERATORS = {
    make_pair(L"and", AND),
    make_pair(L"&", AND),

    make_pair(L"or", OR),
    make_pair(L"|", OR),

    make_pair(L"equal", EQUAL),
    make_pair(L"=", EQUAL),

    make_pair(L"isprefix", ISPREFIX),
    make_pair(L"startswith", ISPREFIX),
    make_pair(L"beginswith", ISPREFIX),

    make_pair(L"issuffix", ISSUFFIX),
    make_pair(L"endswith", ISSUFFIX),

    make_pair(L"issubstring", ISSUBSTRING),
    make_pair(L"contains", ISSUBSTRING),

    make_pair(L"equalcl", EQUALCL),
    make_pair(L"equalcaseless", EQUALCL),
    make_pair(L"equalfold", EQUALCL),
    make_pair(L"equalfoldcase", EQUALCL),

    make_pair(L"isprefixcl", ISPREFIXCL),
    make_pair(L"startswithcl", ISPREFIXCL),
    make_pair(L"beginswithcl", ISPREFIXCL),
    make_pair(L"isprefixcaseless", ISPREFIXCL),
    make_pair(L"startswithcaseless", ISPREFIXCL),
    make_pair(L"beginswithcaseless", ISPREFIXCL),
    make_pair(L"isprefixfold", ISPREFIXCL),
    make_pair(L"startswithfold", ISPREFIXCL),
    make_pair(L"beginswithfold", ISPREFIXCL),
    make_pair(L"isprefixfoldcase", ISPREFIXCL),
    make_pair(L"startswithfoldcase", ISPREFIXCL),
    make_pair(L"beginswithfoldcase", ISPREFIXCL),

    make_pair(L"issuffixcl", ISSUFFIXCL),
    make_pair(L"endswithcl", ISSUFFIXCL),
    make_pair(L"issuffixcaseless", ISSUFFIXCL),
    make_pair(L"endswithcaseless", ISSUFFIXCL),
    make_pair(L"issuffixfold", ISSUFFIXCL),
    make_pair(L"endswithfold", ISSUFFIXCL),
    make_pair(L"issuffixfoldcase", ISSUFFIXCL),
    make_pair(L"endswithfoldcase", ISSUFFIXCL),

    make_pair(L"issubstringcl", ISSUBSTRINGCL),

    make_pair(L"hasprefix", HASPREFIX),

    make_pair(L"hassuffix", HASSUFFIX),

    make_pair(L"in", IN),
    make_pair(L"∈", IN),

    make_pair(L"hasprefixcl", HASPREFIXCL),

    make_pair(L"hassuffixcl", HASSUFFIXCL),

    make_pair(L"incl", INCL)
  };
  /**
   * Rules file
   */
  map<wstring, vector<wstring>> collections;
  map<wstring, pair<wstring, wstring>> attrDefaults;
  
  /**
   * Rules file
   */
  vector<vector<pair<vector<wstring>, vector<wstring>>>> retagRules;
  
  /**
   * output rules
   */
  vector<pair<vector<wstring>, vector<wstring>>> outputRules;
  
  vector<Rule*> reductionRules;
  
  void processRules();
  void makePattern(int ruleid);
  void makeDefaultRule();
  
  int longestPattern;
  
  wstring compileString(wstring s);
  wstring compileTag(wstring s);
  wstring compileClip(wstring part, int pos, wstring side, bool usereplace);
  wstring processOutput(Rule* rule, ResultNode* r);
  wstring processOutputChunk(Rule* rule, OutputChunk* chunk);
  wstring processCond(Cond* cond);

public:
  static wstring const ANY_TAG;
  static wstring const ANY_CHAR;


  RTXReader();
  ~RTXReader()
  {
  }
  void read(string const &filename);
  void write(string const &patfilename, string const &bytefilename);
};

#endif
