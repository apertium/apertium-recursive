#include <rtx_config.h>
#include <rtx_compiler.h>
#include <lttoolbox/lt_locale.h>
#include <cstdlib>
#include <iostream>
#include <libgen.h>
#include <getopt.h>
#include <libxml/xmlreader.h>
#include <trx_compiler.h>
#include <lttoolbox/i18n.h>

using namespace std;

void endProgram(char *name)
{
  I18n i18n {ARC_I18N_DATA, "arc"};
  cout << i18n.format("rtx_comp_desc", {"program"}, {basename(name)});
#if HAVE_GETOPT_LONG
  cout << "  -e, --exclude:      " << i18n.format("exclude_desc") << endl;
  cout << "  -l, --lexical:      " << i18n.format("lexical_desc") << endl;
  cout << "  -s, --summarize:    " << i18n.format("summarize_desc") << endl;
  cout << "  -S, --stats:        " << i18n.format("stats_desc") << endl;
  cout << "  -h, --help:         " << i18n.format("help_desc") << endl;
#else
  cout << "  -e:   " << i18n.format("exclude_desc") << endl;
  cout << "  -l:   " << i18n.format("lexical_desc") << endl;
  cout << "  -s:   " << i18n.format("summarize_desc") << endl;
  cout << "  -S:   " << i18n.format("stats_desc") << endl;
  cout << "  -h:   " << i18n.format("help_desc") << endl;
#endif
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();

#if HAVE_GETOPT_LONG
  static struct option long_options[]=
    {
      {"exclude",           1, 0, 'e'},
      {"lexical",           1, 0, 'l'},
      {"summarize",         0, 0, 's'},
      {"stats",             0, 0, 'S'},
      {"help",              0, 0, 'h'}
    };
#endif

  bool stats = false;
  bool summary = false;
  vector<UString> exclude;
  vector<string> lexFiles;

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "e:fl:sSh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "e:fl:sSh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'e':
      exclude.push_back(to_ustring(optarg));
      break;

    case 'l':
      lexFiles.push_back(optarg);
      break;

    case 's':
      summary = true;
      break;

    case 'S':
      stats = true;
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  if(argc - optind != 2) endProgram(argv[0]);

  FILE* check = fopen(argv[optind], "r");
  if(check == NULL)
  {
    I18n(ARC_I18N_DATA, "arc").error("ARC80020", {"file"}, {argv[optind]}, true);
  }
  int c;
  while((c = fgetc(check)) != '<')
  {
    if(c == EOF) break;
    else if(isspace(c)) continue;
    else break;
  }
  bool xml = (c == '<');
  fclose(check);

  if(xml)
  {
    TRXCompiler comp;
    if(summary)
    {
      I18n(ARC_I18N_DATA, "arc").error("ARC60050", false);
    }
    for(auto lex : lexFiles) comp.loadLex(lex);
    for(auto exc : exclude) comp.excludeRule(exc);
    comp.compile(argv[optind]);
    comp.write(argv[optind+1]);
    if(stats)
    {
      comp.printStats();
    }
  }
  else
  {
    RTXCompiler comp;
    comp.setSummarizing(summary);
    for(auto lex : lexFiles) comp.loadLex(lex);
    for(auto exc : exclude) comp.excludeRule(exc);
    comp.read(argv[optind]);
    comp.write(argv[optind+1]);
    if(stats)
    {
      comp.printStats();
    }
  }

  return EXIT_SUCCESS;
}
