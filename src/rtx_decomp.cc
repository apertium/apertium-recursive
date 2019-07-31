#include <bytecode.h>
#include <lttoolbox/lt_locale.h>
#include <lttoolbox/compression.h>
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>

void endProgram(char *name)
{
  cout << basename(name) << ": decompile a transfer bytecode file" << endl;
  cout << "USAGE: " << basename(name) << " [ -h ] [input_file [output_file]]" << endl;
  cout << "Options:" << endl;
#if HAVE_GETOPT_LONG
  cout << "  -h, --help: show this help" << endl;
#else
  cout << "  -h: show this help" << endl;
#endif
  exit(EXIT_FAILURE);
}

void writeRule(wstring rule, FILE* out)
{
  wstring line;
  for(unsigned int i = 0; i < rule.size(); i++)
  {
    line.clear();
    fwprintf(out, L"[%d]\t", i);
    switch(rule[i])
    {
      case DROP:
        fwprintf(out, L"DROP\n");
        break;
      case DUP:
        fwprintf(out, L"DUP\n");
        break;
      case OVER:
        fwprintf(out, L"OVER\n");
        break;
      case SWAP:
        fwprintf(out, L"SWAP\n");
        break;
      case STRING:
      {
        int len = rule[++i];
        fwprintf(out, L"STRING \"");
        for(int c = 0; c < len; c++)
        {
          fputwc(rule[++i], out);
        }
        //wstring s = rule.substr(i+1, len);
        fwprintf(out, L"\"\n");
      }
        break;
      case INT:
        fwprintf(out, L"INT %d\n", (int)rule[++i]);
        break;
      case PUSHFALSE:
        fwprintf(out, L"PUSHFALSE\n");
        break;
      case PUSHTRUE:
        fwprintf(out, L"PUSHTRUE\n");
        break;
      case JUMP:
        fwprintf(out, L"JUMP %d\n", (int)rule[++i]);
        break;
      case JUMPONTRUE:
        fwprintf(out, L"JUMPONTRUE %d\n", (int)rule[++i]);
        break;
      case JUMPONFALSE:
        fwprintf(out, L"JUMPONFALSE %d\n", (int)rule[++i]);
        break;
      case AND:
        fwprintf(out, L"AND\n");
        break;
      case OR:
        fwprintf(out, L"OR\n");
        break;
      case NOT:
        fwprintf(out, L"NOT\n");
        break;
      case EQUAL:
        fwprintf(out, L"EQUAL\n");
        break;
      case ISPREFIX:
        fwprintf(out, L"ISPREFIX\n");
        break;
      case ISSUFFIX:
        fwprintf(out, L"ISSUFFIX\n");
        break;
      case ISSUBSTRING:
        fwprintf(out, L"ISSUBSTRING\n");
        break;
      case EQUALCL:
        fwprintf(out, L"EQUALCL\n");
        break;
      case ISPREFIXCL:
        fwprintf(out, L"ISPREFIXCL\n");
        break;
      case ISSUFFIXCL:
        fwprintf(out, L"ISSUFFIXCL\n");
        break;
      case ISSUBSTRINGCL:
        fwprintf(out, L"ISSUBSTRINGCL\n");
        break;
      case HASPREFIX:
        fwprintf(out, L"HASPREFIX\n");
        break;
      case HASSUFFIX:
        fwprintf(out, L"HASSUFFIX\n");
        break;
      case IN:
        fwprintf(out, L"IN\n");
        break;
      case HASPREFIXCL:
        fwprintf(out, L"HASPREFIXCL\n");
        break;
      case HASSUFFIXCL:
        fwprintf(out, L"HASSUFFIXCL\n");
        break;
      case INCL:
        fwprintf(out, L"INCL\n");
        break;
      case GETCASE:
        fwprintf(out, L"GETCASE\n");
        break;
      case SETCASE:
        fwprintf(out, L"SETCASE\n");
        break;
      case FETCHVAR:
        fwprintf(out, L"FETCHVAR\n");
        break;
      case SETVAR:
        fwprintf(out, L"SETVAR\n");
        break;
      case SOURCECLIP:
        fwprintf(out, L"SOURCECLIP\n");
        break;
      case TARGETCLIP:
        fwprintf(out, L"TARGETCLIP\n");
        break;
      case REFERENCECLIP:
        fwprintf(out, L"REFERENCECLIP\n");
        break;
      case SETCLIP:
        fwprintf(out, L"SETCLIP\n");
        break;
      case CHUNK:
        fwprintf(out, L"CHUNK\n");
        break;
      case APPENDCHILD:
        fwprintf(out, L"APPENDCHILD\n");
        break;
      case APPENDSURFACE:
        fwprintf(out, L"APPENDSURFACE\n");
        break;
      case APPENDALLCHILDREN:
        fwprintf(out, L"APPENDALLCHILDREN\n");
        break;
      case APPENDALLINPUT:
        fwprintf(out, L"APPENDALLINPUT\n");
        break;
      case OUTPUT:
        fwprintf(out, L"OUTPUT\n");
        break;
      case BLANK:
        fwprintf(out, L"BLANK\n");
        break;
      case OUTPUTALL:
        fwprintf(out, L"OUTPUTALL\n");
        break;
      case CONCAT:
        fwprintf(out, L"CONCAT\n");
        break;
      case REJECTRULE:
        fwprintf(out, L"REJECTRULE\n");
        break;
      case DISTAG:
        fwprintf(out, L"DISTAG\n");
        break;
      case GETRULE:
        fwprintf(out, L"GETRULE\n");
        break;
      case SETRULE:
        fwprintf(out, L"SETRULE\n");
        break;
      default:
        fwprintf(out, L"Unknown instruction: %s", rule.substr(i, 1));
    }
  }
  fwprintf(out, L"\n");
}

