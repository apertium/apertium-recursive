#include <rtx_config.h>
#include <chunk.h>
#include <lttoolbox/string_utils.h>

#include <iostream>

UString
combineWblanks(UString wblank_current, UString wblank_to_add)
{
  if(wblank_current.empty() && wblank_to_add.empty())
  {
    return wblank_current;
  }
  else if(wblank_current.empty())
  {
    return wblank_to_add;
  }
  else if(wblank_to_add.empty())
  {
    return wblank_current;
  }

  UString new_out_wblank;
  for(UString::const_iterator it = wblank_current.begin(); it != wblank_current.end(); it++)
  {
    if(*it == '\\')
    {
      new_out_wblank += *it;
      it++;
      new_out_wblank += *it;
    }
    else if(*it == ']')
    {
      if(*(it+1) == ']')
      {
        new_out_wblank += ';';
        break;
      }
    }
    else
    {
      new_out_wblank += *it;
    }
  }

  for(UString::const_iterator it = wblank_to_add.begin(); it != wblank_to_add.end(); it++)
  {
    if(*it == '\\')
    {
      new_out_wblank += *it;
      it++;
      new_out_wblank += *it;
    }
    else if(*it == '[')
    {
      if(*(it+1) == '[')
      {
        new_out_wblank += ' ';
        it++;
      }
    }
    else
    {
      new_out_wblank += *it;
    }
  }

  return new_out_wblank;
}

UString
Chunk::chunkPart(ApertiumRE const &part, const ClipType side)
{
  switch(side)
  {
    case SourceClip:
      return part.match(source);
      break;
    case TargetClip:
      return part.match(target);
      break;
    case ReferenceClip:
      return part.match(coref);
      break;
  }
  return ""_u;
}

void
Chunk::setChunkPart(ApertiumRE const &part, UString const &value)
{
  part.replace(target, value);
}

