#ifndef _RTXPARSER_
#define _RTXPARSER_

#include <iostream>

using namespace std;

class Parser
{
private:
    /**
     * Rules file
     */
    istream source;

    /**
     * Consume all space characters and comments at front of stream
     */
    void eatSpaces();

    /**
     * Parse an identifier
     * @return identifier
     */
    wstring parseIdent();

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
    void parseAttrRule(vector<string> name);

    /**
     * Parse an rule
     */
    void parseReduceRule(vector<string> output, bool isSingle);

    /**
     * All characters not allowed in identifiers
     */
    static wstring const SPECIAL_CHARS;
public:
    void parse(string fname);
};

#endif
