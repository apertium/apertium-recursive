#include <rtx_config.h>
#include <pattern.h>
#include <bytecode.h>

#include <lttoolbox/compression.h>
#include <apertium/string_utils.h>
#include <lttoolbox/input_file.h>

#include <iostream>
#include <fstream>

using namespace std;

PatternBuilder::PatternBuilder()
{
  alphabet.includeSymbol("<ANY_TAG>"_u);
  alphabet.includeSymbol("<ANY_CHAR>"_u);
  alphabet.includeSymbol("<LOOK:AHEAD>"_u);
  attr_items["lem"_u] = "^(([^<]|\"\\<\")+)"_u;
  attr_items["lemq"_u] = "\\#[- _][^<]+"_u;
  attr_items["lemh"_u] = "^(([^<#]|\"\\<\"|\"\\#\")+)"_u;
  attr_items["whole"_u] = "(.+)"_u;
  attr_items["tags"_u] = "((<[^>]+>)+)"_u;
  attr_items["chname"_u] = "(\\{([^/]+)\\/)"_u; // includes delimiters { and / !!!
  attr_items["chcontent"_u] = "(\\{.+)"_u;
  attr_items["content"_u] = "(\\{.+)"_u;
  attr_items["pos_tag"_u] = "(<[^>]+>)"_u;
  starCanBeEmpty = false;
  chunkVarCount = 0;
}

int
PatternBuilder::insertLemma(int const base, UString const &lemma)
{
  int retval = base;
  static int const any_char = alphabet("<ANY_CHAR>"_u);
  if(lemma.empty())
  {
    retval = transducer.insertSingleTransduction(any_char, retval);
    transducer.linkStates(retval, retval, any_char);
  }
  else
  {
    for(unsigned int i = 0, limit = lemma.size();  i != limit; i++)
    {
      if(lemma[i] == '\\')
      {
        //retval = transducer.insertSingleTransduction('\\', retval);
        i++;
        retval = transducer.insertSingleTransduction(int(lemma[i]),
                                                             retval);
      }
      else if(lemma[i] == '*')
      {
        retval = transducer.insertSingleTransduction(any_char, retval);
        transducer.linkStates(retval, retval, any_char);
      }
      else
      {
        retval = transducer.insertSingleTransduction(int(lemma[i]),
                                                             retval);
      }
    }
  }

  return retval;
}

int
PatternBuilder::insertTags(int const base, const vector<UString>& tags)
{
  int retval = base;
  static int const any_tag = alphabet("<ANY_TAG>"_u);
  for(unsigned int i = 0; i < tags.size(); i++)
  {
    if(tags[i] == "*"_u)
    {
      if(!starCanBeEmpty)
      {
        retval = transducer.insertSingleTransduction(any_tag, retval);
      }
      transducer.linkStates(retval, retval, any_tag);
    }
    else
    {
      vector<UString> tgs = StringUtils::split_UString(tags[i], "."_u);
      for(auto t : tgs)
      {
        UString tg = "<"_u + t + ">"_u;
        alphabet.includeSymbol(tg);
        retval = transducer.insertSingleTransduction(alphabet(tg), retval);
      }
    }
  }
  return retval;
}

int
PatternBuilder::countToFinalSymbol(const int count)
{
  const UString count_sym = "<RULE_NUMBER:"_u + StringUtils::itoa(count) + ">"_u;
  alphabet.includeSymbol(count_sym);
  const int symbol = alphabet(count_sym);
  if(count != -1) final_symbols.insert(symbol);
  return symbol;
}

vector<PatternBuilder::TrieNode*>
PatternBuilder::buildTrie(vector<UString> parts)
{
  vector<TrieNode*> ret;
  vector<vector<UString>> p2;
  for(auto p : parts)
  {
    if(p.size() == 0) continue;
    bool found = false;
    for(unsigned int t = 0; t < p2.size(); t++)
    {
      if(ret[t]->self == p[0])
      {
        p2[t].push_back(p.substr(1));
        found = true;
        break;
      }
    }
    if(!found)
    {
      TrieNode* t = new TrieNode;
      t->self = p[0];
      ret.push_back(t);
      p2.push_back(vector<UString>(1, p.substr(1)));
    }
  }
  for(unsigned int i = 0; i < ret.size(); i++)
  {
    ret[i]->next = buildTrie(p2[i]);
  }
  return ret;
}

