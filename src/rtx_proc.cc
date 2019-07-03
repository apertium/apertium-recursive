#include <rtx_processor.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>

void endProgram(char *name)
{
  cout << basename(name) << ": perform structural transfer" << endl;
  cout << "USAGE: " << basename(name) << " [ -m | -r | -s ] [-n] bytecode_file pattern_file [input_file [output_file]]" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -m, --matches:    print the steps of the pattern transducer" << endl;
  cout << "  -n, --no-coref:   treat stream as having no coreference LUs" << endl;
  cout << "  -r, --rules:      print the rules that are being applied" << endl;
  cout << "  -s, --steps:      print the instructions executed by the stack machine" << endl;
  cout << "  -t, --trx:        mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -h, --help:       show this help" << endl;
#else
  cout << "  -m:   print the steps of the pattern transducer" << endl;
  cout << "  -n:   treat stream as having no coreference LUs" << endl;
  cout << "  -r:   print the rules that are being applied" << endl;
  cout << "  -s:   print the instructions executed by the stack machine" << endl;
  cout << "  -t:   mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -h:   show this help" << endl;
#endif
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  RTXProcessor p;

#if HAVE_GETOPT_LONG
  static struct option long_options[]=
    {
      {"matches",           0, 0, 'm'},
      {"no-coref",          0, 0, 'n'},
      {"rules",             0, 0, 'r'},
      {"steps",             0, 0, 's'},
      {"trx",               0, 0, 't'},
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "mnrsth", long_options, &option_index);
#else
    int c = getopt(argc, argv, "mnrsth");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'm':
      p.printMatch(true);
      break;

    case 'n':
      p.withoutCoref(true);
      break;

    case 'r':
      p.printRules(true);
      break;

    case 's':
      p.printSteps(true);
      break;

    case 't':
      p.mimicChunker(true);
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  LtLocale::tryToSetLocale();

  if(optind > (argc - 2) || optind < (argc - 4))
  {
    endProgram(argv[0]);
  }

  p.read(argv[optind], argv[optind+1]);

  FILE *input = stdin, *output = stdout;

  if(optind <= (argc - 3))
  {
    input = fopen(argv[optind+2], "rb");
  }
  if(optind <= (argc - 4))
  {
    output = fopen(argv[optind+3], "wb");
  }

  p.process(input, output);

  fclose(input);
  fclose(output);
  return EXIT_SUCCESS;
}
