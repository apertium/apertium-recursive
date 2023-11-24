#ifndef __RTXBYTECODE__
#define __RTXBYTECODE__

#include <rtx_config.h>
#include <unicode/uchar.h>

// Stack Operations

static const UChar DROP = 'd';
static const UChar DUP  = '*';
static const UChar OVER = 'o';
static const UChar SWAP = 'w';

// Literals

static const UChar STRING    = 's';
static const UChar INT       = 'i';
static const UChar PUSHFALSE = 'f';
static const UChar PUSHTRUE  = 't';
static const UChar PUSHNULL  = '0';

// Jumps

static const UChar JUMP        = 'j';
static const UChar JUMPONTRUE  = 'J';
static const UChar JUMPONFALSE = '?';
static const UChar LONGJUMP    = 'k';

// Logical Operators

static const UChar AND = '&';
static const UChar OR  = '|';
static const UChar NOT = '!';

// String Comparisons

static const UChar EQUAL       = '=';
static const UChar ISPREFIX    = '(';
static const UChar ISSUFFIX    = ')';
static const UChar ISSUBSTRING = 'c';

// Caseless String Comparisons

static const UChar EQUALCL       = 'q';
static const UChar ISPREFIXCL    = 'p';
static const UChar ISSUFFIXCL    = 'u';
static const UChar ISSUBSTRINGCL = 'r';

// List Comparisons

static const UChar HASPREFIX = '[';
static const UChar HASSUFFIX = ']';
static const UChar IN        = 'n';

// Caseless List Comparisons

static const UChar HASPREFIXCL = '{';
static const UChar HASSUFFIXCL = '}';
static const UChar INCL        = 'N';

// Case Operations

static const UChar GETCASE = 'a';
static const UChar SETCASE = 'A';

// Variables

static const UChar FETCHVAR   = 'v';
static const UChar SETVAR     = '$';
static const UChar FETCHCHUNK = '5';
static const UChar SETCHUNK   = '6';

// Clips

static const UChar SOURCECLIP    = 'S';
static const UChar TARGETCLIP    = 'T';
static const UChar REFERENCECLIP = 'R';
static const UChar SETCLIP       = '>';

// Chunks

static const UChar CHUNK             = 'C';
static const UChar APPENDCHILD       = '1';
static const UChar APPENDSURFACE     = '2';
static const UChar APPENDALLCHILDREN = '3';
static const UChar APPENDALLINPUT    = '4';
static const UChar PUSHINPUT         = '7';
static const UChar APPENDSURFACESL   = '8';
static const UChar APPENDSURFACEREF  = '9';

// Output

static const UChar OUTPUT    = '<';
static const UChar BLANK     = 'b';
static const UChar OUTPUTALL = '@';
static const UChar CONJOIN   = '+';

// Other

static const UChar CONCAT     = '-';
static const UChar REJECTRULE = 'X';
static const UChar DISTAG     = 'D';
static const UChar GETRULE    = '^';
static const UChar SETRULE    = '%';
static const UChar LUCOUNT    = '#';

#endif
