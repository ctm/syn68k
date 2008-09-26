#ifndef _byteorder_h_
#define _byteorder_h_

#include "defopcode.h"
#include "syn68k_private.h"

typedef struct {
  const char *operand_decls;
  int num_bitfields;
  BitfieldInfo bitfield[8];
} OperandInfo;

extern int compute_operand_info (OperandInfo *op, const ParsedOpcodeInfo *p,
				 const CCVariant *v);

#endif  /* Not _byteorder_h_ */
