#include <pattern.h>

#include <lttoolbox/compression.h>
#include <apertium/string_utils.h>
#include <apertium/apertium_re.h>
#include <apertium/utf_converter.h>

using namespace std;

PatternBuilder::PatternBuilder()
{
  alphabet.includeSymbol(L"<ANY_TAG>");
  alphabet.includeSymbol(L"<ANY_CHAR>");
  alphabet.includeSymbol(L"<LOOK:AHEAD>");
  attr_items[L"lem"] = L"^(([^<]|\"\\<\")+)";
  attr_items[L"lemq"] = L"\\#[- _][^<]+";
  attr_items[L"lemh"] = L"^(([^<#]|\"\\<\"|\"\\#\")+)";
  attr_items[L"whole"] = L"(.+)";
  attr_items[L"tags"] = L"((<[^>]+>)+)";
  attr_items[L"chname"] = L"({([^/]+)\\/)"; // includes delimiters { and / !!!
  attr_items[L"chcontent"] = L"(\\{.+)";
  attr_items[L"content"] = L"(\\{.+)";
  attr_items[L"pos_tag"] = L"(<[^>]+>)";
  starCanBeEmpty = false;
}

int
PatternBuilder::insertLemma(int const base, wstring const &lemma)
{
  int retval = base;
  static int const any_char = alphabet(L"<ANY_CHAR>");
  if(lemma == L"")
  {
    retval = transducer.insertSingleTransduction(any_char, retval);
    transducer.linkStates(retval, retval, any_char);
  }
  else
  {
    for(unsigned int i = 0, limit = lemma.size();  i != limit; i++)
    {
      if(lemma[i] == L'\\')
      {
        //retval = transducer.insertSingleTransduction(L'\\', retval);
        i++;
        retval = transducer.insertSingleTransduction(int(lemma[i]),
                                                             retval);
      }
      else if(lemma[i] == L'*')
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
PatternBuilder::insertTags(int const base, const vector<wstring>& tags)
{
  int retval = base;
  static int const any_tag = alphabet(L"<ANY_TAG>");
  for(unsigned int i = 0; i < tags.size(); i++)
  {
    if(tags[i] == L"*")
    {
      if(!starCanBeEmpty)
      {
        retval = transducer.insertSingleTransduction(any_tag, retval);
      }
      transducer.linkStates(retval, retval, any_tag);
    }
    else
    {
      wstring tg = L"<" + tags[i] + L">";
      alphabet.includeSymbol(tg);
      retval = transducer.insertSingleTransduction(alphabet(tg), retval);
    }
  }
  return retval;
}

int
PatternBuilder::countToFinalSymbol(const int count)
{
  const wstring count_sym = L"<RULE_NUMBER:" + to_wstring(count) + L">";
  alphabet.includeSymbol(count_sym);
  const int symbol = alphabet(count_sym);
  final_symbols.insert(symbol);
  return symbol;
}

vector<PatternBuilder::TrieNode*>
PatternBuilder::buildTrie(vector<wstring> parts)
{
  vector<TrieNode*> ret;
  vector<vector<wstring>> p2;
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
      p2.push_back(vector<wstring>(1, p.substr(1)));
    }
  }
  for(unsigned int i = 0; i < ret.size(); i++)
  {
    ret[i]->next = buildTrie(p2[i]);
  }
  return ret;
}

wstring
PatternBuilder::unbuildTrie(PatternBuilder::TrieNode* t)
{
  if(t->self == L'\0') return L"";
  wstring single;
  bool end = false;
  vector<wstring> groups;
  int ct = t->next.size();
  for(auto it : t->next)
  {
    wstring blob = unbuildTrie(it);
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
  wstring ret;
  ret += t->self;
  if(single.size() == 0 && groups.size() == 0) return ret;
  if(single.size() > 1) single = L"[" + single + L"]";
  if(ct > 1 || (groups.size() == 1 && end)) ret += L"(?:";
  for(unsigned int i = 0; i < groups.size(); i++)
  {
    if(i > 0) ret += L"|";
    ret += groups[i];
  }
  if(single.size() > 0)
  {
    if(groups.size() > 0) ret += L"|";
    ret += single;
  }
  if(ct > 1 || (groups.size() == 1 && end)) ret += L")";
  if(end) ret += L"?";
  return ret;
}

wstring
PatternBuilder::trie(vector<wstring> parts)
{
  if(parts.size() == 0) return L"";
  for(unsigned int i = 0; i < parts.size(); i++)
  {
    parts[i] = L"<" + parts[i];
    parts[i] += L'\0';
  }
  vector<TrieNode*> l = buildTrie(parts);
  // they all start with L'<', so there will only be 1.
  return L"(" + unbuildTrie(l[0]) + L">)";
}

int
PatternBuilder::addPattern(vector<vector<PatternElement*>> pat, int rule, double weight)
{
  int state = transducer.getInitial();
  for(unsigned int p = 0; p < pat.size(); p++)
  {
    if(p != 0)
    {
      state = transducer.insertSingleTransduction(L' ', state);
    }
    state = transducer.insertNewSingleTransduction(L'^', state);
    int end = insertLemma(state, pat[p][0]->lemma);
    end = insertTags(end, pat[p][0]->tags);
    end = transducer.insertSingleTransduction(L'$', end);
    for(unsigned int i = 1; i < pat[p].size(); i++)
    {
      int temp = insertLemma(state, pat[p][i]->lemma);
      temp = insertTags(temp, pat[p][i]->tags);
      transducer.linkStates(temp, end, L'$');
    }
    state = end;
  }
  if(rule == -1)
  {
    transducer.setFinal(state);
    return -1;
  }
  else if(seen_rules.find(state) == seen_rules.end())
  {
    seen_rules[state] = rule;
    rules_to_states[rule].push_back(state);
    int symbol = countToFinalSymbol(rule);
    state = transducer.insertSingleTransduction(symbol, state, weight);
    transducer.setFinal(state);
    return -1;
  }
  else
  {
    return seen_rules[state];
  }
}

void
PatternBuilder::addList(wstring name, set<wstring, Ltstr> vals)
{
  lists[name] = vals;
}

void
PatternBuilder::addAttr(wstring name, set<wstring, Ltstr> vals)
{
  /*wstring pat = L"(";
  for(set<wstring, Ltstr>::iterator it = vals.begin(); it != vals.end(); it++)
  {
    if(pat.size() > 1)
    {
      pat += L"|";
    }
    pat += L"<" + StringUtils::substitute(*it, L".", L"><") + L">";
  }
  pat += L")";
  attr_items[name] = pat;*/
  vector<wstring> pat;
  for(auto it : vals)
  {
    wstring p = StringUtils::substitute(it, L"\\.", L"<>");
    p = StringUtils::substitute(p, L".", L"><");
    pat.push_back(StringUtils::substitute(p, L"<>", L"\\."));
  }
  wstring pt = trie(pat);
  //wcerr << name << "\t" << pt << endl;
  attr_items[name] = pt;
}

bool
PatternBuilder::isAttrDefined(wstring name)
{
  return attr_items.find(name) != attr_items.end();
}

void
PatternBuilder::addVar(wstring name, wstring val)
{
  variables[name] = val;
}

void
PatternBuilder::addLookahead(const int rule, const vector<PatternElement*>& options)
{
  // there's a lot of hardcodishness about this function
  if(options.size() == 0) return;
  for(auto s : rules_to_states[rule+1])
  {
    int state = s;
    state = transducer.insertSingleTransduction(alphabet(L"<LOOK:AHEAD>"), state);
    state = transducer.insertSingleTransduction(L'^', state);
    transducer.linkStates(state, state, alphabet(L"<ANY_CHAR>"));
    int end = transducer.insertSingleTransduction(alphabet(L"<" + options[0]->tags[0] + L">"), state);
    transducer.linkStates(end, end, alphabet(L"<ANY_TAG>"));
    end = transducer.insertSingleTransduction(L'$', end);
    for(unsigned int i = 1; i < options.size(); i++)
    {
      int temp = transducer.insertSingleTransduction(alphabet(L"<" + options[i]->tags[0] + L">"), state);
      transducer.linkStates(temp, temp, alphabet(L"<ANY_TAG>"));
      transducer.linkStates(temp, end, L'$');
    }
    transducer.setFinal(end);
  }
}

void
PatternBuilder::write(FILE* output, int longest, vector<pair<int, wstring>> inputBytecode, vector<wstring> outputBytecode)
{
  Compression::multibyte_write(longest, output);
  Compression::multibyte_write(inputBytecode.size(), output);
  for(unsigned int i = 0; i < inputBytecode.size(); i++)
  {
    Compression::multibyte_write(inputBytecode[i].first, output);
    Compression::wstring_write(inputBytecode[i].second, output);
  }

  Compression::multibyte_write(outputBytecode.size(), output);
  for(unsigned int i = 0; i < outputBytecode.size(); i++)
  {
    Compression::wstring_write(outputBytecode[i], output);
  }

  alphabet.write(output);

  transducer.minimize();
  map<int, double> old_finals = transducer.getFinals(); // copy for later removal
  map<int, int> finals_rules;                   // node id -> rule number
  map<int, multimap<int, pair<int, double> > >& transitions = transducer.getTransitions();
  // Find all arcs with "final_symbols" in the transitions, let their source node instead be final,
  // and extract the rule number from the arc. Record relation between source node and rule number
  // in finals_rules. It is now no longer safe to minimize -- but we already did that.
  const wstring rule_sym_pre = L"<RULE_NUMBER:"; // see countToFinalSymbol()
  for(map<int, multimap<int, pair<int, double> > >::const_iterator it = transitions.begin(),
        limit = transitions.end(); it != limit; ++it)
  {
    const int src = it->first;
    for(multimap<int, pair<int, double> >::const_iterator arc = it->second.begin(),
          arclimit = it->second.end(); arc != arclimit; ++arc)
    {
      const int symbol = arc->first;
      const int trg = arc->second.first;
      const double wgt = arc->second.second;
      if(final_symbols.count(symbol) == 0) {
        continue;
      }
      if(!transducer.isFinal(trg)) {
        continue;
      }
      // Extract the rule number encoded by countToFinalSymbol():
      wstring s;
      alphabet.getSymbol(s, symbol);
      if(s.compare(0, rule_sym_pre.size(), rule_sym_pre) != 0) {
        continue;
      }
      const int rule_num = stoi(s.substr(rule_sym_pre.size()));
      transducer.setFinal(src, wgt);
      finals_rules[src] = rule_num;
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
  for(map<int, int>::const_iterator it = finals_rules.begin(), limit = finals_rules.end();
      it != limit; it++)
  {
    Compression::multibyte_write(it->first, output);
    Compression::multibyte_write(it->second, output);
  }

  // attr_items

  // precompiled regexps
  Compression::string_write(string(pcre_version()), output);
  Compression::multibyte_write(attr_items.size(), output);

  map<wstring, wstring, Ltstr>::iterator it, limit;
  for(it = attr_items.begin(), limit = attr_items.end(); it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    ApertiumRE my_re;
    my_re.compile(UtfConverter::toUtf8(it->second));
    my_re.write(output);
    Compression::wstring_write(it->second, output);
  }

  // variables
  Compression::multibyte_write(variables.size(), output);
  for(map<wstring, wstring, Ltstr>::const_iterator it = variables.begin(), limit = variables.end();
      it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    Compression::wstring_write(it->second, output);
  }

  // lists
  Compression::multibyte_write(lists.size(), output);
  for(map<wstring, set<wstring, Ltstr>, Ltstr>::const_iterator it = lists.begin(), limit = lists.end();
      it != limit; it++)
  {
    Compression::wstring_write(it->first, output);
    Compression::multibyte_write(it->second.size(), output);

    for(set<wstring, Ltstr>::const_iterator it2 = it->second.begin(), limit2 = it->second.end();
  it2 != limit2; it2++)
    {
      Compression::wstring_write(*it2, output);
    }
  }

  // rule names
  Compression::multibyte_write(inRuleNames.size(), output);
  for(unsigned int i = 0; i < inRuleNames.size(); i++)
  {
    Compression::wstring_write(inRuleNames[i], output);
  }
  Compression::multibyte_write(outRuleNames.size(), output);
  for(unsigned int i = 0; i < outRuleNames.size(); i++)
  {
    Compression::wstring_write(outRuleNames[i], output);
  }

}
