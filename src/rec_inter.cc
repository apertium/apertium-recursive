#include <interchunk.h>
#include <lttoolbox/lt_locale.h>

int main(int argc, char *argv[])
{
  //LtLocale::tryToSetLocale();

  Interchunk i;
  FILE *input = stdin, *output = stdout;
  string f1 = argv[1];
  string f2 = argv[2];

  i.read(f1, f2);
  i.interchunk(input, output);
  return EXIT_SUCCESS;
}
