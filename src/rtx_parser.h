#ifndef _RTXPARSER_
#define _RTXPARSER_

#include <iostream>
#include <vector>
#include <fstream>
#include <map>

using namespace std;

class Parser
{
private:
    /**
     * Rules file
     */
    wifstream source;
    string sourceFile;

    /**
     * Rules file
     */
    map<wstring, vector<wstring>> attributeRules;
    
    /**
     * Rules file
     */
    vector<vector<pair<vector<wstring>, vector<wstring>>>> retagRules;
    
    /**
     * output rules
     */
    vector<pair<vector<wstring>, vector<wstring>>> outputRules;
    
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
    wstring nextToken(wstring check);

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
    void parseReduceRule(vector<wstring> output, wstring next);

    /**
     * All characters not allowed in identifiers
     */
    static wstring const SPECIAL_CHARS;
public:
    Parser();
    ~Parser();
    void parse(string fname);
};

#endif
