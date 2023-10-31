#include <rtx_config.h>
#include <rtx_processor.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>
#include <libgen.h>
#include <iostream>
#include <lttoolbox/i18n.h>

void endProgram(char *name)
{
  I18n i18n {ARC_I18N_DATA, "arc"};
  cout << i18n.format("rtx_proc_desc", {"program"}, {basename(name)});
#if HAVE_GETOPT_LONG
  cout << "  -a, --anaphora:   " << i18n.format("anaphora_desc") << endl;
  cout << "  -b, --both:       " << i18n.format("both_desc") << endl;
  cout << "  -e, --everything: " << i18n.format("everything_desc") << endl;
  cout << "  -f, --filter:     " << i18n.format("filter_desc") << endl;
  cout << "  -F, --filter:     " << i18n.format("filter2_desc") << endl;
  cout << "  -m, --mode:       " << i18n.format("mode_desc") << endl;
  cout << "  -r, --rules:      " << i18n.format("rules_desc") << endl;
  cout << "  -s, --steps:      " << i18n.format("steps_desc") << endl;
  cout << "  -t, --trx:        " << i18n.format("trx_desc") << endl;
  cout << "  -T, --tree:       " << i18n.format("tree_desc") << endl;
  cout << "  -z, --null-flush: " << i18n.format("null_flush_desc") << endl;
  cout << "  -h, --help:       " << i18n.format("help_desc") << endl;
#else
  cout << "  -a:   " << i18n.format("anaphora_desc") << endl;
  cout << "  -b:   " << i18n.format("both_desc") << endl;
  cout << "  -e:   " << i18n.format("everything_desc") << endl;
  cout << "  -f:   " << i18n.format("filter_desc") << endl;
  cout << "  -F:   " << i18n.format("filter2_desc") << endl;
  cout << "  -m:   " << i18n.format("mode_desc") << endl;
  cout << "  -r:   " << i18n.format("rules_desc") << endl;
  cout << "  -s:   " << i18n.format("steps_desc") << endl;
  cout << "  -t:   " << i18n.format("trx_desc") << endl;
  cout << "  -T:   " << i18n.format("tree_desc") << endl;
  cout << "  -z:   " << i18n.format("null_flush_desc") << endl;
  cout << "  -h:   " << i18n.format("help_desc") << endl;
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
        I18n(ARC_I18N_DATA, "arc").error("ARC80920", {"optarg"}, {optarg}, true);
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
      I18n(ARC_I18N_DATA, "arc").error("ARC80020", {"file"}, {argv[optind+1]}, true);
    }
  }
  if(optind <= (argc - 3))
  {
    output = u_fopen(argv[optind+2], "w", NULL, NULL);
    if(input == NULL)
    {
      I18n(ARC_I18N_DATA, "arc").error("ARC80020", {"file"}, {argv[optind+2]}, true);
    }
  }

  p.process(input, output);

  fclose(input);
  u_fclose(output);
  return EXIT_SUCCESS;
}
