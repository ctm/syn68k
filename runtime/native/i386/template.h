#ifndef _TEMPLATE_H_INCLUDED_
#define _TEMPLATE_H_INCLUDED_

#include "syn68k_private.h"

#define FAILURE 1
#define SUCCESS 0

typedef enum
{
  REGISTER, CONSTANT
} i386_op_type_t;

typedef enum
{
  IN, OUT, INOUT
} io_t;

/* BROKEN_SIZE_32 is a variant of SIZE_32 that is only used with addresses
   that cause trouble on Mac OS X's ld.  The problem ld doesn't recognize
   0x80000000 (-2147483648) as a legitimate 32-bit relative offset.  See
   the extended comment in process.c for more info */

typedef enum { SIZE_8, SIZE_16, SIZE_32, BROKEN_SIZE_32 } byte_size_t;

typedef struct
{
  byte_size_t size;
  i386_op_type_t type;
  io_t inout;
} operand_t;

#define MAX_OPERANDS 6

typedef BOOL boolean_t;

typedef uint8 cc_mask_t;

typedef struct
{
  const char *macro_name;
  const char *i386_cc_in, *i386_cc_out;
  const char *i386_in, *i386_out;   /* All non-cc inputs and outputs. */
  const char *pipe;
  const char *code;
  const char *operand_name[MAX_OPERANDS];
  operand_t operand[MAX_OPERANDS];
} template_t;

extern const template_t template[];

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#endif  /* !_TEMPLATE_H_INCLUDED_ */
