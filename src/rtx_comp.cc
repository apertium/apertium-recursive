#include <rtx_compiler.h>
#include <lttoolbox/lt_locale.h>
#include <cstdlib>
#include <iostream>
#include <apertium/string_utils.h>
#include <libgen.h>
#include <getopt.h>

using namespace Apertium;
using namespace std;

void endProgram(char *name)
{
  cout << basename(name) << ": compile .rtx files" << endl;
  cout << "USAGE: " << basename(name) << " [-f] [-s] rule_file bytecode_file" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -f, --no-fallback:  don't generate a default fallback rule" << endl;
  cout << "  -s, --summarize:    print rules to stderr as 'output -> pattern'" << endl;
  cout << "  -h, --help:         show this help" << endl;
#else
  cout << "  -f:   don't generate a default fallback rule" << endl;
  cout << "  -s:   print rules to stderr as 'output -> pattern'" << endl;
  cout << "  -h:   show this help" << endl;
#endif
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();

  RTXCompiler myCompiler;

#if HAVE_GETOPT_LONG
  static struct option long_options[]=
    {
      {"no-fallback",       0, 0, 'f'},
      {"summarize",         0, 0, 's'},
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "fsh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "fsh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'f':
      myCompiler.setFallback(false);
      break;

    case 's':
      myCompiler.setSummarizing(true);
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  if(argc - optind != 2) endProgram(argv[0]);

  myCompiler.read(argv[optind]);
  myCompiler.write(argv[optind+1]);

  return EXIT_SUCCESS;
}