UString
PatternBuilder::unbuildTrie(PatternBuilder::TrieNode* t)
{
  if(t->self == '\0') return ""_u;
  UString single;
  bool end = false;
  vector<UString> groups;
  int ct = t->next.size();
  for(auto it : t->next)
  {
    UString blob = unbuildTrie(it);
    if(blob.size() == 0)
    {
      end = true;
      ct--;
    }
    else if(blob.size() == 1)
    {
      if(single.size() > 0) ct--;
      single += blob;
    }
    else groups.push_back(blob);
  }
  UString ret;
  if(t->self == '#') ret += '\\';
  ret += t->self;
  if(single.size() == 0 && groups.size() == 0) return ret;
  if(single.size() > 1) single = "["_u + single + "]"_u;
  if(ct > 1 || (groups.size() == 1 && end)) ret += "(?:"_u;
  for(unsigned int i = 0; i < groups.size(); i++)
  {
    if(i > 0) ret += '|';
    ret += groups[i];
  }
  if(single.size() > 0)
  {
    if(groups.size() > 0) ret += '|';
    ret += single;
  }
  if(ct > 1 || (groups.size() == 1 && end)) ret += ')';
  if(end) ret += '?';
  return ret;
}

UString
PatternBuilder::trie(vector<UString> parts)
{
  if(parts.size() == 0) return ""_u;
  for(unsigned int i = 0; i < parts.size(); i++)
  {
    parts[i] = "<"_u + parts[i];
    parts[i] += '\0';
  }
  vector<TrieNode*> l = buildTrie(parts);
  // they all start with '<', so there will only be 1.
  return "("_u + unbuildTrie(l[0]) + ">)"_u;
}

void
PatternBuilder::addPattern(vector<vector<PatternElement*>> pat, int rule, double weight, bool isLex)
{
  int state = transducer.getInitial();
  for(unsigned int p = 0; p < pat.size(); p++)
  {
    if(p != 0)
    {
      if(!isLex)
      {
        // lookahead on lexicalization paths ought to be redundant
        // since anything matching a lex path should also match the general path
        for(auto pe : pat[p])
        {
          if(pe->tags.size() > 0) lookahead[state].push_back(pe->tags[0]);
        }
      }
      state = transducer.insertSingleTransduction(' ', state);
    }
    state = transducer.insertNewSingleTransduction('^', state);
    int end = insertLemma(state, pat[p][0]->lemma);
    end = insertTags(end, pat[p][0]->tags);
    end = transducer.insertSingleTransduction('$', end);
    for(unsigned int i = 1; i < pat[p].size(); i++)
    {
      int temp = insertLemma(state, pat[p][i]->lemma);
      temp = insertTags(temp, pat[p][i]->tags);
      transducer.linkStates(temp, end, '$');
    }
    state = end;
  }
  int symbol = countToFinalSymbol(rule);
  state = transducer.insertSingleTransduction(symbol, state, weight);
  transducer.setFinal(state);
}

void
PatternBuilder::addRule(int rule, double weight, vector<vector<PatternElement*>> pattern, vector<UString> firstChunk, UString name)
{
  rules[rule] = make_pair(firstChunk, pattern);
  addPattern(pattern, rule, weight, false);
  for(auto it : lexicalizations[name])
  {
    if(it.second.size() == pattern.size())
    {
      addPattern(it.second, rule, it.first, true);
    }
  }
  for(auto left : firstChunk)
  {
    for(auto right : pattern[0])
    {
      firstSet[left].insert(right->tags[0]);
    }
  }
}

void
PatternBuilder::addList(UString name, set<UString> vals)
{
  lists[name] = vals;
}

void
PatternBuilder::addAttr(UString name, set<UString> vals)
{
  vector<UString> pat;
  for(auto it : vals)
  {
    UString p = StringUtils::substitute(it, "\\."_u, "<>"_u);
    p = StringUtils::substitute(p, "."_u, "><"_u);
    pat.push_back(StringUtils::substitute(p, "<>"_u, "\\."_u));
  }
  UString pt = trie(pat);
  attr_items[name] = pt;
}

bool
PatternBuilder::isAttrDefined(UString name)
{
  return attr_items.find(name) != attr_items.end();
}

void
PatternBuilder::addVar(UString name, UString val)
{
  variables[name] = val;
}

UString
PatternBuilder::BCstring(const UString& s)
{
  UString ret;
  ret += STRING;
  ret += (UChar)s.size();
  ret += s;
  return ret;
}

UString
PatternBuilder::BCifthenelse(const UString& cond, const UString& yes, const UString& no)
{
  UString ret = cond;
  if(yes.size() == 0)
  {
    ret += JUMPONTRUE;
    ret += (UChar)no.size();
    ret += no;
  }
  else if(no.size() == 0)
  {
    ret += JUMPONFALSE;
    ret += (UChar)yes.size();
    ret += yes;
  }
  else
  {
    ret += JUMPONFALSE;
    ret += (UChar)(yes.size() + 2);
    ret += yes;
    ret += JUMP;
    ret += (UChar)no.size();
    ret += no;
  }
  return ret;
}

