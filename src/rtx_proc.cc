#include <rtx_processor.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>

void endProgram(char *name)
{
  cout << basename(name) << ": perform structural transfer" << endl;
  cout << "USAGE: " << basename(name) << " [ -T | -b ] [ -m mode ] [-r] [-s] [-n] bytecode_file [input_file [output_file]]" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -b, --both:       print parse trees (as with -T) as well as text" << endl;
  cout << "  -m, --mode:       set the mode of tree output, options are 'flat', 'nest', 'latex'" << endl;
  cout << "  -n, --no-coref:   treat stream as having no coreference LUs" << endl;
  cout << "  -r, --rules:      print the rules that are being applied" << endl;
  cout << "  -s, --steps:      print the instructions executed by the stack machine" << endl;
  cout << "  -t, --trx:        mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -T, --tree:       print parse trees rather than apply output rules" << endl;
  cout << "  -h, --help:       show this help" << endl;
#else
  cout << "  -b:   print parse trees (as with -T) as well as text" << endl;
  cout << "  -m:   set the mode of tree output, options are 'flat', 'nest', 'latex'" << endl;
  cout << "  -n:   treat stream as having no coreference LUs" << endl;
  cout << "  -r:   print the rules that are being applied" << endl;
  cout << "  -s:   print the instructions executed by the stack machine" << endl;
  cout << "  -t:   mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -T:   print parse trees rather than apply output rules" << endl;
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
      {"both",              0, 0, 'b'},
      {"mode",              1, 0, 'm'},
      {"no-coref",          0, 0, 'n'},
      {"rules",             0, 0, 'r'},
      {"steps",             0, 0, 's'},
      {"trx",               0, 0, 't'},
      {"tree",              0, 0, 'T'},
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "bm:nrstTh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "bm:nrstTh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'b':
      p.printTrees(true);
      p.printText(true);
      break;

    case 'm':
      if(!p.setOutputMode(optarg))
      {
        cout << "\"" << optarg << "\" is not a recognized tree mode. Valid options are \"flat\", \"nest\", and \"latex\"." << endl;
        exit(EXIT_FAILURE);
      }
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

    case 'T':
      p.printTrees(true);
      p.printText(false);
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  LtLocale::tryToSetLocale();

  if(optind > (argc - 1) || optind < (argc - 3))
  {
    endProgram(argv[0]);
  }

  p.read(argv[optind]);

  FILE *input = stdin, *output = stdout;

  if(optind <= (argc - 2))
  {
    input = fopen(argv[optind+1], "rb");
  }
  if(optind <= (argc - 3))
  {
    output = fopen(argv[optind+2], "wb");
  }

  p.process(input, output);

  fclose(input);
  fclose(output);
  return EXIT_SUCCESS;
}
