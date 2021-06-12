#ifndef __RTXCOMPILER__
#define __RTXCOMPILER__

#include <rtx_config.h>
#include <pattern.h>
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

  struct OutputChoice;

  enum ClipSource
  {
    // positive values refer to input nodes
    ConstantClip  = 0,
    ParentClip    = -1,
    ConditionClip = -2,
    StringVarClip = -3,
    ChunkVarClip  = -4
  };

  struct Clip
  {
    int src;
    UString part;
    UString side;
    vector<UString> rewrite;
    OutputChoice* choice;
    UString varName;
  };

  struct Cond
  {
    UChar op;
    Clip* val;
    Cond* left;
    Cond* right;
  };

  struct OutputChunk
  {
    UString mode;
    unsigned int pos;
    UString lemma;
    vector<UString> tags;
    bool getall;
    map<UString, Clip*> vars;
    UString pattern;
    vector<OutputChoice*> children;
    bool conjoined;
    bool interpolated;
    bool nextConjoined;
  };

  struct OutputChoice
  {
    vector<Cond*> conds;
    vector<OutputChoice*> nest;
    vector<OutputChunk*> chunks;
    vector<Clip*> clips;
  };

  struct Rule
  {
    int line;
    int grab_all;
    float weight;
    UString name;
    vector<vector<UString>> pattern;
    vector<OutputChoice*> output;
    vector<OutputChoice*> output_sl;
    vector<OutputChoice*> output_ref;
    map<UString, Clip*> vars;
    map<UString, OutputChoice*> globals;
    map<UString, Clip*> stringGlobals;
    vector<UString> result;
    UString compiled;
    Cond* cond;
  };

  enum Location
  {
    LocTopLevel,
    LocClip,
    LocChunk,
    LocVarSet
  };

  enum LocationType
  {
    LocTypeNone,
    LocTypeInput,
    LocTypeOutput,
    LocTypeMacro
  };

  //////////
  // SETTINGS
  //////////

  /**
   * If true, write the output and pattern of each rule to stderr
   * Default: false
   */
  bool summarizing;

  /**
   * Names of rules that should be excluded from the pattern transducer
   */
  set<UString> excluded;

  //////////
  // COLLECTIONS AND DATA STRUCTURES
  //////////

  /**
   * All characters not allowed in identifiers
   */
  static UString const SPECIAL_CHARS;

  static UString const ANY_TAG;
  static UString const ANY_CHAR;

  /**
   * Pattern-file generator
   */
  PatternBuilder PB;

  /**
   * Map of names to attribute lists
   */
  map<UString, vector<UString>> collections;

  /**
   * Map of attribute names to default and replacement values
   * First value of pair is value to return if the attribute is not found
   * Second value is value to overwrite it with if it's still there at output
   */
  map<UString, pair<UString, UString>> attrDefaults;

  /**
   * Map of attribute names to values that should never be modified
   * Note: This is not currently used
   */
  map<UString, vector<UString>> noOverwrite;
  
  /**
   * List of tag-replacement rules
   * Each list begins with pair<old attribute name, new attribute name>
   * Followed by some number of pair<old attribute value, new attribute value>
   * Note: This is not currently used
   */
  vector<vector<pair<UString, UString>>> retagRules;

  /**
   * Map key => [ value ]
   * Where key and value both name attribute lists
   * Where for each value, there is a tag-replacement rule from value to key
   */
  map<UString, vector<UString>> altAttrs;
  
  /**
   * Map of pattern names to output patterns
   * Where '_' represents "lemh" and the part of speech tag
   * (which is usually the pattern name)
   * "lemq" is automatically appended to the end
   * If the contents of the vector is "macro"_u, look at macros
   */
  map<UString, vector<UString>> outputRules;

  /**
   * Map of pattern names to conditioned output patterns
   */
  map<UString, OutputChoice*> macros;

  /**
   * Names of global chunk-type variables and corresponding indecies
   */
  map<UString, unsigned int> globalVarNames;

  /**
   * Map of pattern names to booleans
   * false indicates that the node on the left side of rules
   * and thus all clips should be target clips
   * true indicates both surface only and unspecified
   */
  map<UString, bool> nodeIsSurface;

  /**
   * List of all reduction rules in the order they were parsed
   */
  vector<Rule*> reductionRules;

  /**
   * List of compiled forms of output-time rules
   * in the order they were generated
   */
  vector<UString> outputBytecode;

  /**
   * Either the current rule being parsed or the current rule being compiled
   */
  Rule* currentRule;

  /**
   * Either the current chunk being parsed or the current chunk being compiled
   */
  OutputChunk* currentChunk;

  /**
   * Either the current if statement being parsed or the current if statement being compiled
   */
  OutputChoice* currentChoice;

  /**
   * The Clip currently being parsed
   */
  Clip* currentClip;

  /**
   * Global variable currently being assigned to
   */
  unsigned int currentVar;

  /**
   * Which surface of a chunk is being assigned to
   * one of APPENDSURFACE, APPENDSURFACESL, APPENDSURFACEREF
   */
  UChar currentSurface;

  /**
   * All attributes which can be clipped from the chunk whose children
   * are currently being compiled
   */
  vector<UString> parentTags;

  /**
   * Current construct being parsed or compiled
   */
  Location currentLoc;

  /**
   * Current top-level construct
   */
  LocationType currentLocType;

  /**
   * The length of the longest left side of a rule
   */
  unsigned int longestPattern;

  /**
   * Input stream
   */
  ifstream source;

  //////////
  // ERROR REPORTING
  //////////

  // for generating error messages
  int currentLine;
  UString recentlyRead;
  UString unreadbuf;
  int unreadmark;
  bool errorsAreSyntax;
  string sourceFile;
  vector<UString> macroNameStack;

  /**
   * Report an error in the input file and exit
   * if errorsAreSyntax == true, will also print the most recently read line
   * with a marker of the approximate location of the error
   */
  void die(UString message);

  //////////
  // TOKENIZATION
  //////////

  /**
   * Read and return one character from the input stream
   * Other functions should call this rather than source.get()
   * to ensure that recentlyRead gets updated properly
   * @return character
   */
  UChar getchar();

  /**
   * Return the next character in the input stream without reading
   * Other functions should call this rather than source.peek()
   * in order to properly manage unreadbuf
   * @ return character
   */
  UChar peekchar();

  /**
   * Mark the current location so that it can be jumped back to with unread()
   */
  void setUnreadMark();

  /**
   * Move everything read since last call to setUnreadMark() into unreadbuf
   */
  void unread();

  /**
   * Consume all space characters and comments at front of source
   */
  void eatSpaces();

  /**
   * Parse the next token
   * Report a syntax error if it is preceded by spaces
   * @return token
   */
  UString nextTokenNoSpace();

  /**
   * Parse the next token
   * Calls eatSpaces() beforehand
   * If check1 or check2 is supplied and the token not equal to one of them
   * report a syntax error
   * @return token
   */
  UString nextToken(UString check1, UString check2);

  /**
   * Parse an identifier
   * Calls eatSpaces() beforehand if prespace == true
   * @return identifier
   */
  UString parseIdent(bool prespace);

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
  bool isNextToken(UChar c);

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

  /**
   * Convert a string to an operator
   * @param op - the string from the rule
   * @return bytecode for corresponding operation or L'\0' if not found
   */
  UChar lookupOperator(UString op);

  const vector<pair<UString, UChar>> OPERATORS = {
    make_pair("and"_u, AND),
    make_pair("&"_u, AND),

    make_pair("or"_u, OR),
    make_pair("|"_u, OR),

    make_pair("not"_u, NOT),
    make_pair("~"_u, NOT),
    make_pair("⌐"_u, NOT),

    make_pair("equal"_u, EQUAL),
    make_pair("="_u, EQUAL),

    make_pair("isprefix"_u, ISPREFIX),
    make_pair("startswith"_u, ISPREFIX),
    make_pair("beginswith"_u, ISPREFIX),

    make_pair("issuffix"_u, ISSUFFIX),
    make_pair("endswith"_u, ISSUFFIX),

    make_pair("issubstring"_u, ISSUBSTRING),
    make_pair("contains"_u, ISSUBSTRING),

    make_pair("equalcl"_u, EQUALCL),
    make_pair("equalcaseless"_u, EQUALCL),
    make_pair("equalfold"_u, EQUALCL),
    make_pair("equalfoldcase"_u, EQUALCL),

    make_pair("isprefixcl"_u, ISPREFIXCL),
    make_pair("startswithcl"_u, ISPREFIXCL),
    make_pair("beginswithcl"_u, ISPREFIXCL),
    make_pair("isprefixcaseless"_u, ISPREFIXCL),
    make_pair("startswithcaseless"_u, ISPREFIXCL),
    make_pair("beginswithcaseless"_u, ISPREFIXCL),
    make_pair("isprefixfold"_u, ISPREFIXCL),
    make_pair("startswithfold"_u, ISPREFIXCL),
    make_pair("beginswithfold"_u, ISPREFIXCL),
    make_pair("isprefixfoldcase"_u, ISPREFIXCL),
    make_pair("startswithfoldcase"_u, ISPREFIXCL),
    make_pair("beginswithfoldcase"_u, ISPREFIXCL),

    make_pair("issuffixcl"_u, ISSUFFIXCL),
    make_pair("endswithcl"_u, ISSUFFIXCL),
    make_pair("issuffixcaseless"_u, ISSUFFIXCL),
    make_pair("endswithcaseless"_u, ISSUFFIXCL),
    make_pair("issuffixfold"_u, ISSUFFIXCL),
    make_pair("endswithfold"_u, ISSUFFIXCL),
    make_pair("issuffixfoldcase"_u, ISSUFFIXCL),
    make_pair("endswithfoldcase"_u, ISSUFFIXCL),

    make_pair("issubstringcl"_u, ISSUBSTRINGCL),
    make_pair("issubstringcaseless"_u, ISSUBSTRINGCL),
    make_pair("issubstringfold"_u, ISSUBSTRINGCL),
    make_pair("issubstringfoldcase"_u, ISSUBSTRINGCL),

    make_pair("hasprefix"_u, HASPREFIX),
    make_pair("startswithlist"_u, HASPREFIX),
    make_pair("beginswithlist"_u, HASPREFIX),

    make_pair("hassuffix"_u, HASSUFFIX),
    make_pair("endswithlist"_u, HASSUFFIX),

    make_pair("in"_u, IN),
    make_pair("∈"_u, IN),

    make_pair("hasprefixcl"_u, HASPREFIXCL),
    make_pair("startswithlistcl"_u, HASPREFIXCL),
    make_pair("beginswithlistcl"_u, HASPREFIXCL),
    make_pair("hasprefixcaseless"_u, HASPREFIXCL),
    make_pair("startswithlistcaseless"_u, HASPREFIXCL),
    make_pair("beginswithlistcaseless"_u, HASPREFIXCL),
    make_pair("hasprefixfold"_u, HASPREFIXCL),
    make_pair("startswithlistfold"_u, HASPREFIXCL),
    make_pair("beginswithlistfold"_u, HASPREFIXCL),
    make_pair("hasprefixfoldcase"_u, HASPREFIXCL),
    make_pair("startswithlistfoldcase"_u, HASPREFIXCL),
    make_pair("beginswithlistfoldcase"_u, HASPREFIXCL),

    make_pair("hassuffixcl"_u, HASSUFFIXCL),
    make_pair("endswithlistcl"_u, HASSUFFIXCL),
    make_pair("hassuffixcaseless"_u, HASSUFFIXCL),
    make_pair("endswithlistcaseless"_u, HASSUFFIXCL),
    make_pair("hassuffixfold"_u, HASSUFFIXCL),
    make_pair("endswithlistfold"_u, HASSUFFIXCL),
    make_pair("hassuffixfoldcase"_u, HASSUFFIXCL),
    make_pair("endswithlistfoldcase"_u, HASSUFFIXCL),

    make_pair("incl"_u, INCL),
    make_pair("∈cl"_u, INCL), // why you would want to use ∈ here I'm not sure
    make_pair("incaseless"_u, INCL),
    make_pair("∈caseless"_u, INCL), // but the documentation implies they exist
    make_pair("infold"_u, INCL),
    make_pair("∈fold"_u, INCL), // so here they are
    make_pair("infoldcase"_u, INCL),
    make_pair("∈foldcase"_u, INCL)
  };

  /**
   * Parse an element of the left side of a rule
   * @param rule - the rule object to attach the result to
   */
  void parsePatternElement(Rule* rule);

  /**
   * Wrap an OutputChunk in an OutputChoice with an empty condition
   */
  OutputChoice* chunkToCond(OutputChunk* ch);

  /**
   * Parse a non-chunk element of the right side of a rule
   */
  OutputChunk* parseOutputElement();

  /**
   * Parse the right side of a rule
   */
  OutputChunk* parseOutputChunk();

  /**
   * Parse an if statement on the right side of a rule
   */
  OutputChoice* parseOutputCond();

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
  void parseOutputRule(UString pattern);

  /**
   * Parse a tag-replacement rule
   * Note: these rules currently have no effect
   */
  void parseRetagRule(UString srcTag);

  /**
   * Parse an attribute category
   */
  void parseAttrRule(UString name);

  /**
   * Parse a reduction rule and append it to reductionRules
   */
  void parseReduceRule(UString firstnode, UString next);

  //////////
  // ANALYSIS
  //////////

  /**
   * Checks for possible mistakes in tag-rewrite rules and issues warnings
   */
  void processRetagRules();

  //////////
  // COMPILATION
  //////////

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
  UString compileString(UString s);

  /**
   * Compiles a string as to a literal tag
   * @param s - the tag
   * @return bytecode
   */
  UString compileTag(UString s);

  /**
   * Compile a Clip object
   * If a destination attribute is provided, the function will check for
   * applicable tag-rewrite rules
   * @param c - the clip
   * @param dest - the destination attribute
   * @return bytecode
   */
  UString compileClip(Clip* c, UString dest);

  /**
   * Wrapper around compileClip(Clip*)
   */
  UString compileClip(UString part, int pos, UString side);

  // TODO
  Clip* processMacroClip(Clip* mac, OutputChunk* arg);
  Cond* processMacroCond(Cond* mac, OutputChunk* arg);
  OutputChunk* processMacroChunk(OutputChunk* mac, OutputChunk* arg);
  OutputChoice* processMacroChoice(OutputChoice* mac, OutputChunk* arg);

  /**
   * Compile a non-chunk output element
   * @param ch - the element
   * @return bytecode
   */
  UString processOutputChunk(OutputChunk* ch);

  /**
   * Compile and the output rule for a chunk
   * @param chunk - the chunk
   * @return bytecode
   */
  UString processOutput(OutputChunk* chunk);

  /**
   * Compile the output rule for an if statement
   * @param chunk - the chunk
   * @return bytecode
   */
  UString processOutputChoice(OutputChoice* choice);

  /**
   * Compile a Cond object
   * @param cond - the conditional
   * @return bytecode
   */
  UString processCond(Cond* cond);

  /**
   * Iterate over reductionRules, compiling them
   * Stores compiled form in Rule->compiled
   */
  void processRules();

public:
  RTXCompiler();
  ~RTXCompiler() {}

  void setSummarizing(bool value)
  {
    summarizing = value;
  }
  void excludeRule(UString name)
  {
    excluded.insert(name);
  }
  void loadLex(const string& filename);

  void read(string const &filename);
  void write(string const &filename);

  void printStats();
};

#endif
