#ifndef __RTXCHUNK__
#define __RTXCHUNK__

#include <vector>
#include <string>
#include <apertium/apertium_re.h>

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
  TreeModeDot
};

class Chunk
{
public:
  wstring source;
  wstring target;
  wstring coref;
  bool isBlank;
  vector<Chunk*> contents;
  int rule;
  Chunk()
  : isBlank(false), rule(-1)
  {}
  Chunk(wstring blankContent)
  : target(blankContent), isBlank(true), rule(-1)
  {}
  Chunk(wstring src, wstring dest, wstring cor)
  : source(src), target(dest), coref(cor), isBlank(false), rule(-1)
  {}
  Chunk(wstring dest, vector<Chunk*>& children, int r = -1)
  : target(dest), isBlank(false), contents(children), rule(r)
  {}
  Chunk(Chunk& other) // copy constructor
  {
    source = other.source;
    target = other.target;
    coref = other.coref;
    isBlank = other.isBlank;
    contents = other.contents;
    rule = other.rule;
  }
  Chunk(Chunk&& other) // move constructor
  {
    source.swap(other.source);
    target.swap(other.target);
    coref.swap(other.coref);
    isBlank = other.isBlank;
    contents.swap(other.contents);
    rule = other.rule;
  }
  Chunk& operator=(Chunk other)
  {
    source.swap(other.source);
    target.swap(other.target);
    coref.swap(other.coref);
    isBlank = other.isBlank;
    contents.swap(other.contents);
    rule = other.rule;
    return *this;
  }
  Chunk* copy()
  {
    Chunk* ret = new Chunk();
    ret->isBlank = isBlank;
    ret->source = source;
    ret->target = target;
    ret->coref = coref;
    ret->contents.reserve(contents.size());
    for(unsigned int i = 0, limit = contents.size(); i < limit; i++)
    {
      ret->contents.push_back(contents[i]);
    }
    return ret;
  }
  wstring chunkPart(ApertiumRE const &part, const ClipType side)
  {
    string chunk;
    switch(side)
    {
      case SourceClip:
        chunk = UtfConverter::toUtf8(source);
        break;
      case TargetClip:
        chunk = UtfConverter::toUtf8(target);
        break;
      case ReferenceClip:
        chunk = UtfConverter::toUtf8(coref);
        break;
    }
    string result = part.match(chunk);
    if(result.size() == 0)
    {
      return wstring(L"");
    }
    else
    {
      return UtfConverter::fromUtf8(result);
    }
  }
  void setChunkPart(ApertiumRE const &part, wstring const &value)
  {
    string surf = UtfConverter::toUtf8(target);
    if(part.match(surf).size() == 0)
    {
      target += value;
    }
    else
    {
      string val = UtfConverter::toUtf8(value);
      part.replace(surf, val);
      target = UtfConverter::fromUtf8(surf);
    }
  }
  vector<wstring> getTags(const vector<wstring>& parentTags)
  {
    unsigned int last = 0;
    vector<wstring> ret;
    for(unsigned int i = 0, limit = target.size(); i < limit; i++)
    {
      if(target[i] == L'<')
      {
        last = i;
        bool isNum = true;
        for(unsigned int j = i+1; j < limit; j++)
        {
          if(target[j] == L'>')
          {
            if(isNum)
            {
              unsigned int n = stoul(target.substr(last+1, j-last-1));
              if(n != 0 && n <= parentTags.size())
              {
                ret.push_back(parentTags[n-1]);
                last = j+1;
                break;
              }
            }
            wstring tag = target.substr(last, j-last+1);
            ret.push_back(tag);
            last = j+1;
            break;
          }
          if(!isdigit(target[j]))
          {
            isNum = false;
          }
        }
      }
      else if(target[i] == L'\\')
      {
        i++;
      }
    }
    return ret;
  }
  void updateTags(const vector<wstring>& parentTags)
  {
    if(isBlank) return;
    unsigned int last = 0;
    wstring result;
    result.reserve(target.size() + (2*parentTags.size()));
    // a rough estimate - works if most number tags are 1 digit and most new tags are 3 chars or less
    for(unsigned int i = 0, limit = target.size(); i < limit; i++)
    {
      if(target[i] == L'<')
      {
        result += target.substr(last, i-last);
        last = i;
        bool isNum = true;
        for(unsigned int j = i+1; j < limit; j++)
        {
          if(target[j] == L'>')
          {
            if(isNum)
            {
              unsigned int n = stoul(target.substr(last+1, j-last-1));
              if(n != 0 && n <= parentTags.size())
              {
                result += parentTags[n-1];
              }
            }
            else
            {
              result += target.substr(last, j-last+1);
            }
            last = j+1;
            break;
          }
          if(!isdigit(target[j]))
          {
            isNum = false;
          }
        }
      }
      else if(target[i] == L'\\')
      {
        i++;
      }
    }
    if(last != target.size()-1)
    {
      result += target.substr(last);
    }
    target = result;
  }
  void output(const vector<wstring>& parentTags, FILE* out = NULL)
  {
    if(contents.size() > 0)
    {
      vector<wstring> tags = getTags(parentTags);
      for(unsigned int i = 0; i < contents.size(); i++)
      {
        contents[i]->output(tags, out);
      }
    }
    else if(isBlank)
    {
      if(out == NULL)
      {
        cout << UtfConverter::toUtf8(target);
      }
      else
      {
        fputs_unlocked(UtfConverter::toUtf8(target).c_str(), out);
      }
    }
    else
    {
      updateTags(parentTags);
      if(target.size() == 0)
      {
      }
      else if(out == NULL)
      {
        cout << "^" << UtfConverter::toUtf8(target) << "$";
      }
      else
      {
        fputc_unlocked('^', out);
        fputs_unlocked(UtfConverter::toUtf8(target).c_str(), out);
        fputc_unlocked('$', out);
      }
    }
  }
  void output(FILE* out)
  {
    vector<wstring> tags;
    output(tags, out);
  }
  wstring matchSurface()
  {
    if(source.size() == 0)
    {
      return target;
    }
    return source;
  }
  void appendChild(Chunk* kid)
  {
    contents.push_back(kid);
  }
  void writeTree(TreeMode mode, FILE* out)
  {
    switch(mode)
    {
      case TreeModeFlat: writeTreePlain(out, -1); break;
      case TreeModeNest: writeTreePlain(out, 0); break;
      case TreeModeLatex:
        if(isBlank) return;
        if(out == NULL) wcerr << "\\begin{tikzpicture}" << endl << "\\Tree ";
        else fputs_unlocked("\\begin{tikzpicture}\n\\Tree ", out);
        writeTreeLatex(out);
        if(out == NULL) wcerr << endl << "\\end{tikzpicture}" << endl;
        else fputs_unlocked("\n\\end{tikzpicture}\n", out);
        break;
      default:
        wcerr << L"That tree mode has not yet been implemented." << endl;
    }
  }
  void writeTreePlain(FILE* out, int depth)
  {
    if(depth >= 0 && isBlank) return;
    wstring base;
    for(int i = 0; i < depth; i++)
    {
      base += L'\t';
    }
    if(!isBlank) base += L"^";
    if(source.size() > 0)
    {
      base += source + L"/";
    }
    base += target;
    if(coref.size() > 0)
    {
      base += L"/" + coref;
    }
    if(out == NULL)
    {
      wcerr << base;
    }
    else
    {
      fputs_unlocked(UtfConverter::toUtf8(base).c_str(), out);
    }
    if(contents.size() > 0)
    {
      if(out == NULL)
      {
        wcerr << "{";
        if(depth != -1) wcerr << endl;
      }
      else
      {
        fputc_unlocked('{', out);
        if(depth != -1) fputc_unlocked('\n', out);
      }
      int newdepth = (depth == -1) ? -1 : depth + 1;
      for(unsigned int i = 0; i < contents.size(); i++)
      {
        contents[i]->writeTreePlain(out, newdepth);
      }
      for(int i  = 0; i < depth; i++)
      {
        if(out == NULL) wcerr << "\t";
        else fputc_unlocked('\t', out);
      }
      if(out == NULL)
      {
        wcerr << "}";
      }
      else
      {
        fputc_unlocked('}', out);
      }
    }
    if(!isBlank)
    {
      if(out == NULL) wcerr << "$";
      else fputc_unlocked('$', out);
    }
    if(depth != -1)
    {
      if(out == NULL) wcerr << endl;
      else fputc_unlocked('\n', out);
    }
  }
  void writeTreeLatex(FILE* out)
  {
    if(isBlank) return;
    wstring base = L"{";
    if(source.size() > 0) base += source + L"/";
    base += target;
    if(coref.size() > 0) base += L"/" + coref;
    base += L"}";
    if(contents.size() == 0)
    {
      if(out == NULL) wcerr << base;
      else fputs_unlocked(UtfConverter::toUtf8(base).c_str(), out);
    }
    else if(out == NULL)
    {
      wcerr << "[." << base;
      for(unsigned int i = 0; i < contents.size(); i++)
      {
        wcerr << " ";
        contents[i]->writeTreeLatex(out);
      }
      wcerr << "]";
    }
    else
    {
      fputs_unlocked(UtfConverter::toUtf8(L"[." + base).c_str(), out);
      for(unsigned int i = 0; i < contents.size(); i++)
      {
        fputc_unlocked(' ', out);
        contents[i]->writeTreeLatex(out);
      }
      fputc_unlocked(']', out);
    }
  }
};

#endif
