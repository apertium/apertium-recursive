#ifndef _RTXFILEPARSER_
#define _RTXFILEPARSER_

#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <reduction_rule.h>

using namespace std;

class Parser
{
private:
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
    void parsePatternElement(ReductionRule* rule);

    /**
     * Parse an rule
     */
    void parseOutputElement(ReductionRule* rule);

    /**
     * Parse an rule
     */
    void parseReduceRule(vector<wstring> output, wstring next);

    /**
     * All characters not allowed in identifiers
     */
    static wstring const SPECIAL_CHARS;
public:
    /**
     * Rules file
     */
    map<wstring, vector<wstring>> attributeRules;
    map<wstring, bool> allAttributes;
    
    /**
     * Rules file
     */
    vector<vector<pair<vector<wstring>, vector<wstring>>>> retagRules;
    
    /**
     * output rules
     */
    vector<pair<vector<wstring>, vector<wstring>>> outputRules;
    
    vector<ReductionRule*> reductionRules;
    
    Parser();
    ~Parser();
    void parse(string fname);
};

#endif
