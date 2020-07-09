#ifndef __RTXBYTECODE__
#define __RTXBYTECODE__

#include <rtx_config.h>

// Stack Operations

static const wchar_t DROP = L'd';
static const wchar_t DUP  = L'*';
static const wchar_t OVER = L'o';
static const wchar_t SWAP = L'w';

// Literals

static const wchar_t STRING    = L's';
static const wchar_t INT       = L'i';
static const wchar_t PUSHFALSE = L'f';
static const wchar_t PUSHTRUE  = L't';
static const wchar_t PUSHNULL  = L'0';

// Jumps

static const wchar_t JUMP        = L'j';
static const wchar_t JUMPONTRUE  = L'J';
static const wchar_t JUMPONFALSE = L'?';

// Logical Operators

static const wchar_t AND = L'&';
static const wchar_t OR  = L'|';
static const wchar_t NOT = L'!';

// String Comparisons

static const wchar_t EQUAL       = L'=';
static const wchar_t ISPREFIX    = L'(';
static const wchar_t ISSUFFIX    = L')';
static const wchar_t ISSUBSTRING = L'c';

// Caseless String Comparisons

static const wchar_t EQUALCL       = L'q';
static const wchar_t ISPREFIXCL    = L'p';
static const wchar_t ISSUFFIXCL    = L'u';
static const wchar_t ISSUBSTRINGCL = L'r';

// List Comparisons

static const wchar_t HASPREFIX = L'[';
static const wchar_t HASSUFFIX = L']';
static const wchar_t IN        = L'n';

// Caseless List Comparisons

static const wchar_t HASPREFIXCL = L'{';
static const wchar_t HASSUFFIXCL = L'}';
static const wchar_t INCL        = L'N';

// Case Operations

static const wchar_t GETCASE = L'a';
static const wchar_t SETCASE = L'A';

// Variables

static const wchar_t FETCHVAR   = L'v';
static const wchar_t SETVAR     = L'$';
static const wchar_t FETCHCHUNK = L'5';
static const wchar_t SETCHUNK   = L'6';

// Clips

static const wchar_t SOURCECLIP    = L'S';
static const wchar_t TARGETCLIP    = L'T';
static const wchar_t REFERENCECLIP = L'R';
static const wchar_t SETCLIP       = L'>';

// Chunks

static const wchar_t CHUNK             = L'C';
static const wchar_t APPENDCHILD       = L'1';
static const wchar_t APPENDSURFACE     = L'2';
static const wchar_t APPENDALLCHILDREN = L'3';
static const wchar_t APPENDALLINPUT    = L'4';
static const wchar_t PUSHINPUT         = L'7';
static const wchar_t APPENDSURFACESL   = L'8';
static const wchar_t APPENDSURFACEREF  = L'9';

// Output

static const wchar_t OUTPUT    = L'<';
static const wchar_t BLANK     = L'b';
static const wchar_t OUTPUTALL = L'@';
static const wchar_t CONJOIN   = L'+';

// Other

static const wchar_t CONCAT     = L'-';
static const wchar_t REJECTRULE = L'X';
static const wchar_t DISTAG     = L'D';
static const wchar_t GETRULE    = L'^';
static const wchar_t SETRULE    = L'%';
static const wchar_t LUCOUNT    = L'#';

#endif
