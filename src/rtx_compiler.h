#ifndef __RTXCOMPILER__
#define __RTXCOMPILER__

#include <apertium/transfer_data.h>
#include <bytecode.h>

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

class RTXCompiler
{
private:

  //////////
  // TYPES
  //////////

  struct Clip
  {
    int src;
    wstring part;
    wstring side;
  };

  struct Cond
  {
    wchar_t op;
    Clip* val;
    Cond* left;
    Cond* right;
  };

  struct OutputChunk
  {
    wstring mode;
    unsigned int pos;
    wstring lemma;
    vector<wstring> tags;
    bool getall;
    map<wstring, Clip*> vars;
    wstring pattern;
    vector<OutputChunk*> children;
    bool isToplevel;
  };

  struct Rule
  {
    int line;
    int grab_all;
    float weight;
    vector<vector<wstring>> pattern;
    vector<pair<OutputChunk*, Cond*>> output;
    map<wstring, Clip*> vars;
    wstring compiled;
    Cond* cond;
  };

  //////////
  // COLLECTIONS AND DATA STRUCTURES
  //////////

  /**
   * All characters not allowed in identifiers
   */
  static wstring const SPECIAL_CHARS;

  static wstring const ANY_TAG;
  static wstring const ANY_CHAR;

  /**
   * Pattern-file generator
   */
  TransferData td;

  /**
   * Map of names to attribute lists
   */
  map<wstring, vector<wstring>> collections;

  /**
   * Map of attribute names to default and replacement values
   * First value of pair is value to return if the attribute is not found
   * Second value is value to overwrite it with if it's still there at output
   */
  map<wstring, pair<wstring, wstring>> attrDefaults;

  /**
   * Map of attribute names to values that should never be modified
   * Note: This is not currently used
   */
  map<wstring, vector<wstring>> noOverwrite;
  
  /**
   * List of tag-replacement rules
   * Each list begins with pair<old attribute name, new attribute name>
   * Followed by some number of pair<old attribute value, new attribute value>
   * Note: This is not currently used
   */
  vector<vector<pair<wstring, wstring>>> retagRules;

  /**
   * Map key => [ value ]
   * Where key and value both name attribute lists
   * Where for each value, there is a tag-replacement rule from value to key
   */
  map<wstring, vector<wstring>> altAttrs;
  
  /**
   * Map of pattern names to output patterns
   * Where '_' represents "lemh" and the part of speech tag
   * (which is usually the pattern name)
   * "lemq" is automatically appended to the end
   */
  map<wstring, vector<wstring>> outputRules;

  /**
   * Map of pattern names to booleans
   * false indicates that the node on the left side of rules
   * and thus all clips should be target clips
   * true indicates both surface only and unspecified
   */
  map<wstring, bool> nodeIsSurface;

  /**
   * List of all reduction rules in the order they were parsed
   */
  vector<Rule*> reductionRules;

  /**
   * List of compiled forms of output-time rules
   * in the order they were generated
   */
  vector<wstring> outputBytecode;

  /**
   * Either the current rule being parsed or the current rule being compiled
   */
  Rule* currentRule;

  /**
   * Either the current chunk being parsed or the current chunk being compiled
   */
  OutputChunk* currentChunk;

  /**
   * All attributes which can be clipped from the chunk whose children
   * are currently being compiled
   */
  vector<wstring> parentTags;

  /**
   * If true, then the current compilation is for an output-time rule
   */
  bool inOutputRule;

  /**
   * The length of the longest left side of a rule
   */
  unsigned int longestPattern;

  /**
   * Input stream
   */
  wifstream source;

  //////////
  // ERROR REPORTING
  //////////

  // for generating error messages
  int currentLine;
  wstring recentlyRead;
  bool errorsAreSyntax;
  string sourceFile;

  /**
   * Report an error in the input file and exit
   * if errorsAreSyntax == true, will also print the most recently read line
   * with a marker of the approximate location of the error
   */
  void die(wstring message);

  //////////
  // TOKENIZATION
  //////////

  // All functions in this section must update currentLine and recentlyRead

  /**
   * Consume all space characters and comments at front of source
   */
  void eatSpaces();

  /**
   * Parse the next token
   * Report a syntax error if it is preceded by spaces
   * @return token
   */
  wstring nextTokenNoSpace();

