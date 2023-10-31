#include <rtx_config.h>
#include <lttoolbox/transducer.h>
#include <lttoolbox/alphabet.h>
#include <lttoolbox/compression.h>
#include <lttoolbox/lt_locale.h>
#include <random>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <lttoolbox/i18n.h>

using namespace std;

Alphabet A;
Transducer T;
UString prefix;
vector<pair<int, UString>> paths;
unsigned int donecount = 0;

bool load(FILE* input)
{
  // based on lttoolbox/fst_processor.cc
  fpos_t pos;
  if (fgetpos(input, &pos) == 0) {
      char header[4]{};
      size_t count = fread(header, 1, 4, input);
      if (count == 4 && strncmp(header, HEADER_LTTOOLBOX, 4) == 0) {
          auto features = read_le<uint64_t>(input);
          if (features >= LTF_UNKNOWN) {
              I18n(ARC_I18N_DATA, "arc").error("ARC80010", true);
          }
      }
      else {
          // Old binary format
          fsetpos(input, &pos);
      }
  }

  // letters
  int len = Compression::multibyte_read(input);
  while(len > 0)
  {
    Compression::multibyte_read(input);
    len--;
  }

  // symbols
  A.read(input);

  len = Compression::multibyte_read(input);

  if(len == 0) return false;

  while(len > 0)
  {
    UString name = Compression::string_read(input);
    T.read(input);
    len--;
    return true;
  }
  return true;
}

void followPath(int idx)
{
  if(paths[idx].second.size() > 100)
  {
    paths.erase(paths.begin() + idx);
    return;
  }
  int state = paths[idx].first;
  while(!T.isFinal(state))
  {
    vector<pair<int, int>> ops;
    for(auto tr : T.getTransitions()[state])
    {
      ops.push_back(make_pair(A.decode(tr.first).first, tr.second.first));
      if(paths.size() + ops.size() >= 100) break;
    }
    if(paths[idx].second.size() > 100)
    {
      paths.erase(paths.begin() + idx);
      return;
    }
    if(ops.size() == 0)
    {
      paths.erase(paths.begin() + idx);
      return;
    }
    for(unsigned int i = 1; i < ops.size(); i++)
    {
      paths.push_back(make_pair(ops[i].second, paths[idx].second));
      A.getSymbol(paths.back().second, ops[i].first);
      if(paths.back().second.size() > 0 && paths.back().second.back() == '+')
      {
        paths.pop_back();
      }
    }
    state = ops[0].second;
    A.getSymbol(paths[idx].second, ops[0].first);
    if(paths[idx].second.size() > 0 && paths[idx].second.back() == '+')
    {
      paths.erase(paths.begin() + idx);
      return;
    }
  }
  paths[idx].first = -1;
  donecount++;
}

void generatePaths()
{
  set<int> states = T.closure(T.getInitial());
  for(unsigned int i = 0; i < prefix.size(); i++)
  {
    int sym = prefix[i];
    int sym2 = u_tolower(prefix[i]);
    if(prefix[i] == '<')
    {
      for(unsigned int j = i+1; j < prefix.size(); j++)
      {
        if(prefix[j] == '>')
        {
          sym = A(prefix.substr(i, j-i+1));
          i = j;
          break;
        }
      }
    }
    set<int> newstates, temp;
    for(auto s : states)
    {
      for(auto tr : T.getTransitions()[s])
      {
        if(tr.first == sym || tr.first == sym2 ||
            A.decode(tr.first).first == sym || A.decode(tr.first).first == sym2)
        {
          temp = T.closure(tr.second.first);
          newstates.insert(temp.begin(), temp.end());
        }
      }
    }
    states.swap(newstates);
    newstates.clear();
  }
  for(auto s : states)
  {
    paths.push_back(make_pair(s, ""_u));
    followPath(paths.size() - 1);
  }
  while(donecount < paths.size())
  {
    for(unsigned int i = 0; i < paths.size(); i++)
    {
      if(paths[i].first != -1)
      {
        followPath(i);
        break;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();
  if(argc != 3)
  {
    cerr << I18n(ARC_I18N_DATA, "arc").format("randpath_desc", {"program"}, {argv[0]});
    return EXIT_FAILURE;
  }
  FILE* tf = fopen(argv[1], "rb");
  if(tf == NULL)
  {
    I18n(ARC_I18N_DATA, "arc").error("ARC80020", {"file"}, {argv[1]}, true);
  }
  if(!load(tf))
  {
    I18n(ARC_I18N_DATA, "arc").error("ARC80030", true);
  }
  prefix = to_ustring(argv[2]);
  generatePaths();
  if(paths.size() == 0)
  {
    I18n(ARC_I18N_DATA, "arc").error("ARC80040", true);
  }
  //seed_seq s (prefix.begin(), prefix.end());
  unsigned s = chrono::system_clock::now().time_since_epoch().count();
  minstd_rand0 g (s);
  cout << prefix << paths[g() % paths.size()].second << endl;
  return EXIT_SUCCESS;
}
