#include <rtx_config.h>
#include <bytecode.h>
#include <lttoolbox/lt_locale.h>
#include <lttoolbox/compression.h>
#include <getopt.h>
#include <libgen.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <unicode/ustdio.h>

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

void writeRule(UString rule, UFILE* out)
{
  UString line;
  for(unsigned int i = 0; i < rule.size(); i++)
  {
    line.clear();
    u_fprintf(out, "[%d]\t", i);
    switch(rule[i])
    {
      case DROP:
        u_fprintf(out, "DROP\n");
        break;
      case DUP:
        u_fprintf(out, "DUP\n");
        break;
      case OVER:
        u_fprintf(out, "OVER\n");
        break;
      case SWAP:
        u_fprintf(out, "SWAP\n");
        break;
      case STRING:
      {
        int len = rule[++i];
        u_fprintf(out, "STRING \"");
        UString s;
        for(int c = 0; c < len; c++)
        {
          s += rule[++i];
        }
        write(s, out);
        //UString s = rule.substr(i+1, len);
        u_fprintf(out, "\"\n");
      }
        break;
      case INT:
        u_fprintf(out, "INT %d\n", (int)rule[++i]);
        break;
      case PUSHFALSE:
        u_fprintf(out, "PUSHFALSE\n");
        break;
      case PUSHTRUE:
        u_fprintf(out, "PUSHTRUE\n");
        break;
      case JUMP:
        u_fprintf(out, "JUMP %d\n", (int)rule[++i]);
        break;
      case JUMPONTRUE:
        u_fprintf(out, "JUMPONTRUE %d\n", (int)rule[++i]);
        break;
      case JUMPONFALSE:
        u_fprintf(out, "JUMPONFALSE %d\n", (int)rule[++i]);
        break;
      case AND:
        u_fprintf(out, "AND\n");
        break;
      case OR:
        u_fprintf(out, "OR\n");
        break;
      case NOT:
        u_fprintf(out, "NOT\n");
        break;
      case EQUAL:
        u_fprintf(out, "EQUAL\n");
        break;
      case ISPREFIX:
        u_fprintf(out, "ISPREFIX\n");
        break;
      case ISSUFFIX:
        u_fprintf(out, "ISSUFFIX\n");
        break;
      case ISSUBSTRING:
        u_fprintf(out, "ISSUBSTRING\n");
        break;
      case EQUALCL:
        u_fprintf(out, "EQUALCL\n");
        break;
      case ISPREFIXCL:
        u_fprintf(out, "ISPREFIXCL\n");
        break;
      case ISSUFFIXCL:
        u_fprintf(out, "ISSUFFIXCL\n");
        break;
      case ISSUBSTRINGCL:
        u_fprintf(out, "ISSUBSTRINGCL\n");
        break;
      case HASPREFIX:
        u_fprintf(out, "HASPREFIX\n");
        break;
      case HASSUFFIX:
        u_fprintf(out, "HASSUFFIX\n");
        break;
      case IN:
        u_fprintf(out, "IN\n");
        break;
      case HASPREFIXCL:
        u_fprintf(out, "HASPREFIXCL\n");
        break;
      case HASSUFFIXCL:
        u_fprintf(out, "HASSUFFIXCL\n");
        break;
      case INCL:
        u_fprintf(out, "INCL\n");
        break;
      case GETCASE:
        u_fprintf(out, "GETCASE\n");
        break;
      case SETCASE:
        u_fprintf(out, "SETCASE\n");
        break;
      case FETCHVAR:
        u_fprintf(out, "FETCHVAR\n");
        break;
      case SETVAR:
        u_fprintf(out, "SETVAR\n");
        break;
      case FETCHCHUNK:
        u_fprintf(out, "FETCHCHUNK\n");
        break;
      case SETCHUNK:
        u_fprintf(out, "SETCHUNK\n");
        break;
      case SOURCECLIP:
        u_fprintf(out, "SOURCECLIP\n");
        break;
      case TARGETCLIP:
        u_fprintf(out, "TARGETCLIP\n");
        break;
      case REFERENCECLIP:
        u_fprintf(out, "REFERENCECLIP\n");
        break;
      case SETCLIP:
        u_fprintf(out, "SETCLIP\n");
        break;
      case CHUNK:
        u_fprintf(out, "CHUNK\n");
        break;
      case APPENDCHILD:
        u_fprintf(out, "APPENDCHILD\n");
        break;
      case APPENDSURFACE:
        u_fprintf(out, "APPENDSURFACE\n");
        break;
      case APPENDALLCHILDREN:
        u_fprintf(out, "APPENDALLCHILDREN\n");
        break;
      case APPENDALLINPUT:
        u_fprintf(out, "APPENDALLINPUT\n");
        break;
      case PUSHINPUT:
        u_fprintf(out, "PUSHINPUT\n");
        break;
      case APPENDSURFACESL:
        u_fprintf(out, "APPENDSURFACESL\n");
        break;
      case APPENDSURFACEREF:
        u_fprintf(out, "APPENDSURFACEREF\n");
        break;
      case OUTPUT:
        u_fprintf(out, "OUTPUT\n");
        break;
      case BLANK:
        u_fprintf(out, "BLANK\n");
        break;
      case OUTPUTALL:
        u_fprintf(out, "OUTPUTALL\n");
        break;
      case CONCAT:
        u_fprintf(out, "CONCAT\n");
        break;
      case REJECTRULE:
        u_fprintf(out, "REJECTRULE\n");
        break;
      case DISTAG:
        u_fprintf(out, "DISTAG\n");
        break;
      case GETRULE:
        u_fprintf(out, "GETRULE\n");
        break;
      case SETRULE:
        u_fprintf(out, "SETRULE\n");
        break;
      case CONJOIN:
        u_fprintf(out, "CONJOIN\n");
        break;
      default:
        auto tmp = rule.substr(i, 1);
        u_fprintf(out, "Unknown instruction: %s\n", tmp.c_str());
    }
  }
  u_fprintf(out, "\n");
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

  FILE* in = stdin;
  UFILE* out = u_finit(stdout, NULL, NULL);

  if(optind <= (argc - 1))
  {
    in = fopen(argv[optind], "rb");
    if(in == NULL)
    {
      cerr << "Error: could not open file " << argv[optind] << " for reading." << endl;
      exit(EXIT_FAILURE);
    }
  }
  if(optind <= (argc - 2))
  {
    out = u_fopen(argv[optind+1], "wb", NULL, NULL);
    if(out == NULL)
    {
      cerr << "Error: could not open file " << argv[optind+1] << " for writing." << endl;
      exit(EXIT_FAILURE);
    }
  }

  int longestPattern = Compression::multibyte_read(in);
  int count = Compression::multibyte_read(in);
  u_fprintf(out, "Input rules:\n");
  u_fprintf(out, "Longest pattern: %d chunks\nNumber of rules: %d\n\n", longestPattern, count);
  int patlen;
  UString cur;
  for(int i = 0; i < count; i++)
  {
    patlen = Compression::multibyte_read(in);
    cur = Compression::string_read(in);
    u_fprintf(out, "Rule %d (%d bytes, pattern %d chunks)\n", i+1, cur.size(), patlen);
    writeRule(cur, out);
  }

  count = Compression::multibyte_read(in);
  u_fprintf(out, "Output rules:\nNumber of rules: %d\n\n", count);
  for(int i = 0; i < count; i++)
  {
    cur = Compression::string_read(in);
    u_fprintf(out, "Rule %d (%d bytes)\n", i, cur.size());
    writeRule(cur, out);
  }

  fclose(in);
  u_fclose(out);
  return EXIT_SUCCESS;
}
