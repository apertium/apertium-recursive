#include <rtx_processor.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>
#include <libgen.h>
#include <iostream>

void endProgram(char *name)
{
  cout << basename(name) << ": perform structural transfer" << endl;
  cout << "USAGE: " << basename(name) << " [-a] [-t] [-z] [ -T | -b ] [ -m mode ] [ -e | -f | -r | -s ] bytecode_file [input_file [output_file]]" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -a, --anaphora:   expect coreference LUs from apertium-anaphora" << endl;
  cout << "  -b, --both:       print text (use with -T)" << endl;
  cout << "  -e, --everything: print a complete trace of execution" << endl;
  cout << "  -f, --filter:     trace filterParseGraph()" << endl;
  cout << "  -F, --filter:     filter branches more often" << endl;
  cout << "  -m, --mode:       set the mode of tree output, options are 'flat', 'nest', 'latex', 'dot', 'box'" << endl;
  cout << "  -r, --rules:      print the rules that are being applied" << endl;
  cout << "  -s, --steps:      print the instructions executed by the stack machine" << endl;
  cout << "  -t, --trx:        mimic the behavior of apertium-transfer and apertium-interchunk" << endl;
  cout << "  -T, --tree:       print parse trees rather than apply output rules" << endl;
  cout << "  -z, --null-flush: flush output on \\0" << endl;
  cout << "  -h, --help:       show this help" << endl;
#else
  cout << "  -a:   expect coreference LUs from apertium-anaphora" << endl;
  cout << "  -b:   print text (use with -T)" << endl;
  cout << "  -e:   print a complete trace of execution" << endl;
  cout << "  -f:   trace filterParseGraph()" << endl;
  cout << "  -F:   filter branches more often" << endl;
  cout << "  -m:   set the mode of tree output, options are 'flat', 'nest', 'latex', 'dot', 'box'" << endl;
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
      {"anaphora",          0, 0, 'a'},
      {"both",              0, 0, 'b'},
      {"everything",        0, 0, 'e'},
      {"filter",            0, 0, 'f'},
      {"filter",            0, 0, 'F'},
      {"mode",              1, 0, 'm'},
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
    int c = getopt_long(argc, argv, "abefFm:rstTzh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "abefFm:rstTzh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'a':
      p.withoutCoref(false);
      break;

    case 'b':
      haveB = true;
      break;

    case 'e':
      p.completeTrace(true);
      break;

    case 'f':
      p.printFilter(true);
      break;

    case 'F':
      p.noFiltering(false);
      break;

    case 'm':
      if(!p.setOutputMode(optarg))
      {
        cout << "\"" << optarg << "\" is not a recognized tree mode. Valid options are \"flat\", \"nest\", \"latex\", \"dot\", and \"box\"." << endl;
        exit(EXIT_FAILURE);
      }
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

  FILE *input = stdin;
  UFILE* output = u_finit(stdout, NULL, NULL);

  if(optind <= (argc - 2))
  {
    input = fopen(argv[optind+1], "rb");
    if(input == NULL)
    {
      cerr << "Unable to open " << argv[optind+1] << " for reading." << endl;
      exit(EXIT_FAILURE);
    }
  }
  if(optind <= (argc - 3))
  {
    output = u_fopen(argv[optind+2], "w", NULL, NULL);
    if(input == NULL)
    {
      cerr << "Unable to open " << argv[optind+2] << " for writing." << endl;
      exit(EXIT_FAILURE);
    }
  }

  p.process(input, output);

  fclose(input);
  u_fclose(output);
  return EXIT_SUCCESS;
}