int main(int argc, char *argv[])
{
#if HAVE_GETOPT_LONG
  static struct option long_options[]=
    {
      {"help",              0, 0, 'h'}
    };
#endif

  while(true)
  {
#if HAVE_GETOPT_LONG
    int option_index;
    int c = getopt_long(argc, argv, "h", long_options, &option_index);
#else
    int c = getopt(argc, argv, "h");
#endif

    if(c == -1)
    {
      break;
    }

    switch(c)
    {
    case 'h':
    default:
      endProgram(argv[0]);
      break;
    }
  }

  LtLocale::tryToSetLocale();

  if(optind < (argc - 2))
  {
    endProgram(argv[0]);
  }

  FILE *in = stdin, *out = stdout;

  if(optind <= (argc - 1))
  {
    in = fopen(argv[optind], "rb");
    if(in == NULL)
    {
      wcerr << L"Error: could not open file " << argv[optind] << " for reading." << endl;
      exit(EXIT_FAILURE);
    }
  }
  if(optind <= (argc - 2))
  {
    out = fopen(argv[optind+1], "wb");
    if(out == NULL)
    {
      wcerr << L"Error: could not open file " << argv[optind+1] << " for writing." << endl;
      exit(EXIT_FAILURE);
    }
  }

  int longestPattern = Compression::multibyte_read(in);
  int count = Compression::multibyte_read(in);
  fwprintf(out, L"Input rules:\n");
  fwprintf(out, L"Longest pattern: %d chunks\nNumber of rules: %d\n\n", longestPattern, count);
  int patlen;
  wstring cur;
  for(int i = 0; i < count; i++)
  {
    patlen = Compression::multibyte_read(in);
    cur = Compression::wstring_read(in);
    fwprintf(out, L"Rule %d (%d bytes, pattern %d chunks)\n", i+1, cur.size(), patlen);
    writeRule(cur, out);
  }

  count = Compression::multibyte_read(in);
  fwprintf(out, L"Output rules:\nNumber of rules: %d\n\n", count);
  for(int i = 0; i < count; i++)
  {
    cur = Compression::wstring_read(in);
    fwprintf(out, L"Rule %d (%d bytes)\n", i, cur.size());
    writeRule(cur, out);
  }

  fclose(in);
  fclose(out);
  return EXIT_SUCCESS;
}
