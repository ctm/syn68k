#ifndef _generatecode_h_
#define _generatecode_h_

#include "syn68k_private.h"
#include "defopcode.h"

extern void generate_c_code (const ParsedOpcodeInfo *info,
			     const CCVariant *var,
			     int m68kop, int synop, SymbolTable *sym,
			     const char *operand_decls, const char *postcode,
			     const OpcodeMappingInfo *map);

#endif  /* Not _generatecode_h_ */
