#include <rtx_processor.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>

void endProgram(char *name)
{
  cout << basename(name) << ": perform structural transfer" << endl;
  cout << "USAGE: " << basename(name) << " [ -T | -b ] [ -m mode ] [-r] [-s] [-n] bytecode_file [input_file [output_file]]" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -b, --both:       print text (use with -T)" << endl;
  cout << "  -m, --mode:       set the mode of tree output, options are 'flat', 'nest', 'latex', 'dot', 'box'" << endl;
  cout << "  -n, --no-coref:   treat stream as having no coreference LUs" << endl;
  cout << "  -r, --rules:      print the rules that are being applied" << endl;
  cout << "  -s, --steps:      print the instructions executed by the stack machine" << endl;
  cout << "  -t, --trx:        mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -T, --tree:       print parse trees rather than apply output rules" << endl;
  cout << "  -z, --null-flush: flush output on \\0" << endl;
  cout << "  -h, --help:       show this help" << endl;
#else
  cout << "  -b:   print text (use with -T)" << endl;
  cout << "  -m:   set the mode of tree output, options are 'flat', 'nest', 'latex', 'dot', 'box'" << endl;
  cout << "  -n:   treat stream as having no coreference LUs" << endl;
  cout << "  -r:   print the rules that are being applied" << endl;
  cout << "  -s:   print the instructions executed by the stack machine" << endl;
  cout << "  -t:   mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -T:   print parse trees rather than apply output rules" << endl;
  cout << "  -z:   flush output on \\0" << endl;
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
      {"null-flush",        0, 0, 'z'},
      {"help",              0, 0, 'h'}
    };
#endif

  bool haveB = false;
  bool haveT = false;

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "bm:nrstTzh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "bm:nrstTzh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'b':
      haveB = true;
      break;

    case 'm':
      if(!p.setOutputMode(optarg))
      {
        cout << "\"" << optarg << "\" is not a recognized tree mode. Valid options are \"flat\", \"nest\", \"latex\", \"dot\", and \"box\"." << endl;
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
      haveT = true;
      break;

    case 'z':
      p.setNullFlush(true);
      break;

    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  p.printTrees(haveT);
  p.printText(haveB || (!haveT && !haveB));

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