  /**
   * Parse the next token
   * Calls eatSpaces() beforehand
   * If check1 or check2 is supplied and the token not equal to one of them
   * report a syntax error
   * @return token
   */
  wstring nextToken(wstring check1, wstring check2);

  /**
   * Parse an identifier
   * Calls eatSpaces() beforehand if prespace == true
   * @return identifier
   */
  wstring parseIdent(bool prespace);

  /**
   * Parse an integer
   * @return integer
   */
  unsigned int parseInt();

  /**
   * Parse a positive floating point number followed by a colon
   * @return float
   */
  float parseWeight();

  /**
   * If the next character in the input stream is c, consume it and return true
   * Otherwise return false
   */
  bool isNextToken(wchar_t c);

  //////////
  // COMPONENT PARSING
  //////////

  // Functions in this section may not access the input stream directly
  // in order to ensure error messages have the right contents

  /**
   * Parse an value specifier
   * If src == -2, checks for position specifiers or $
   * Parses identifier and possible clip side
   * @return Clip pointer
   */
  Clip* parseClip(int src);

  /**
   * Parse condition
   * Assumes that condition being parsed is contained in parentheses
   * Looks up operator names in OPERATORS after removing '-' and '_'
   * and converting to lower case
   * Note that negation can currently only be written as '~'
   * @return Cond pointer
   */
  Cond* parseCond();

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
   * Parse an element of the left side of a rule
   * @param rule - the rule object to attach the result to
   */
  void parsePatternElement(Rule* rule);

  /**
   * Parse a non-chunk element of the right side of a rule
   * The result is appended to currentChunk
   */
  void parseOutputElement();

  /**
   * Parse the right side of a rule
   * @param recursing - whether the chunk to be parsed is inside another chunk
   */
  void parseOutputChunk(bool recursing);

  //////////
  // RULE PARSING
  //////////

  /**
   * Reads initial two tokens of a rule and calls appropriate function
   */
  void parseRule();

  /**
   * Parse a tag-order rule
   */
  void parseOutputRule(wstring pattern);

  /**
   * Parse a tag-replacement rule
   * Note: these rules currently have no effect
   */
  void parseRetagRule(wstring srcTag);

  /**
   * Parse an attribute category
   */
  void parseAttrRule(wstring name);

  /**
   * Parse a reduction rule and append it to reductionRules
   */
  void parseReduceRule(wstring firstnode, wstring next);

  //////////
  // COMPILATION
  //////////

  /**
   * Adds a lemma to the pattern transducer
   * @param base - starting point of insertion
   * @param lemma - lemma to insert
   * @return end state of insertion
   */
  int insertLemma(int const base, wstring const &lemma);

  /**
   * Adds tags to the pattern transducer
   * Note: Appends any_tag* to the end of the pattern
   * @param base - starting point of insertion
   * @param tags - tags to insert
   * @return end state of insertion
   */
  int insertTags(int const base, wstring const &tags);

  /**
   * Construct the appropriate transducer path for a rule
   * @param ruleid - index of rule in reductionRules
   */
  void makePattern(int ruleid);

  /**
   * Compiles a string literal
   * @param s - the string
   * @return bytecode
   */
  wstring compileString(wstring s);

  /**
   * Compiles a string as to a literal tag
   * @param s - the tag
   * @return bytecode
   */
  wstring compileTag(wstring s);

  /**
   * Compile a Clip object
   * @param c - the clip
   * @return bytecode
   */
  wstring compileClip(Clip* c);

  /**
   * Wrapper around compileClip(Clip*)
   */
  wstring compileClip(wstring part, int pos, wstring side);

  /**
   * Compile a non-chunk output element
   * @param ch - the element
   * @return bytecode
   */
  wstring processOutput(OutputChunk* ch);

  /**
   * Compile an output chunk and potentially generate output rule
   * @param chunk - the chunk
   * @return bytecode
   */
  wstring processOutputChunk(OutputChunk* chunk);

  /**
   * Compile a Cond object
   * @param cond - the conditional
   * @return bytecode
   */
  wstring processCond(Cond* cond);

  /**
   * Iterate over reductionRules, compiling them
   * Stores compiled form in Rule->compiled
   */
  void processRules();

public:
  RTXCompiler();
  ~RTXCompiler() {}
  void read(string const &filename);
  void write(string const &patfilename, string const &bytefilename);
};

#endif
