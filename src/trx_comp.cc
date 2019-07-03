#include <trx_compiler.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>
#include <vector>

void endProgram(char *name)
{
  cout << basename(name) << ": compile xml transfer rules to bytecode" << endl;
  cout << "USAGE: " << basename(name) << " [ -m | -r | -s ] [-n] bytecode_file pattern_file rule_files..." << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -m, --matches:    print the steps of the pattern transducer" << endl;
  cout << "  -n, --no-coref:   treat stream as having no coreference LUs" << endl;
  cout << "  -r, --rules:      print the rules that are being applied" << endl;
  cout << "  -s, --steps:      print the instructions executed by the stack machine" << endl;
  cout << "  -h, --help:       show this help" << endl;
#else
  cout << "  -m:   print the steps of the pattern transducer" << endl;
  cout << "  -n:   treat stream as having no coreference LUs" << endl;
  cout << "  -r:   print the rules that are being applied" << endl;
  cout << "  -s:   print the instructions executed by the stack machine" << endl;
  cout << "  -h:   show this help" << endl;
#endif
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  TRXCompiler p;

#if HAVE_GETOPT_LONG
  static struct option long_options[]=
    {
      {"matches",           0, 0, 'm'},
      {"no-coref",          0, 0, 'n'},
      {"rules",             0, 0, 'r'},
      {"steps",             0, 0, 's'},
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "mnrsh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "mnrsh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  LtLocale::tryToSetLocale();

  char* byte = argv[optind];
  char* bin = argv[optind+1];
  vector<string> files;
  for(int i = optind + 2; i < argc; i++)
  {
    files.push_back(argv[i]);
  }

  p.compile(files);
  p.write(bin, byte);

  return EXIT_SUCCESS;
}
