#ifndef _defopcode_h_
#define _defopcode_h_

#include "list.h"
#include "hash.h"

#define MAX_OPCODE_WORDS 16
#define MAX_VARIANTS 64

typedef struct _CCVariant {
  unsigned cc_may_set        :5;
  unsigned cc_may_not_set    :5;
  unsigned cc_needed         :5;
  unsigned cc_to_known_value :5;
  unsigned cc_known_values   :5;
  char *bits_to_expand;
  List *code;
  const char *native_code_info;
  struct _CCVariant *next;
} CCVariant;

typedef struct {
  const char *name;
  int cpu;
  List *amode;                 
  List *misc_flags;            
  char opcode_bits[16 * MAX_OPCODE_WORDS + 1];/* Concat'd 16-bit patterns.   */
  CCVariant *cc_variant;                      /* Linked list of CC variants. */
  BOOL ends_block;
  BOOL next_block_dynamic;
  BOOL dont_postincdec_unexpanded;
  int operand_words_to_skip;
} ParsedOpcodeInfo;

extern void generate_opcode (ParsedOpcodeInfo *info, SymbolTable *sym);
extern void begin_generating_code (void);
extern void done_generating_code (void);
extern int synthetic_opcode_size (const OpcodeMappingInfo *map);

#endif  /* Not _defopcode_h_ */