vector<UString>
Chunk::getTags(const vector<UString>& parentTags)
{
  unsigned int last = 0;
  vector<UString> ret;
  for(unsigned int i = 0, limit = target.size(); i < limit; i++)
  {
    if(target[i] == '<')
    {
      last = i;
      bool isNum = true;
      for(unsigned int j = i+1; j < limit; j++)
      {
        if(target[j] == '>')
        {
          if(isNum)
          {
            unsigned int n = StringUtils::stoi(target.substr(last+1, j-last-1));
            if(n != 0 && n <= parentTags.size())
            {
              ret.push_back(parentTags[n-1]);
              last = j+1;
              break;
            }
          }
          UString tag = target.substr(last, j-last+1);
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
    else if(target[i] == '\\')
    {
      i++;
    }
  }
  return ret;
}

void
Chunk::updateTags(const vector<UString>& parentTags)
{
  if(isBlank) return;
  unsigned int last = 0;
  UString result;
  result.reserve(target.size() + (2*parentTags.size()));
  // a rough estimate - works if most number tags are 1 digit and most new tags are 3 chars or less
  for(unsigned int i = 0, limit = target.size(); i < limit; i++)
  {
    if(target[i] == '<')
    {
      result += target.substr(last, i-last);
      last = i;
      bool isNum = true;
      for(unsigned int j = i+1; j < limit; j++)
      {
        if(target[j] == '>')
        {
          if(isNum)
          {
            unsigned int n = StringUtils::stoi(target.substr(last+1, j-last-1));
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
    else if(target[i] == '\\')
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

void
Chunk::output(const vector<UString>& parentTags, UFILE* out = NULL)
{
  if(contents.size() > 0)
  {
    vector<UString> tags = getTags(parentTags);
    for(unsigned int i = 0; i < contents.size(); i++)
    {
      contents[i]->output(tags, out);
    }
  }
  else if(isBlank)
  {
    if(out == NULL)
    {
      cout << target;
    }
    else
    {
      write(target, out);
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
      cout << wblank;
      cout << "^";
      cout << target;
      cout << "$";
    }
    else
    {
      u_fprintf(out, "%S^%S$", wblank.c_str(), target.c_str());
    }
  }
}

void
Chunk::output(UFILE* out)
{
  vector<UString> tags;
  output(tags, out);
}

UString
Chunk::matchSurface()
{
  if(contents.size() == 0)
  {
    return source;
  }
  return target;
}

void
Chunk::appendChild(Chunk* kid)
{
  contents.push_back(kid);
}

void
Chunk::conjoin(Chunk* other)
{
  unsigned int lemq_loc = 0;
  for(; lemq_loc < target.size(); lemq_loc++)
  {
    if(target[lemq_loc] == '\\')
    {
      lemq_loc++;
      continue;
    }
    else if(target[lemq_loc] == '#')
    {
      break;
    }
  }
  target.insert(lemq_loc, "+"_u + other->target);
  wblank = combineWblanks(other->wblank, wblank);
}

void
Chunk::writeTree(TreeMode mode, UFILE* out)
{
  switch(mode)
  {
    case TreeModeFlat: writeTreePlain(out, -1); break;
    case TreeModeNest: writeTreePlain(out, 0); break;
    case TreeModeLatex:
      if(isBlank) return;
      writeString("\\begin{forest}\n%where n children=0{tier=word}{}\n"_u, out);
      writeString("% Uncomment the preceding line to make the LUs bottom-aligned.\n"_u, out);
      writeTreeLatex(out);
      writeString("\n\\end{forest}\n"_u, out);
      break;
    case TreeModeDot:
      if(isBlank) return;
      writeString("digraph {"_u, out);
      writeTreeDot(out);
      writeString("}\n"_u, out);
      break;
    case TreeModeBox:
    {
      if(isBlank) return;
      vector<vector<UString>> tree = writeTreeBox();
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
      writeString("Tree"_u + UString(tr-3, ' '), out);
      writeString("Source Lemma"_u + UString(sl - 11, ' '), out);
      writeString("Source Tags"_u + UString(st - 10, ' '), out);
      writeString("Target Lemma"_u + UString(tl - 11, ' '), out);
      writeString("Target Tags"_u + UString(tt - 10, ' '), out);
      if(doCoref)
      {
        writeString("Coreference Lemma"_u + UString(rl - 16, ' '), out);
        writeString("Coreference Tags"_u, out);
        if(rt > 16) writeString(UString(rt - 16, ' '), out);
      }
      writeString("\n"_u, out);
      UChar dash = u'\u2500'; // '─'
      writeString(UString(tr, dash) + " "_u, out);
      writeString(UString(sl, dash) + " "_u, out);
      writeString(UString(st, dash) + " "_u, out);
      writeString(UString(tl, dash) + " "_u, out);
      writeString(UString(tt, dash), out);
      if(doCoref) writeString(" "_u + UString(rl, dash), out);
      if(doCoref) writeString(" "_u + UString(rt, dash), out);
      writeString("\n"_u, out);
      for(unsigned int i = 0; i < tree.size(); i++)
      {
        writeString(UString(tr - tree[i][0].size(), ' ') + tree[i][0] + " "_u, out);
        writeString(tree[i][1] + UString(sl - tree[i][1].size() + 1, ' '), out);
        writeString(tree[i][2] + UString(st - tree[i][2].size() + 1, ' '), out);
        writeString(tree[i][3] + UString(tl - tree[i][3].size() + 1, ' '), out);
        writeString(tree[i][4] + UString(tt - tree[i][4].size(), ' '), out);
        if(doCoref)
        {
          writeString(" "_u + tree[i][5] + UString(rl - tree[i][5].size(), ' '), out);
          writeString(" "_u + tree[i][6], out);
        }
        writeString("\n"_u, out);
      }
      writeString("\n"_u, out);
    }
      break;
    default:
      cerr << "That tree mode has not yet been implemented." << endl;
  }
}

pair<UString, UString>
Chunk::chopString(UString s)
{
  UString lem;
  UString tags;
  for(unsigned int i = 0; i < s.size(); i++)
  {
    if(s[i] == '<')
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
  return make_pair(lem, StringUtils::substitute(tags, "><"_u, "."_u));
}

void
Chunk::writeString(UString s, UFILE* out)
{
  if(out == NULL) cerr << s;
  else write(s, out);
}

void
Chunk::writeTreePlain(UFILE* out, int depth)
{
  if(depth > 0 && isBlank) return;
  UString base;
  for(int i = 0; i < depth; i++)
  {
    base += '\t';
  }
  if(!isBlank)
  {
    if(wblank.size() > 0)
    {
      base += wblank;
    }
    base += '^';
  }
  if(source.size() > 0)
  {
    base += source + "/"_u;
  }
  base += target;
  if(coref.size() > 0)
  {
    base += "/"_u + coref;
  }
  writeString(base, out);
  if(contents.size() > 0)
  {
    writeString((depth == -1) ? "{"_u : "{\n"_u, out);
    int newdepth = (depth == -1) ? -1 : depth + 1;
    for(unsigned int i = 0; i < contents.size(); i++)
    {
      contents[i]->writeTreePlain(out, newdepth);
    }
    for(int i  = 0; i < depth; i++)
    {
      writeString("\t"_u, out);
    }
    writeString("}"_u, out);
  }
  if(!isBlank) writeString("$"_u, out);
  if(depth != -1) writeString("\n"_u, out);
}

void
Chunk::writeTreeLatex(UFILE* out)
{
  if(isBlank) return;
  UString nl = " \\\\ "_u;
  UString base;
  pair<UString, UString> p;
  if(source.size() > 0)
  {
    p = chopString(source);
    base += "\\textbf{"_u + p.first + "}"_u + nl + "\\texttt{"_u + p.second + "}"_u + nl;
  }
  p = chopString(target);
  if(contents.size() == 0)
  {
    base += "\\textit{"_u + p.first + "}"_u + nl + "\\texttt{"_u + p.second + "}"_u;
  }
  else
  {
    unsigned int i = 0;
    for(; i < p.second.size(); i++)
    {
      if(p.second[i] == '.') break;
    }
    if(i < p.second.size())
    {
      base += p.second.substr(0, i) + nl + "\\textit{"_u + p.first + "}"_u;
      base += nl + "\\texttt{"_u + p.second.substr(i+1) + "}"_u;
    }
    else
    {
      base += p.second + nl + "\\textit{"_u + p.first + "}"_u;
    }
  }
  if(coref.size() > 0)
  {
    p = chopString(coref);
    base += nl + "\\textit{"_u + p.first + "}"_u + nl + "\\texttt{"_u + p.second + "}"_u;
  }
  base = "[{ \\begin{tabular}{c} "_u + base + " \\end{tabular} } "_u;
  base = StringUtils::substitute(base, "_"_u, "\\_"_u);
  writeString(base, out);
  for(unsigned int i = 0; i < contents.size(); i++) contents[i]->writeTreeLatex(out);
  writeString(" ]"_u, out);
}

UString
Chunk::writeTreeDot(UFILE* out)
{
  if(isBlank) return ""_u;
  static int nodeId = 0;
  nodeId++;
  UString name = "n"_u + StringUtils::itoa(nodeId);
  UString node = name;
  node += " \\[label=\""_u;
  if(source.size() > 0)
  {
    node += source;
    node += "\\\\n"_u;
  }
  node += target;
  if(coref.size() > 0)
  {
    node += "\\\\n"_u;
    node += coref;
  }
  node += "\"\\];"_u;
  writeString(node, out);
  for(unsigned int i = 0; i < contents.size(); i++)
  {
    UString kid = contents[i]->writeTreeDot(out);
    if(kid.size() > 0) writeString(name + " -> "_u + kid + ";"_u, out);
  }
  return name;
}

vector<vector<UString>>
Chunk::writeTreeBox()
{
  if(contents.size() == 0)
  {
    vector<UString> ret;
    ret.resize(7);
    pair<UString, UString> p = chopString(source);
    ret[1] = p.first; ret[2] = p.second;
    p = chopString(target);
    ret[3] = p.first; ret[4] = p.second;
    p = chopString(coref);
    ret[5] = p.first; ret[6] = p.second;
    return vector<vector<UString>>(1, ret);
  }
  else
  {
    vector<pair<unsigned int, unsigned int>> bounds;
    vector<vector<UString>> tree;
    for(unsigned int i = 0; i < contents.size(); i++)
    {
      if(!contents[i]->isBlank)
      {
        vector<vector<UString>> temp = contents[i]->writeTreeBox();
        tree.insert(tree.end(), temp.begin(), temp.end());
        if(temp.size() == 1)
        {
          bounds.push_back(make_pair(tree.size() -1, tree.size() - 1));
          continue;
        }
        int first = -1, last = -1;
        for(unsigned int j = tree.size() - temp.size(); j < tree.size(); j++)
        {
          if(first == -1 && tree[j][0][0] != ' ') first = j;
          else if(first != -1 && last == -1 && tree[j][0][0] == ' ') last = j-1;
        }
        first = (first == -1) ? tree.size() - temp.size() : first;
        last = (last == -1) ? tree.size() - 1 : last;
        bounds.push_back(make_pair((unsigned int)first, (unsigned int)last));
      }
    }
    if(tree.size() == 1)
    {
      tree[0][0].insert(tree[0][0].begin(), u'\u2500'); // '─'
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
        tree[i][0] = UString(len - sz, ' ') + tree[i][0];
      }
      else
      {
        if(sz > 0)
        {
          switch(tree[i][0][0])
          {
          case u'\u2502': // '│'
            tree[i][0][0] = u'\u2524'; break; // '┤'
          case u'\u251c': // '├'
            tree[i][0][0] = u'\u253c'; break; // '┼'
          case u'\u250c': // '┌'
            tree[i][0][0] = u'\u252c'; break; // '┬'
          case u'\u2514': // '└'
            tree[i][0][0] = u'\u2534'; break; // '┴'
          default: break;
          }
        }
        tree[i][0] = UString(len - sz, u'\u2500') + tree[i][0]; // '─'
      }
      UChar prefix = ' ';
      if (i > firstLine && i < lastLine) {
        if (lines.count(i) == 0) {
          prefix = u'\u2502'; // '│'
        } else {
          prefix = u'\u251c'; // '├'
        }
      } else if (i == firstLine) {
        if (i == lastLine) {
          prefix = u'\u2500'; // '─'
        } else {
          prefix = u'\u250c'; // '┌'
        }
      } else if (i == lastLine) {
        prefix = u'\u2514'; // '└'
      }
      tree[i][0] = prefix + tree[i][0];
    }
    return tree;
  }
}
