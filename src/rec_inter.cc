#include <interchunk.h>
#include <lttoolbox/lt_locale.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
  Interchunk i;

#if HAVE_GETOPT_LONG
  static struct option long_options[]=
    {
      {"rule-steps",        0, 0, 's'},
      {"rules",             0, 0, 'r'},
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "srh", long_options, &option_index);
#else
    int c = getopt(argc, argv, "srh");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 's':
      i.printSteps(true);
      break;

    case 'r':
      i.printRules(true);
      break;

    case 'h':
    default:
      //endProgram(argv[0]);
      break;
    }
  }

  LtLocale::tryToSetLocale();

  FILE *input = stdin, *output = stdout;
  string f1 = argv[argc-2];
  string f2 = argv[argc-1];

  i.read(f1, f2);
  i.interchunk(input, output);
  return EXIT_SUCCESS;
}
