#ifndef __RTXCHUNK__
#define __RTXCHUNK__

#include <rtx_config.h>
#include <vector>
#include <string>
#include <apertium_re.h>
#include <apertium/string_utils.h>

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
  Chunk(wstring src, wstring dest, wstring cor)
  : source(src), target(dest), coref(cor), isBlank(false), isJoiner(false), rule(-1)
  {}
  Chunk(wstring dest, vector<Chunk*>& children, int r = -1)
  : target(dest), isBlank(false), isJoiner(false), contents(children), rule(r)
  {}
  Chunk(Chunk& other) // copy constructor
  {
    source = other.source;
    target = other.target;
    coref = other.coref;
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
      //target += value;
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
        cout << "^";
        cout << UtfConverter::toUtf8(target);
        cout << "$";
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
        writeString(L"\\begin{forest}\n%where n children=0{tier=word}{}\n", out);
        writeString(L"% Uncomment the preceding line to make the LUs bottom-aligned.\n", out);
        writeTreeLatex(out);
        writeString(L"\n\\end{forest}\n", out);
        break;
      case TreeModeDot:
        if(isBlank) return;
        writeString(L"digraph {", out);
        writeTreeDot(out);
        writeString(L"}\n", out);
        break;
      case TreeModeBox:
      {
        if(isBlank) return;
        vector<vector<wstring>> tree = writeTreeBox();
        if(tree.size() == 0) return;
        unsigned int tr = 4, sl = 12, st = 11, tl = 12, tt = 11, rl = 0, rt = 0;
        for(unsigned int i = 0; i < tree.size(); i++)
        {
          if(tree[i][0].size() > tr) tr = tree[i][0].size();
          if(tree[i][1].size() > sl) sl = tree[i][1].size();
          if(tree[i][2].size() > st) st = tree[i][2].size();
          if(tree[i][3].size() > tl) tl = tree[i][3].size();
          if(tree[i][4].size() > tt) tt = tree[i][4].size();
          if(tree[i][5].size() > rl) rl = tree[i][5].size();
          if(tree[i][6].size() > rt) rt = tree[i][6].size();
        }
        bool doCoref = (rl > 0 || rt > 0);
        if(doCoref && rl < 17) rl = 17;
        if(doCoref && rt < 16) rt = 16;
        writeString(L"Tree" + wstring(tr-3, L' '), out);
        writeString(L"Source Lemma" + wstring(sl - 11, L' '), out);
        writeString(L"Source Tags" + wstring(st - 10, L' '), out);
        writeString(L"Target Lemma" + wstring(tl - 11, L' '), out);
        writeString(L"Target Tags" + wstring(tt - 10, L' '), out);
        if(doCoref)
        {
          writeString(L"Coreference Lemma" + wstring(rl - 16, L' '), out);
          writeString(L"Coreference Tags", out);
          if(rt > 16) writeString(wstring(rt - 16, L' '), out);
        }
        writeString(L"\n", out);
        writeString(wstring(tr, L'─') + L" ", out);
        writeString(wstring(sl, L'─') + L" ", out);
        writeString(wstring(st, L'─') + L" ", out);
        writeString(wstring(tl, L'─') + L" ", out);
        writeString(wstring(tt, L'─'), out);
        if(doCoref) writeString(L" " + wstring(rl, L'─'), out);
        if(doCoref) writeString(L" " + wstring(rt, L'─'), out);
        writeString(L"\n", out);
        for(unsigned int i = 0; i < tree.size(); i++)
        {
          writeString(wstring(tr - tree[i][0].size(), L' ') + tree[i][0] + L" ", out);
          writeString(tree[i][1] + wstring(sl - tree[i][1].size() + 1, L' '), out);
          writeString(tree[i][2] + wstring(st - tree[i][2].size() + 1, L' '), out);
          writeString(tree[i][3] + wstring(tl - tree[i][3].size() + 1, L' '), out);
          writeString(tree[i][4] + wstring(tt - tree[i][4].size(), L' '), out);
          if(doCoref)
          {
            writeString(L" " + tree[i][5] + wstring(rl - tree[i][5].size(), L' '), out);
            writeString(L" " + tree[i][6], out);
          }
          writeString(L"\n", out);
        }
        writeString(L"\n", out);
      }
        break;
      default:
        wcerr << L"That tree mode has not yet been implemented." << endl;
    }
  }
