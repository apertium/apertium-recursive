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

  int insertLemma(int const base, wstring const &lemma)
  {
    int retval = base;
    static int const any_char = td.getAlphabet()(L"<ANY_CHAR>");
    if(lemma == L"")
    {
      retval = td.getTransducer().insertSingleTransduction(any_char, retval);
      td.getTransducer().linkStates(retval, retval, any_char);
    }
    else
    {
      for(unsigned int i = 0, limit = lemma.size();  i != limit; i++)
      {
        if(lemma[i] == L'\\')
        {
          //retval = td.getTransducer().insertSingleTransduction(L'\\', retval);
          i++;
          retval = td.getTransducer().insertSingleTransduction(int(lemma[i]),
                                                               retval);
        }
        else if(lemma[i] == L'*')
        {
          retval = td.getTransducer().insertSingleTransduction(any_char, retval);
          td.getTransducer().linkStates(retval, retval, any_char);
        }
        else
        {
          retval = td.getTransducer().insertSingleTransduction(int(lemma[i]),
                                                               retval);
        }
      }
    }

    return retval;
  }

  int insertTags(int const base, const vector<wstring>& tags)
  {
    int retval = base;
    static int const any_tag = td.getAlphabet()(L"<ANY_TAG>");
    for(unsigned int i = 0; i < tags.size(); i++)
    {
      if(tags[i] == L"*")
      {
        retval = td.getTransducer().insertSingleTransduction(any_tag, retval);
        td.getTransducer().linkStates(retval, retval, any_tag);
      }
      else
      {
        wstring tg = L"<" + tags[i] + L">";
        td.getAlphabet().includeSymbol(tg);
        retval = td.getTransducer().insertSingleTransduction(td.getAlphabet()(tg), retval);
      }
    }
    return retval;
  }
public:
  PatternBuilder()
  {
    td.getAlphabet().includeSymbol(L"<ANY_TAG>");
    td.getAlphabet().includeSymbol(L"<ANY_CHAR>");
  }

  int addPattern(vector<vector<PatternElement*>> pat, int rule, double weight = 0.0)
  {
    int state = td.getTransducer().getInitial();
    for(unsigned int p = 0; p < pat.size(); p++)
    {
      if(p != 0)
      {
        state = td.getTransducer().insertSingleTransduction(L' ', state);
      }
      state = td.getTransducer().insertSingleTransduction(L'^', state);
      int end = insertLemma(state, pat[p][0]->lemma);
      end = insertTags(end, pat[p][0]->tags);
      end = td.getTransducer().insertSingleTransduction(L'$', end);
      for(unsigned int i = 1; i < pat[p].size(); i++)
      {
        int temp = insertLemma(state, pat[p][i]->lemma);
        temp = insertTags(temp, pat[p][i]->tags);
        td.getTransducer().linkStates(temp, end, L'$');
      }
      state = end;
    }
    if(td.seen_rules.find(state) == td.seen_rules.end())
    {
      td.seen_rules[state] = rule;
      int symbol = td.countToFinalSymbol(rule);
      state = td.getTransducer().insertSingleTransduction(symbol, state, weight);
      td.getTransducer().setFinal(state);
      return -1;
    }
    else
    {
      return td.seen_rules[state];
    }
  }

  void addList(wstring name, set<wstring, Ltstr> vals)
  {
    td.getLists()[name] = vals;
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
    td.getAttrItems()[name] = pat;
  }
  void addVar(wstring name, wstring val)
  {
    td.getVariables()[name] = val;
  }
  void write(FILE* output)
  {
    td.write(output);
  }
};

#endif
