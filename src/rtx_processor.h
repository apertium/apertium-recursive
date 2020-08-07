#ifndef __RTXPROCESSOR__
#define __RTXPROCESSOR__

#include <rtx_config.h>
#include <apertium_re.h>
#include <apertium/utf_converter.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/buffer.h>
#include <lttoolbox/ltstr.h>
#include <matcher.h>
#include <chunk.h>
#include <pool.h>

#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

using namespace std;

struct StackElement
{
  int mode;
  bool b;
  int i;
  wstring s;
  Chunk* c;
};

class RTXProcessor
{
private:

  //////////
  // DATA
  //////////

  /**
   * Alphabet instance for the pattern transducer
   */
  Alphabet alphabet;

  /**
   * The pattern transducer
   */
  MatchExe2 *mx;

  /**
   * Attribute category regular expressions
   */
  map<wstring, ApertiumRE, Ltstr> attr_items;

  /**
   * Virtual machine global variables
   * name => value
   */
  map<wstring, wstring, Ltstr> variables;

  /**
   * Lists
   * name => { values }
   */
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;

  /**
   * Lists, but all values are converted to lower case
   * Used for case-insensitive comparison
   * name => { values }
   */
  map<wstring, set<wstring, Ltstr>, Ltstr> listslow;

  /**
   * Bytecode for input-time rules
   */
  vector<wstring> rule_map;

  /**
   * Bytecode for output-time rules
   */
  vector<wstring> output_rules;

  /**
   * Debug names for input-time rules (may be empty)
   */
  vector<wstring> inRuleNames;

  /**
   * Debug names for output-time rules (may be empty)
   */
  vector<wstring> outRuleNames;

  /**
   * Length of pattern of each input-time rule, including blanks
   */
  vector<int> pat_size;

  /**
   * Maximum value of pat_size
   */
  unsigned int longestPattern;

  /**
   * Number of Chunk* global variables
   */
  unsigned int varCount;

  /**
   * false if EOF or \0 has been reached in the input stream, true otherwise
   */
  bool furtherInput;

  /**
   * The stack used by the virtual machine
   * The stack size is set to 32 because a rule would only go higher than
   * roughly 10 if it was evaluating a very complex conditional,
   * so 32 is probably more than anyone will ever need
   */
  StackElement theStack[32];

  /**
   * Index of the top element on theStack
   */
  int stackIdx;

  /**
   * Input to the virtual machine
   * currentInput is global rather than a parameter mostly for symmetry
   * with currentOutput
   */
  vector<Chunk*> currentInput;

  /**
   * Output from the virtual machine
   * The return value of the virtual machine is whether or not the rule was
   * rejected, and passing data by semi-global array saves some amount of
   * allocating and copying
   */
  vector<Chunk*> currentOutput;

  /**
   * Chunk containing currentInput for output-time rules
   */
  Chunk* parentChunk;

  /**
   * Chunks waiting to be written to output stream
   */
  list<Chunk*> outputQueue;

  /**
   * The parse stack
   * Each element represents one possible partial or complete parse
   * of the input read in since the last flush
   */
  vector<ParseNode*> parseGraph;

  /**
   * Pool allocator for Chunks, freed on output flush
   */
  Pool<Chunk> chunkPool;

  /**
   * Pool allocator for ParseNodes, freed on output flush
   */
  Pool<ParseNode> parsePool;

  /**
   * The next few tokens in the input stream (usually 5)
   */
  list<Chunk*> inputBuffer;

  /**
   * Pseudo-input buffer used by checkForReduce()
   * Rules that output a single node are processed immediately
   * but when rules output multiple nodes, treating them as input
   * necessitates that reductions could happen, so any output past the first
   * node is pushed onto currentContinuation, with the next node to be
   * processed being currentContinuation.back()->back()
   */
  vector<vector<Chunk*>*> currentContinuation;

  /**
   * Branch of parseGraph currently being operated on
   * Needed by applyRule() for FETCHCHUNK and SETCHUNK
   */
  ParseNode* currentBranch;

  //////////
  // SETTINGS
  //////////

  /**
   * true if the next input token should be parsed as an LU, false otherwise
   * Initial value: false
   */
  bool inword;
  
  /**
   * true if the next input token should be parsed as a wordbound blank, false otherwise
   * Initial value: false
   */
  bool inwblank;

  /**
   * Whether output should flush on \0
   * Default: false
   */
  bool null_flush;

  /**
   * If true, each instruction of virtual machine will be printed to wcerr
   * Default: false
   */
  bool printingSteps;

  /**
   * If true, each rule that is applied will be printed to wcerr
   * Default: false
   */
  bool printingRules;

  /**
   * If true, each action of filterParseGraph() will be logged to wcerr
   * Default: false
   */
  bool printingBranches;

  /**
   * If true, produce a full report, similar to (printingRules && printingBranches)
   * Affected by treePrintMode
   * Default: false
   */
  bool printingAll;

  /**
   * false if input comes from apertium-anaphora, true otherwise
   * Default: true
   */
  bool noCoref;

  /**
   * true if rule application should mimic the chunker-interchunk-postchunk
   * pipeline, false otherwise
   * Default: false
   */
  bool isLinear;

  /**
   * If true, parse tree will be printed according to treePrintMode
   * before output-time rules are applied
   * Default: false
   */
  bool printingTrees;

