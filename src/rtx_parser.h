#ifndef _RTXPARSER_
#define _RTXPARSER_

#include <iostream>
#include <vector>
#include <fstream>

using namespace std;

class Parser
{
private:
    /**
     * Rules file
     */
    wifstream source;

    /**
     * Consume all space characters and comments at front of stream
     */
    void eatSpaces();

    /**
     * Parse an identifier
     * @return identifier
     */
    wstring nextToken();

    /**
     * Parse an identifier
     * @return identifier
     */
    wstring parseIdent();

    /**
     * Parse an identifier
     * @return identifier
     */
    vector<wstring> parseIdentGroup();

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
    void parseRetagRule(vector<wstring> source);

    /**
     * Parse an rule
     */
    void parseAttrRule(vector<wstring> name);

    /**
     * Parse an rule
     */
    void parseReduceRule(vector<wstring> output, bool isSingle);

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