void
PatternBuilder::buildLookahead()
{
  for(auto it : firstSet)
  {
    firstSet[it.first].insert(it.first);
    vector<UString> todo;
    for(auto op : it.second) todo.push_back(op);
    while(todo.size() > 0)
    {
      UString cur = todo.back();
      todo.pop_back();
      if(cur != it.first && firstSet.find(cur) != firstSet.end())
      {
        for(auto other : firstSet[cur])
        {
          if(firstSet[it.first].find(other) == firstSet[it.first].end())
          {
            firstSet[it.first].insert(other);
            todo.push_back(other);
          }
        }
      }
    }
  }
  for(auto it : lookahead)
  {
    int state = transducer.insertSingleTransduction(alphabet("<LOOK:AHEAD>"_u), it.first);
    state = transducer.insertSingleTransduction('^', state);
    transducer.linkStates(state, state, alphabet("<ANY_CHAR>"_u));
    int end = -1;
    for(auto next : it.second)
    {
      if(firstSet.find(next) == firstSet.end()) firstSet[next].insert(next);
      for(auto tag : firstSet[next])
      {
        int temp = state;
        if(tag != "*"_u)
        {
          temp = transducer.insertSingleTransduction(alphabet("<"_u + tag + ">"_u), temp);
        }
        transducer.linkStates(temp, temp, alphabet("<ANY_TAG>"_u));
        if(end == -1)
        {
          end = transducer.insertSingleTransduction('$', temp);
          transducer.setFinal(end);
        }
        else
        {
          transducer.linkStates(temp, end, '$');
        }
      }
    }
  }
}

bool
PatternBuilder::isPrefix(const vector<vector<PatternElement*>>& rule, const vector<vector<PatternElement*>>& prefix)
{
  if(prefix.size() >= rule.size()) return false;
  for(unsigned int i = 0; i < prefix.size(); i++)
  {
    bool found = false;
    for(auto r : rule[i])
    {
      if(r->tags.size() == 0) continue;
      else if(r->tags[0] == "*"_u)
      {
        found = true;
        break;
      }
      for(auto p : prefix[i])
      {
        if(p->tags.size() == 0) continue;
        else if(p->tags[0] == "*"_u || p->tags[0] == r->tags[0])
        {
          found = true;
          break;
        }
      }
      if(found) break;
    }
    if(!found) return false;
  }
  return true;
}

void
PatternBuilder::buildFallback()
{
  bool starWas = starCanBeEmpty;
  starCanBeEmpty = true;
  vector<PatternElement*> fallback;
  PatternElement* fall = new PatternElement;
  fall->tags.push_back("FALL:BACK"_u);
  fallback.push_back(fall);
  for(auto rule : rules)
  {
    vector<PatternElement*> result;
    for(auto tg : rule.second.first)
    {
      PatternElement* pe = new PatternElement;
      pe->tags.push_back(tg);
      pe->tags.push_back("*"_u);
      result.push_back(pe);
    }
    vector<vector<PatternElement*>> resultPat;
    resultPat.push_back(result);
    set<UString> patPrefix;
    set<UString> resultPrefix;
    for(auto rule2 : rules)
    {
      if(isPrefix(rule2.second.second, resultPat))
      {
        for(auto it : rule2.second.second[1])
        {
          if(it->tags.size() > 0) resultPrefix.insert(it->tags[0]);
        }
      }
      if(rule2.second.second.size() > rule.second.second.size() + 1 &&
         isPrefix(rule2.second.second, rule.second.second))
      {
        for(auto it : rule2.second.second[rule.second.second.size()])
        {
          if(it->tags.size() > 0) patPrefix.insert(it->tags[0]);
        }
      }
    }
    for(auto it : resultPrefix)
    {
      patPrefix.erase(it);
    }
    if(patPrefix.size() > 0)
    {
      vector<PatternElement*> add;
      for(auto it : patPrefix)
      {
        PatternElement* pe = new PatternElement;
        pe->tags.push_back(it);
        pe->tags.push_back("*"_u);
        add.push_back(pe);
      }
      resultPat.push_back(add);
      resultPat.push_back(fallback);
      addPattern(resultPat, -1, 0, false);
      for(auto pe : add) delete pe;
    }
    for(auto pe : result) delete pe;
  }
  delete fall;
  starCanBeEmpty = starWas;
}

