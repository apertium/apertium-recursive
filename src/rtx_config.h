#ifndef __RTX_CONFIG_H__
#define __RTX_CONFIG_H__

#include <auto_config.h>

#if !defined(HAVE_DECL_FGETC_UNLOCKED) || !HAVE_DECL_FGETC_UNLOCKED
	#define fgetc_unlocked fgetc
	#define fputc_unlocked fputc
	#define fputs_unlocked fputs
	#define fread_unlocked fread
	#define fwrite_unlocked fwrite
#endif

#endif
