#ifndef __RTXCHUNK__
#define __RTXCHUNK__

#include <rtx_config.h>
#include <apertium_re.h>
#include <apertium/string_utils.h>

#include <vector>
#include <string>
#include <set>

enum ClipType
{
  SourceClip,
  TargetClip,
  ReferenceClip
};

enum TreeMode
{
  TreeModeFlat,
  TreeModeNest,
  TreeModeLatex,
  TreeModeDot,
  TreeModeBox
};

class Chunk
{
public:
  wstring source;
  wstring target;
  wstring coref;
  wstring wblank;
  bool isBlank;
  bool isJoiner;
  vector<Chunk*> contents;
  int rule;
  
  Chunk()
  : isBlank(false), isJoiner(false), rule(-1)
  {}
  Chunk(wstring blankContent)
  : target(blankContent), isBlank(true), isJoiner(false), rule(-1)
  {}
  Chunk(wstring src, wstring dest, wstring cor, wstring wbl)
  : source(src), target(dest), coref(cor), wblank(wbl), isBlank(false), isJoiner(false), rule(-1)
  {}
  Chunk(wstring dest, vector<Chunk*>& children, int r)
  : target(dest), isBlank(false), isJoiner(false), contents(children), rule(r)
  {}
  Chunk(Chunk& other) // copy constructor
  {
    source = other.source;
    target = other.target;
    coref = other.coref;
    wblank = other.wblank;
    isBlank = other.isBlank;
    isJoiner = other.isJoiner;
    contents = other.contents;
    rule = other.rule;
  }
  Chunk(Chunk&& other) // move constructor
  {
    source.swap(other.source);
    target.swap(other.target);
    coref.swap(other.coref);
    wblank.swap(other.wblank);
    isBlank = other.isBlank;
    isJoiner = other.isJoiner;
    contents.swap(other.contents);
    rule = other.rule;
  }
  Chunk& operator=(Chunk other)
  {
    source.swap(other.source);
    target.swap(other.target);
    coref.swap(other.coref);
    wblank.swap(other.wblank);
    isBlank = other.isBlank;
    isJoiner = other.isJoiner;
    contents.swap(other.contents);
    rule = other.rule;
    return *this;
  }
  Chunk* copy()
  {
    Chunk* ret = new Chunk();
    ret->isBlank = isBlank;
    ret->isJoiner = isJoiner;
    ret->source = source;
    ret->target = target;
    ret->coref = coref;
    ret->wblank = wblank;
    ret->contents.reserve(contents.size());
    for(unsigned int i = 0, limit = contents.size(); i < limit; i++)
    {
      ret->contents.push_back(contents[i]);
    }
    return ret;
  }
  
  wstring chunkPart(ApertiumRE const &part, const ClipType side);
  void setChunkPart(ApertiumRE const &part, wstring const &value);
  vector<wstring> getTags(const vector<wstring>& parentTags);
  void updateTags(const vector<wstring>& parentTags);
  void output(const vector<wstring>& parentTags, FILE* out);
  void output(FILE* out);
  wstring matchSurface();
  void appendChild(Chunk* kid);
  void conjoin(Chunk* other);
  void writeTree(TreeMode mode, FILE* out);
  
private:
  static pair<wstring, wstring> chopString(wstring s);
  static void writeString(wstring s, FILE* out);
  void writeTreePlain(FILE* out, int depth);
  void writeTreeLatex(FILE* out);
  wstring writeTreeDot(FILE* out);
  vector<vector<wstring>> writeTreeBox();
};

/**
 * Combines two wordbound blanks and returns it
*/
wstring combineWblanks(wstring wblank_current, wstring wblank_to_add);

#endif
