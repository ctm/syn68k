#ifndef _common_h_
#define _common_h_

#include <stdio.h>
#include "syn68k_private.h"   /* To typedef BOOL. */

/* Useful macros. */
#define ABS(x) ((x)>=0?(x):-(x))
#define SGN(x) ((x)>0?1:((x<0)?-1:0))  /* or (((x) > 0) - ((x) < 0))  :-) */
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* Global variables. */
extern int optimization_level;
extern BOOL preprocess_only;
extern FILE *syn68k_c_stream, *mapinfo_c_stream, *mapindex_c_stream;
extern FILE *profileinfo_stream;
extern BOOL verbose;

#endif  /* not _common_h_ */
