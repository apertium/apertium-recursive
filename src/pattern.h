#ifndef __RTXPATTERNBUILDER__
#define __RTXPATTERNBUILDER__

#include <apertium/transfer_data.h>
#include <string>
#include <vector>

struct PatternElement
{
  wstring lemma;
  vector<wstring> tags;
};

class PatternBuilder
{
private:
  TransferData td;
public:
  void addPattern(vector<vector<PatternElement>> pat, int rule);
};

#endif
