#include <rtx_reader.h>
#include <lttoolbox/lt_locale.h>
#include <cstdlib>
#include <iostream>
#include <apertium/string_utils.h>
#include <libgen.h>

using namespace Apertium;
using namespace std;

int main(int argc, char *argv[])
{
  LtLocale::tryToSetLocale();

  if(argc != 4)
  {
    wcerr << "USAGE: " << basename(argv[0]) << " rules_file pattern_file bytecode_file" << endl;
    exit(EXIT_FAILURE);
  }

  RTXReader myReader;
  myReader.read(argv[1]);
  myReader.write(argv[2], argv[3]);

  return EXIT_SUCCESS;
}
