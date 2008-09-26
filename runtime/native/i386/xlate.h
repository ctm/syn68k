#ifndef _xlate_h_
#define _xlate_h_

#include "syn68k_private.h"
#include "native.h"


typedef enum { NA=0, B=1, W=2, L=4, BW=3, BL=5, WL=6, BWL=7 } size_mask_t;

typedef enum
{
  AMODE_NONE,
  AMODE_IMM,
  AMODE_REG,
  AMODE_AREG,
  AMODE_IND,
  AMODE_POSTINC,
  AMODE_PREDEC,
  AMODE_INDOFF,
  AMODE_ABS,
  AMODE_INDIX,
} amode_t;

#define MEMORY_AMODE_P(a)   ((a) >= AMODE_IND && (a) <= AMODE_INDIX)
#define REGISTER_AMODE_P(a) ((a) == AMODE_REG || (a) == AMODE_AREG)

typedef enum
{
  OP_UNARY,   /* e.g. negw a0@          */
  OP_BINARY,  /* e.g. addl d0,a5        */
  OP_MOVE     /* e.g. movel a0@(124),d0 */
} op_type_t;

typedef struct
{
  amode_t amode;  /* Addressing mode for how to grab this value.          */
  int operand_num[2];  /* Which ops specify this value (may not use all). */
} value_t;

#define MAX_XLATE_VALUES 2

typedef struct
{
  const char *name;
  op_type_t type;
  size_mask_t sizes;
  int cc_to_compute;
  const char *i386_op;
  value_t value[MAX_XLATE_VALUES]; /* For binops & moves, 2nd is the dest. */
} xlate_descriptor_t;

#ifndef NELEM
#define NELEM(a) ((sizeof (a)) / sizeof ((a)[0]))
#endif

extern guest_code_descriptor_t
*process_xlate_descriptor (const xlate_descriptor_t *x);

extern const xlate_descriptor_t xlate_table[];

#endif  /* !_xlate_h_ */