  /**
   * If false, output-time rules will not be applied and linear output
   * will not be produced
   * Default: true
   */
  bool printingText;

  /**
   * Manner in which to print trees
   * Set by setOutputMode()
   * Enum defined in chunk.h
   * Default: TreeModeNest
   */
  TreeMode treePrintMode;

  /**
   * Counter used to give distinct, consistent identifiers to ParseNodes
   * for tracing purposes
   */
  int newBranchId;

  /**
   * If this is set to true, filterParseGraph() will only discard branches
   * on parse error
   */
  bool noFilter;

  //////////
  // VIRTUAL MACHINE
  //////////

  /**
   * Determine capitalization of a string
   * @param str - input string
   * @return L"AA", L"Aa", or L"aa"
   */
  wstring caseOf(wstring const &str);

  /**
   * Produce a version of target_word with the case of source_word
   * @param source_word - source of case
   * @param target_word - source of content
   * @return generated string
   */
  wstring copycase(wstring const &source_word, wstring const &target_word);

  /**
   * Return whether str1 begins with str2
   */
  bool beginsWith(wstring const &str1, wstring const &str2) const;

  /**
   * Return whether str1 ends with str2
   */
  bool endsWith(wstring const &str1, wstring const &str2) const;

  /**
   * The virtual machine
   * Modifies: theStack, stackIdx, variables, currentOutput
   * Reads from: currentInput, parentChunk
   * @param rule - bytecode for rule to be applied
   * @return false if REJECTRULE was executed, true otherwise
   */
  bool applyRule(const wstring& rule);

  /**
   * Pop and return a boolean from theStack
   * Log error and call exit(1) if top element is not a bool
   */
  bool popBool();

  /**
   * Pop and return an integer from theStack
   * Log error and call exit(1) if top element is not an int
   */
  int popInt();

  /**
   * Pop and return a wstring from theStack
   * Log error and call exit(1) if top element is not a wstring
   */
  wstring popString();

  /**
   * Equivalent to popString(), but with called as
   * wstring x; popString(x);
   * rather than
   * wstring x = popString();
   * This uses a swap to save an allocation and a copy, which is almost twice
   * as fast, which has a noticeable impact on overall speed
   */
  void popString(wstring& dest);

  /**
   * Pop and return a Chunk pointer from theStack
   * Log error and call exit(1) if top element is not a Chunk*
   */
  Chunk* popChunk();

  inline void pushStack(bool b)
  {
    theStack[++stackIdx].mode = 0;
    theStack[stackIdx].b = b;
  }
  inline void pushStack(int i)
  {
    theStack[++stackIdx].mode = 1;
    theStack[stackIdx].i = i;
  }
  inline void pushStack(const wstring& s)
  {
    theStack[++stackIdx].mode = 2;
    theStack[stackIdx].s.assign(s);
  }
  inline void pushStack(Chunk* c)
  {
    theStack[++stackIdx].mode = 3;
    theStack[stackIdx].c = c;
  }

  /**
   * Copy the contents of theStack[src] to theStack[dest]
   */
  void stackCopy(int src, int dest);

  //////////
  // RULE SELECTION AND I/O
  //////////

  /**
   * Read an LU or a blank
   * Modifies: furtherInput
   * @param in - input stream
   * @return pointer to token read
   */
  Chunk* readToken(FILE *in);

  bool lookahead(ParseNode* node);

  /**
   * Check whether any rules can apply to node
   * if there are any, select one and apply it
   * if not or there is a shift-reduce conflict, fork
   * append resulting node(s) to result
   */
  void checkForReduce(vector<ParseNode*>& result, ParseNode* node);

  /**
   * Apply output-time rules and write nodes to output stream
   * @param out - output stream
   */
  void outputAll(FILE* out);

  /**
   * Prune any ParseNodes that have reached error states
   * Modifies: parseGraph
   * @return true if outputAll should be called
   */
  bool filterParseGraph();

  /**
   * Process input as a GLR parser
   * Read input, call checkForReduce(), call filterParseGraph(), call outputAll()
   */
  void processGLR(FILE* in, FILE* out);

  /**
   * Apply longest rule matching the beginning of t1x and append the result to t2x
   */
  void processTRXLayer(list<Chunk*>& t1x, list<Chunk*>& t2x);

  /**
   * Mimic apertium-transfer | apertium-interchunk | apertium-postchunk
   * Read input, call processTRXLayer twice, apply output-time rules, output
   */
  void processTRX(FILE* in, FILE* out);
  
public:
  RTXProcessor();
  ~RTXProcessor();

  void read(string const &filename);
  void process(FILE *in, FILE *out);
  bool getNullFlush(void);
  void setNullFlush(bool null_flush);
  void printSteps(bool val)
  {
    printingSteps = val;
  }
  void printRules(bool val)
  {
    printingRules = val;
  }
  void printFilter(bool val)
  {
    printingBranches = val;
  }
  void withoutCoref(bool val)
  {
    noCoref = val;
  }
  void mimicChunker(bool val)
  {
    isLinear = val;
  }
  void printTrees(bool val)
  {
    printingTrees = val;
  }
  void printText(bool val)
  {
    printingText = val;
  }
  void completeTrace(bool val)
  {
    printingAll = val;
  }
  void noFiltering(bool val)
  {
    noFilter = val;
  }
  bool setOutputMode(string mode);
};

#endif
