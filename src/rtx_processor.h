#ifndef __RTXPROCESSOR__
#define __RTXPROCESSOR__

#include <apertium/apertium_re.h>
#include <lttoolbox/alphabet.h>
#include <matcher.h>
#include <chunk.h>
#include <pool.h>
#include <lttoolbox/input_file.h>

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
  UString s;
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
  map<UString, ApertiumRE> attr_items;

  /**
   * Virtual machine global variables
   * name => value
   */
  map<UString, UString> variables;
  
  /**
   * Virtual machine global variables to wblank map
   * name => value
   */
  map<UString, UString> wblank_variables;

  /**
   * Lists
   * name => { values }
   */
  map<UString, set<UString>> lists;

  /**
   * Lists, but all values are converted to lower case
   * Used for case-insensitive comparison
   * name => { values }
   */
  map<UString, set<UString>> listslow;

  /**
   * Bytecode for input-time rules
   */
  vector<UString> rule_map;

  /**
   * Bytecode for output-time rules
   */
  vector<UString> output_rules;

  /**
   * Debug names for input-time rules (may be empty)
   */
  vector<UString> inRuleNames;

  /**
   * Debug names for output-time rules (may be empty)
   */
  vector<UString> outRuleNames;

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
  bool furtherInput = true;

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
   * A parallel stack to store wordbound blanks that mimics the operations
   * of the main stack. wblanks are added everytime lemmas are clipped
   */
  UString theWblankStack[32];
  
  /**
   * wordbound blank to be output
   */
  UString out_wblank;

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
   * Blanks waiting to be written to output stream
   * Note that this is updated by processGLR(), NOT readToken()
   * The reason for this is that if a blank is outside a parse tree
   * (such as at the very beginning or very end of the stream)
   * then we want to output it directly, particularly if it's empty
   * and because of lookahead, only processGLR() knows which blanks are which
   */
  list<UString> blankQueue;

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
  ParseNode* currentBranch = nullptr;

  //////////
  // SETTINGS
  //////////

  /**
   * true if the next input token should be parsed as an LU, false otherwise
   */
  bool inword = false;
  
  /**
   * true if the next input token should be parsed as a wordbound blank, false otherwise
   */
  bool inwblank = false;

  /**
   * Whether output should flush on \0
   */
  bool null_flush = false;

  /**
   * If true, each instruction of virtual machine will be printed to cerr
   */
  bool printingSteps = false;

  /**
   * If true, each rule that is applied will be printed to cerr
   */
  bool printingRules = false;

  /**
   * If true, each action of filterParseGraph() will be logged to cerr
   */
  bool printingBranches = false;

  /**
   * If true, produce a full report, similar to (printingRules && printingBranches)
   * Affected by treePrintMode
   */
  bool printingAll = false;

  /**
   * false if input comes from apertium-anaphora, true otherwise
   */
  bool noCoref = true;

  /**
   * true if rule application should mimic the chunker-interchunk-postchunk
   * pipeline, false otherwise
   */
  bool isLinear = false;

  /**
   * If true, parse tree will be printed according to treePrintMode
   * before output-time rules are applied
   */
  bool printingTrees = false;

  /**
   * If false, output-time rules will not be applied and linear output
   * will not be produced
   */
  bool printingText = true;

  /**
   * Manner in which to print trees
   * Set by setOutputMode()
   * Enum defined in chunk.h
   */
  TreeMode treePrintMode = TreeModeNest;

  /**
   * Counter used to give distinct, consistent identifiers to ParseNodes
   * for tracing purposes
   */
  int newBranchId = 0;

  /**
   * If this is set to true, filterParseGraph() will only discard branches
   * on parse error
   */
  bool noFilter = true;

  //////////
  // VIRTUAL MACHINE
  //////////

  /**
   * Return whether str1 begins with str2
   */
  bool beginsWith(UString const &str1, UString const &str2) const;

  /**
   * Return whether str1 ends with str2
   */
  bool endsWith(UString const &str1, UString const &str2) const;

  /**
   * The virtual machine
   * Modifies: theStack, stackIdx, variables, currentOutput
   * Reads from: currentInput, parentChunk
   * @param rule - bytecode for rule to be applied
   * @return false if REJECTRULE was executed, true otherwise
   */
  bool applyRule(const UString& rule);

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
   * Pop and return a UString from theStack
   * Log error and call exit(1) if top element is not a UString
   */
  UString popString();

  /**
   * Equivalent to popString(), but with called as
   * UString x; popString(x);
   * rather than
   * UString x = popString();
   * This uses a swap to save an allocation and a copy, which is almost twice
   * as fast, which has a noticeable impact on overall speed
   */
  void popString(UString& dest);

  /**
   * Pop and return a Chunk pointer from theStack
   * Log error and call exit(1) if top element is not a Chunk*
   */
  Chunk* popChunk();

  inline void pushStack(bool b)
  {
    theStack[++stackIdx].mode = 0;
    theStack[stackIdx].b = b;
    theWblankStack[stackIdx].clear();
  }
  inline void pushStack(int i)
  {
    theStack[++stackIdx].mode = 1;
    theStack[stackIdx].i = i;
    theWblankStack[stackIdx].clear();
  }
  inline void pushStack(const UString& s, UString wbl = ""_u)
  {
    theStack[++stackIdx].mode = 2;
    theStack[stackIdx].s.assign(s);
    theWblankStack[stackIdx] = wbl;
  }
  inline void pushStack(Chunk* c)
  {
    theStack[++stackIdx].mode = 3;
    theStack[stackIdx].c = c;
    theWblankStack[stackIdx].clear();
  }

  /**
   * Copy the contents of theStack[src] to theStack[dest]
   */
  void stackCopy(int src, int dest);

  //////////
  // RULE SELECTION AND I/O
  //////////

  InputFile infile;

  /**
   * Read an LU or a blank
   * Modifies: furtherInput
   * @param in - input stream
   * @return pointer to token read
   */
  Chunk* readToken();

  bool lookahead(ParseNode* node);

  /**
   * Check whether any rules can apply to node
   * if there are any, select one and apply it
   * if not or there is a shift-reduce conflict, fork
   * append resulting node(s) to result
   */
  void checkForReduce(vector<ParseNode*>& result, ParseNode* node);

  /**
   * Output the next blank in blankQueue, or a space if the queue is empty
   */
  void writeBlank(UFILE* out);

  /**
   * Apply output-time rules and write nodes to output stream
   * @param out - output stream
   */
  void outputAll(UFILE* out);

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
  void processGLR(UFILE* out);

  /**
   * Apply longest rule matching the beginning of t1x and append the result to t2x
   */
  void processTRXLayer(list<Chunk*>& t1x, list<Chunk*>& t2x);

  /**
   * Mimic apertium-transfer | apertium-interchunk | apertium-postchunk
   * Read input, call processTRXLayer twice, apply output-time rules, output
   */
  void processTRX(UFILE* out);
  
  /**
   * True if clipping lem/lemh/whole
  */
  bool gettingLemmaFromWord(UString attr);
  
public:
  RTXProcessor();
  ~RTXProcessor();

  void read(string const &filename);
  void process(FILE *in, UFILE *out);
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
