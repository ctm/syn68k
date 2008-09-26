#include "amode.h"
#include <assert.h>

const char *
amode_to_string (int amode)
{
  static const char *amode_string[64] = {
#if 0
    "d0",   "d1",   "d2",   "d3",   "d4",   "d5",   "d6",   "d7",
    "a0",   "a1",   "a2",   "a3",   "a4",   "a5",   "a6",   "a7",
    "a0@",  "a1@",  "a2@",  "a3@",  "a4@",  "a5@",  "a6@",  "a7@",
    "a0@+", "a1@+", "a2@+", "a3@+", "a4@+", "a5@+", "a6@+", "a7@+",
    "a0@-", "a1@-", "a2@-", "a3@-", "a4@-", "a5@-", "a6@-", "a7@-",
    "a0@(d16)",    "a1@(d16)",      "a2@(d16)",      "a3@(d16)",
    "a4@(d16)",    "a5@(d16)",      "a6@(d16)",      "a7@(d16)",
    "[mode 6/a0]", "[mode 6/a1]",   "[mode 6/a2]",   "[mode 6/a3]",
    "[mode 6/a4]", "[mode 6/a5]",   "[mode 6/a6]",   "[mode 6/a7]",
    "abs.w",       "abs.l",         "pc@(d16)",      "[amode 7/3]",
    "#<data>",     "[undefined]",   "[undefined]",   "[undefined]",
#else   /* We want more general categories. */
    "dn", "dn", "dn", "dn", "dn", "dn", "dn", "dn",
    "an", "an", "an", "an", "an", "an", "an", "an",
    "an@",  "an@",  "an@",  "an@",  "an@",  "an@",  "an@",  "an@",
    "an@+", "an@+", "an@+", "an@+", "an@+", "an@+", "an@+", "an@+",
    "an@-", "an@-", "an@-", "an@-", "an@-", "an@-", "an@-", "an@-",
    "an@(d16)",    "an@(d16)",      "an@(d16)",      "an@(d16)",
    "an@(d16)",    "an@(d16)",      "an@(d16)",      "an@(d16)",
    "[mode 6/an]", "[mode 6/an]",   "[mode 6/an]",   "[mode 6/an]",
    "[mode 6/an]", "[mode 6/an]",   "[mode 6/an]",   "[mode 6/an]",
    "abs.w",       "abs.l",         "pc@(d16)",      "[amode 7/3]",
    "#<data>",     "[undefined]",   "[undefined]",   "[undefined]",
#endif
  };

  assert (amode >= 0 && amode < 64);
  return amode_string[amode];
}
