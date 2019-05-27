#include <cstdio>
#include <cerrno>
#include <iostream>
#include <stdlib.h>

#include <rtx_parser.h>

using namespace std;

int main(int argc, char** argv)
{
  Parser p;
  p.parse(argv[1]);
  cout << "rtx_comp running" << endl ;
  return 0;
}
