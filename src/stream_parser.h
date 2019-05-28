#ifndef _RTXSTREAMPARSER_
#define _RTXSTREAMPARSER_

#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <parser_node.h>

using namespace std;

class StreamParser
{
private:
    wistream* source;
    map<wstring, vector<wstring>> attributes;
    void readLU(ParserNode* node, int pos);

public:
    StreamParser(wistream* input, map<wstring, vector<wstring>> attributes);
    ~StreamParser();
    ParserNode* nextToken();
};

#endif
