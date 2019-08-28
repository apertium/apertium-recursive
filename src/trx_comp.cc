#include <rtx_config.h>
#include <trx_compiler.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>
#include <libgen.h>
#include <vector>

void endProgram(char *name)
{
  cout << basename(name) << ": compile xml transfer rules to bytecode" << endl;
  cout << "USAGE: " << basename(name) << " [-l file] bytecode_file rule_files..." << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -l, --lexical:    load a file of lexicalized weights" << endl;
  cout << "  -h, --help:       show this help" << endl;
#else
  cout << "  -l:   load a file of lexicalized weights" << endl;
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
      {"lexical",           1, 0, 'l'},
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "l:h", long_options, &option_index);
#else
    int c = getopt(argc, argv, "l:h");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'l':
      p.loadLex(optarg);
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  LtLocale::tryToSetLocale();

  if(argc - optind < 2) endProgram(argv[0]);

  char* bin = argv[optind];
  vector<string> files;
  for(int i = optind + 1; i < argc; i++)
  {
    files.push_back(argv[i]);
  }

  p.compile(files);
  p.write(bin);

  return EXIT_SUCCESS;
}
