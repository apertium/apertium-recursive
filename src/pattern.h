#ifndef __RTXPATTERNBUILDER__
#define __RTXPATTERNBUILDER__

#include <lttoolbox/compression.h>
#include <iostream>
#include <apertium/string_utils.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/ltstr.h>
#include <lttoolbox/transducer.h>

#include <string>
#include <vector>
#include <map>

#include <apertium/apertium_re.h>
#include <apertium/utf_converter.h>

using namespace std;

struct PatternElement
{
  wstring lemma;
  vector<wstring> tags;
};

class PatternBuilder
{
private:
  map<wstring, wstring, Ltstr> attr_items;
  map<wstring, set<wstring, Ltstr>, Ltstr> lists;
  map<wstring, wstring, Ltstr> variables;
  set<int> final_symbols;

  map<int, int> seen_rules;

  Alphabet alphabet;
  Transducer transducer;
  Transducer attributes;
  map<int, wstring> attr_vals;
  set<wstring, Ltstr> all_attrs;

  int insertLemma(int const base, wstring const &lemma)
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

  int insertTags(int const base, const vector<wstring>& tags)
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
  int countToFinalSymbol(const int count) {
    const wstring count_sym = L"<RULE_NUMBER:" + to_wstring(count) + L">";
    alphabet.includeSymbol(count_sym);
    const int symbol = alphabet(count_sym);
    final_symbols.insert(symbol);
    return symbol;
  }
public:
  // false: * = 1 or more tags, true: * = 0 or more tags
  bool starCanBeEmpty;
  vector<wstring> inRuleNames;
  vector<wstring> outRuleNames;

  PatternBuilder()
  {
    alphabet.includeSymbol(L"<ANY_TAG>");
    alphabet.includeSymbol(L"<ANY_CHAR>");
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

  int addPattern(vector<vector<PatternElement*>> pat, int rule, double weight = 0.0)
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
    else if(rule != -1 && seen_rules.find(state) == seen_rules.end())
    {
      seen_rules[state] = rule;
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

  void addList(wstring name, set<wstring, Ltstr> vals)
  {
    lists[name] = vals;
  }
  void addAttr(wstring name, set<wstring, Ltstr> vals)
  {
    wstring pat = L"(";
    for(set<wstring, Ltstr>::iterator it = vals.begin(); it != vals.end(); it++)
    {
      if(pat.size() > 1)
      {
        pat += L"|";
      }
      pat += L"<" + StringUtils::substitute(*it, L".", L"><") + L">";
    }
    pat += L")";
    attr_items[name] = pat;

    alphabet.includeSymbol(L"<" + name + L">");
    int sym = alphabet(L"<" + name + L">");
    int start = attributes.insertSingleTransduction(sym, attributes.getInitial());
    attributes.linkStates(start, start, alphabet(L"<ANY_CHAR>"));
    for(set<wstring, Ltstr>::iterator it = vals.begin(), limit = vals.end();
            it != limit; it++)
    {
      int loc = start;
      wstring s = L"<" + StringUtils::substitute(*it, L".", L"><") + L">";
      for(unsigned int i = 0; i < s.size(); i++)
      {
        loc = attributes.insertSingleTransduction(s[i], loc);
      }
      attributes.setFinal(loc);
      attr_vals[loc] = s;
      all_attrs.insert(s);
    }
  }
  void addVar(wstring name, wstring val)
  {
    variables[name] = val;
  }
  void write(FILE* output, int longest, vector<pair<int, wstring>> inputBytecode, vector<wstring> outputBytecode)
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


    map<wstring, int> attr_loc;
    Compression::multibyte_write(all_attrs.size(), output);
    int loc = 0;
    for(set<wstring, Ltstr>::const_iterator it = all_attrs.begin(), limit = all_attrs.end();
          it != limit; it++)
    {
      Compression::wstring_write(*it, output);
      attr_loc[*it] = loc++;
    }
    attributes.write(output, alphabet.size());
    Compression::multibyte_write(attr_vals.size(), output);
    for(map<int, wstring>::const_iterator it = attr_vals.begin(), limit = attr_vals.end();
        it != limit; it++)
    {
      Compression::multibyte_write(it->first, output);
      Compression::multibyte_write(attr_loc[it->second], output);
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
};

#endif
