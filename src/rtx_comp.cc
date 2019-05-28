#include <cstdio>
#include <cerrno>
#include <iostream>
#include <stdlib.h>

#include <rtx_compiler.h>

using namespace std;

int main(int argc, char** argv)
{
  Compiler* c = new Compiler(argv[1]);
  cout << "rtx_comp running" << endl ;
  delete c;
  return 0;
}