void
PatternBuilder::loadLexFile(const string& fname)
{
  InputFile lex;
  lex.open_or_exit(fname.c_str());
  while(!lex.eof()) {
    UString name;
    while(!lex.eof() && lex.peek() != '\t') name += lex.get();
    lex.get();
    UString weight;
    while(!lex.eof() && lex.peek() != '\t') weight += lex.get();
    lex.get();
    if(lex.eof()) break;
    vector<vector<PatternElement*>> pat;
    while(!lex.eof() && lex.peek() != '\n') {
      PatternElement* p = new PatternElement;
      while(lex.peek() != '@') p->lemma += u_tolower(lex.get());
      lex.get();
      UString tag;
      while(lex.peek() != ' ' && lex.peek() != '\n') {
        if(lex.peek() == '.') {
          lex.get();
          p->tags.push_back(tag);
          tag.clear();
        }
        else tag += lex.get();
      }
      p->tags.push_back(tag);
      if(lex.peek() == ' ') lex.get();
      pat.push_back(vector<PatternElement*>(1, p));
    }
    lex.get();
    lexicalizations[name].push_back(make_pair(stod(weight), pat));
  }
}

void
PatternBuilder::write(FILE* output, int longest, vector<pair<int, UString>> inputBytecode, vector<UString> outputBytecode)
{
  Compression::multibyte_write(longest, output);
  Compression::multibyte_write(inputBytecode.size(), output);
  for(unsigned int i = 0; i < inputBytecode.size(); i++)
  {
    Compression::multibyte_write(inputBytecode[i].first, output);
    Compression::string_write(inputBytecode[i].second, output);
  }

  Compression::multibyte_write(outputBytecode.size(), output);
  for(unsigned int i = 0; i < outputBytecode.size(); i++)
  {
    Compression::string_write(outputBytecode[i], output);
  }

  Compression::multibyte_write(chunkVarCount, output);

  buildFallback();
  buildLookahead();

  alphabet.write(output);

  transducer.minimize();
  map<int, double> old_finals = transducer.getFinals(); // copy for later removal
  multimap<int, pair<int, double>> finals_rules; // node id -> rule number
  map<int, multimap<int, pair<int, double> > >& transitions = transducer.getTransitions();
  // Find all arcs with "final_symbols" in the transitions, let their source node instead be final,
  // and extract the rule number from the arc. Record relation between source node and rule number
  // in finals_rules. It is now no longer safe to minimize -- but we already did that.
  const UString rule_sym_pre = "<RULE_NUMBER:"_u; // see countToFinalSymbol()
  for(map<int, multimap<int, pair<int, double> > >::const_iterator it = transitions.begin(),
        limit = transitions.end(); it != limit; ++it)
  {
    const int src = it->first;
    for(multimap<int, pair<int, double> >::const_iterator arc = it->second.begin(),
          arclimit = it->second.end(); arc != arclimit; ++arc)
    {
      const int symbol = arc->first;
      const double wgt = arc->second.second;
      if(final_symbols.count(symbol) == 0) {
        continue;
      }
      // Extract the rule number encoded by countToFinalSymbol():
      UString s;
      alphabet.getSymbol(s, symbol);
      if(s.compare(0, rule_sym_pre.size(), rule_sym_pre) != 0) {
        continue;
      }
      const int rule_num = stoi(s.substr(rule_sym_pre.size()));
      transducer.setFinal(src);
      finals_rules.insert(make_pair(src, make_pair(rule_num, wgt)));
    }
  }
  // Remove the old finals:
  for(map<int, double>::const_iterator it = old_finals.begin(), limit = old_finals.end();
      it != limit; ++it)
  {
    transducer.setFinal(it->first, it->second, false);
  }

  transducer.write(output, alphabet.size());

  // finals_rules

  Compression::multibyte_write(finals_rules.size(), output);
  for(auto it : finals_rules)
  {
    Compression::multibyte_write(it.first, output);
    Compression::multibyte_write(it.second.first, output);
    Compression::long_multibyte_write(it.second.second, output);
  }

  // attr_items

  // empty version number since we're not on PCRE anymore
  Compression::multibyte_write(0, output);
  Compression::multibyte_write(attr_items.size(), output);

  for (auto& it : attr_items) {
    Compression::string_write(it.first, output);
    Compression::multibyte_write(0, output); // empty binary form of regex
    Compression::string_write(it.second, output);
  }

  // variables
  Compression::multibyte_write(variables.size(), output);
  for (auto& it : variables) {
    Compression::string_write(it.first, output);
    Compression::string_write(it.second, output);
  }

  // lists
  Compression::multibyte_write(lists.size(), output);
  for (auto& it : lists) {
    Compression::string_write(it.first, output);
    Compression::multibyte_write(it.second.size(), output);

    for (auto& it2 : it.second) {
      Compression::string_write(it2, output);
    }
  }

  // rule names
  Compression::multibyte_write(inRuleNames.size(), output);
  for (auto& name : inRuleNames) {
    Compression::string_write(name, output);
  }
  Compression::multibyte_write(outRuleNames.size(), output);
  for (auto& name : outRuleNames) {
    Compression::string_write(name, output);
  }

}
