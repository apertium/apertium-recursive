#include <cstdio>
#include <cerrno>
#include <iostream>
#include <stdlib.h>

#include <rtx_parser.h>

int main(int argc, char** argv)
{
  Parser p;
  p.parse(argv[1]);
  return 0;
}
