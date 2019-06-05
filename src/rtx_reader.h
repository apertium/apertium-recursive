#ifndef _RTXREADER_
#define _RTXREADER_

#include <apertium/transfer_data.h>
#include <lttoolbox/ltstr.h>

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
  
  struct Rule
  {
    int ID;
    int grab_all;
    float weight;
    int patternLength;
    vector<vector<wstring>> pattern;
    vector<wstring> resultNodes;
    vector<vector<wstring>> resultVars;
    vector<vector<wstring>> resultContents;
    vector<vector<pair<int, wstring>>> variableGrabs;
    vector<pair<pair<int, wstring>, pair<int, wstring>>> variableUpdates;
    wstring compiled;
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
    
    int currentLine;
    
    void die(wstring message);

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
    wstring parseIdent();

    /**
     * Parse an identifier
     * @return identifier
     */
    vector<wstring> parseIdentGroup(wstring first);

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
    void parseOutputElement(Rule* rule);

    /**
     * Parse an rule
     */
    void parseReduceRule(vector<wstring> output, wstring next);

    /**
     * All characters not allowed in identifiers
     */
    static wstring const SPECIAL_CHARS;
    /**
     * Rules file
     */
    map<wstring, vector<wstring>> collections;
    set<wstring> attrs;
    set<wstring> lemmas;
    set<wstring> lists;
    map<wstring, bool> allAttributes;
    
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

public:
  static wstring const ANY_TAG;
  static wstring const ANY_CHAR;


  RTXReader();
  ~RTXReader()
  {
  }
  void read(string const &filename);
  void write(string const &filename);
};

#endif