private:
  static pair<wstring, wstring> chopString(wstring s)
  {
    wstring lem;
    wstring tags;
    for(unsigned int i = 0; i < s.size(); i++)
    {
      if(s[i] == L'<')
      {
        lem = s.substr(0, i);
        tags = s.substr(i+1, s.size()-i-2);
        break;
      }
    }
    if(lem.size() == 0 && tags.size() == 0 && s.size() > 0)
    {
      lem = s;
    }
    return make_pair(lem, StringUtils::substitute(tags, L"><", L"."));
  }
  static void writeString(wstring s, FILE* out)
  {
    if(out == NULL) wcerr << s;
    else fputs_unlocked(UtfConverter::toUtf8(s).c_str(), out);
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
    writeString(base, out);
    if(contents.size() > 0)
    {
      writeString((depth == -1) ? L"{" : L"{\n", out);
      int newdepth = (depth == -1) ? -1 : depth + 1;
      for(unsigned int i = 0; i < contents.size(); i++)
      {
        contents[i]->writeTreePlain(out, newdepth);
      }
      for(int i  = 0; i < depth; i++)
      {
        writeString(L"\t", out);
      }
      writeString(L"}", out);
    }
    if(!isBlank) writeString(L"$", out);
    if(depth != -1) writeString(L"\n", out);
  }
  void writeTreeLatex(FILE* out)
  {
    if(isBlank) return;
    wstring nl = L" \\\\ ";
    wstring base;
    pair<wstring, wstring> p;
    if(source.size() > 0)
    {
      p = chopString(source);
      base += L"\\textbf{" + p.first + L"}" + nl + L"\\texttt{" + p.second + L"}" + nl;
    }
    p = chopString(target);
    if(contents.size() == 0)
    {
      base += L"\\textit{" + p.first + L"}" + nl + L"\\texttt{" + p.second + L"}";
    }
    else
    {
      unsigned int i = 0;
      for(; i < p.second.size(); i++)
      {
        if(p.second[i] == L'.') break;
      }
      if(i < p.second.size())
      {
        base += p.second.substr(0, i) + nl + L"\\textit{" + p.first + L"}";
        base += nl + L"\\texttt{" + p.second.substr(i+1) + L"}";
      }
      else
      {
        base += p.second + nl + L"\\textit{" + p.first + L"}";
      }
    }
    if(coref.size() > 0)
    {
      p = chopString(coref);
      base += nl + L"\\textit{" + p.first + L"}" + nl + L"\\texttt{" + p.second + L"}";
    }
    base = L"[{ \\begin{tabular}{c} " + base + L" \\end{tabular} } ";
    base = StringUtils::substitute(base, L"_", L"\\_");
    writeString(base, out);
    for(unsigned int i = 0; i < contents.size(); i++) contents[i]->writeTreeLatex(out);
    writeString(L" ]", out);
  }
  wstring writeTreeDot(FILE* out)
  {
    if(isBlank) return L"";
    static int nodeId = 0;
    nodeId++;
    wstring name = L"n" + to_wstring(nodeId);
    wstring node = name + L" \\[label=\"";
    if(source.size() > 0)
    {
      node += source + L"\\\\n";
    }
    node += target;
    if(coref.size() > 0)
    {
      node += L"\\\\n" + coref;
    }
    node += L"\"\\];";
    writeString(node, out);
    for(unsigned int i = 0; i < contents.size(); i++)
    {
      wstring kid = contents[i]->writeTreeDot(out);
      if(kid.size() > 0) writeString(name + L" -> " + kid + L";", out);
    }
    return name;
  }
  vector<vector<wstring>> writeTreeBox()
  {
    if(contents.size() == 0)
    {
      vector<wstring> ret;
      ret.resize(7);
      pair<wstring, wstring> p = chopString(source);
      ret[1] = p.first; ret[2] = p.second;
      p = chopString(target);
      ret[3] = p.first; ret[4] = p.second;
      p = chopString(coref);
      ret[5] = p.first; ret[6] = p.second;
      return vector<vector<wstring>>(1, ret);
    }
    else
    {
      vector<pair<unsigned int, unsigned int>> bounds;
      vector<vector<wstring>> tree;
      for(unsigned int i = 0; i < contents.size(); i++)
      {
        if(!contents[i]->isBlank)
        {
          vector<vector<wstring>> temp = contents[i]->writeTreeBox();
          tree.insert(tree.end(), temp.begin(), temp.end());
          if(temp.size() == 1)
          {
            bounds.push_back(make_pair(tree.size() -1, tree.size() - 1));
            continue;
          }
          int first = -1, last = -1;
          for(unsigned int j = tree.size() - temp.size(); j < tree.size(); j++)
          {
            if(first == -1 && tree[j][0][0] != L' ') first = j;
            else if(first != -1 && last == -1 && tree[j][0][0] == L' ') last = j-1;
          }
          first = (first == -1) ? tree.size() - temp.size() : first;
          last = (last == -1) ? tree.size() - 1 : last;
          bounds.push_back(make_pair((unsigned int)first, (unsigned int)last));
        }
      }
      if(tree.size() == 1)
      {
        tree[0][0] = L"─" + tree[0][0];
        return tree;
      }
      unsigned int center = tree.size() / 2;
      unsigned int len = 0;
      for(unsigned int i = 0; i < tree.size(); i++)
      {
        if(tree[i][0].size() > len) len = tree[i][0].size();
      }
      set<unsigned int> lines;
      for(unsigned int i = 0; i < bounds.size(); i++)
      {
        if(bounds[i].second < center) lines.insert(bounds[i].second);
        else if(bounds[i].first > center) lines.insert(bounds[i].first);
        else lines.insert(center);
      }
      unsigned int firstLine = *lines.begin();
      unsigned int lastLine = *lines.rbegin();
      for(unsigned int i = 0; i < tree.size(); i++)
      {
        unsigned int sz = tree[i][0].size();
        if(lines.count(i) == 0)
        {
          tree[i][0] = wstring(len - sz, L' ') + tree[i][0];
        }
        else
        {
          if(sz > 0)
          {
            switch(tree[i][0][0])
            {
              case L'│': tree[i][0][0] = L'┤'; break;
              case L'├': tree[i][0][0] = L'┼'; break;
              case L'┌': tree[i][0][0] = L'┬'; break;
              case L'└': tree[i][0][0] = L'┴'; break;
              default: break;
            }
          }
          tree[i][0] = wstring(len - sz, L'─') + tree[i][0];
        }
        if(i < firstLine || i > lastLine) tree[i][0] = L' ' + tree[i][0];
        else if(i == firstLine && i == lastLine) tree[i][0] = L'─' + tree[i][0];
        else if(i == firstLine) tree[i][0] = L'┌' + tree[i][0];
        else if(i > firstLine && i < lastLine)
        {
          if(lines.count(i) == 0) tree[i][0] = L'│' + tree[i][0];
          else tree[i][0] = L'├' + tree[i][0];
        }
        else if(i == lastLine) tree[i][0] = L'└' + tree[i][0];
      }
      return tree;
    }
  }
};

#endif
