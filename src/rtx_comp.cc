#include <rtx_config.h>
#include <rtx_compiler.h>
#include <lttoolbox/lt_locale.h>
#include <cstdlib>
#include <iostream>
#include <apertium/string_utils.h>
#include <apertium/utf_converter.h>
#include <libgen.h>
#include <getopt.h>
#include <libxml/xmlreader.h>
#include <trx_compiler.h>

using namespace Apertium;
using namespace std;

void endProgram(char *name)
{
  cout << basename(name) << ": compile .rtx files" << endl;
  cout << "USAGE: " << basename(name) << " [-e name] [-f] [-l file] [-s] [-S] [-x] [-h] rule_file bytecode_file" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -e, --exclude:      exclude a rule by name" << endl;
  cout << "  -l, --lexical:      load a file of lexicalized weights" << endl;
  cout << "  -s, --summarize:    print rules to stderr as 'output -> pattern'" << endl;
  cout << "  -S, --stats:        print statistics about rule file to stdout" << endl;
  cout << "  -x, --xml:          rules are in xml format" << endl;
  cout << "  -h, --help:         show this help" << endl;
#else
  cout << "  -e:   exclude a rule by name" << endl;
  cout << "  -l:   load a file of lexicalized weights" << endl;
  cout << "  -s:   print rules to stderr as 'output -> pattern'" << endl;
  cout << "  -S:   print statistics about rule file to stdout" << endl;
  cout << "  -x:   rules are in xml format" << endl;
  cout << "  -h:   show this help" << endl;
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
      {"xml",               0, 0, 'x'},
      {"help",              0, 0, 'h'}
    };
#endif

  bool stats = false;
  bool summary = false;
  vector<wstring> exclude;
  vector<string> lexFiles;
  bool xml = false;

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "e:fl:sSxh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "e:fl:sSxh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'e':
      exclude.push_back(UtfConverter::fromUtf8(optarg));
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

    case 'x':
      xml = true;
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  if(argc - optind != 2) endProgram(argv[0]);

  if(xml)
  {
    TRXCompiler comp;
    if(summary)
    {
      cout << "Summary mode not available for XML." << endl;
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
