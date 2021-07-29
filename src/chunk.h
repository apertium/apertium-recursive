#ifndef __RTXCHUNK__
#define __RTXCHUNK__

#include <apertium/apertium_re.h>

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
  UString source;
  UString target;
  UString coref;
  UString wblank;
  bool isBlank;
  bool isJoiner;
  vector<Chunk*> contents;
  int rule;
  
  Chunk()
  : isBlank(false), isJoiner(false), rule(-1)
  {}
  Chunk(UString blankContent)
  : target(blankContent), isBlank(true), isJoiner(false), rule(-1)
  {}
  Chunk(UString src, UString dest, UString cor, UString wbl)
  : source(src), target(dest), coref(cor), wblank(wbl), isBlank(false), isJoiner(false), rule(-1)
  {}
  Chunk(UString dest, vector<Chunk*>& children, int r = -1)
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
  
  UString chunkPart(ApertiumRE const &part, const ClipType side);
  void setChunkPart(ApertiumRE const &part, UString const &value);
  vector<UString> getTags(const vector<UString>& parentTags);
  void updateTags(const vector<UString>& parentTags);
  void output(const vector<UString>& parentTags, UFILE* out);
  void output(UFILE* out);
  UString matchSurface();
  void appendChild(Chunk* kid);
  void conjoin(Chunk* other);
  void writeTree(TreeMode mode, UFILE* out);
  
private:
  static pair<UString, UString> chopString(UString s);
  static void writeString(UString s, UFILE* out);
  void writeTreePlain(UFILE* out, int depth);
  void writeTreeLatex(UFILE* out);
  UString writeTreeDot(UFILE* out);
  vector<vector<UString>> writeTreeBox();
};

/**
 * Combines two wordbound blanks and returns it
*/
UString combineWblanks(UString wblank_current, UString wblank_to_add);

#endif
