#ifndef _token_h_
#define _token_h_

#include <stdio.h>

#include "common.h"

/* Current file stuff. */
#define MAX_INCLUDE_DEPTH 32
#define MAX_FILENAME_LENGTH 248
#define MAX_TOKEN_SIZE 1024

typedef struct {
  FILE *fp;
  unsigned long lineno;
  char filename[MAX_FILENAME_LENGTH];
} InputFile;


/* Token stuff. */
typedef enum {
  TOK_FALSE, TOK_TRUE,

  TOK_IF, TOK_SWITCH, TOK_DEFAULT,
  TOK_AND, TOK_OR, TOK_XOR, TOK_NOT,

  TOK_EQUAL, TOK_NOT_EQUAL, TOK_GREATER_THAN, TOK_LESS_THAN,
  TOK_GREATER_OR_EQUAL, TOK_LESS_OR_EQUAL,

  TOK_DOLLAR_DATA_REGISTER,
  TOK_DOLLAR_ADDRESS_REGISTER,
  TOK_DOLLAR_GENERAL_REGISTER,
  TOK_DOLLAR_AMODE,
  TOK_DOLLAR_REVERSED_AMODE,  /* Used in move; reg/mode order reversed. */
  TOK_DOLLAR_NUMBER,
  TOK_DOLLAR_AMODE_PTR, TOK_DOLLAR_REVERSED_AMODE_PTR,

  TOK_AMODE, TOK_REVERSED_AMODE,
  TOK_AMODE_PTR, TOK_REVERSED_AMODE_PTR,

  TOK_IDENTIFIER,
  TOK_QUOTED_STRING,
  TOK_NUMBER,

  TOK_LEFT_PAREN, TOK_RIGHT_PAREN,

  TOK_DATA_REGISTER, TOK_ADDRESS_REGISTER, TOK_TEMP_REGISTER,

  TOK_NOP,

  TOK_UNION, TOK_INTERSECT,

  TOK_CCC, TOK_CCN, TOK_CCV, TOK_CCX, TOK_CCNZ,

  TOK_SHIFT_LEFT, TOK_SHIFT_RIGHT,

  TOK_ASSIGN,

  TOK_PLUS, TOK_MINUS, TOK_MULTIPLY, TOK_DIVIDE, TOK_MOD,
  TOK_BITWISE_AND, TOK_BITWISE_OR, TOK_BITWISE_XOR, TOK_BITWISE_NOT,

  TOK_NUMARGS,
  TOK_NIL,

  TOK_FUNC_CALL,
  TOK_CAST, TOK_DEREF,
  TOK_CODE,

  TOK_SWAP,
  
  TOK_EXPLICIT_LIST,

  TOK_ENDS_BLOCK, TOK_DONT_POSTINCDEC_UNEXPANDED,
  TOK_NEXT_BLOCK_DYNAMIC, TOK_SKIP_TWO_OPERAND_WORDS,
  TOK_SKIP_FOUR_OPERAND_WORDS, TOK_SKIP_ONE_POINTER, TOK_SKIP_TWO_POINTERS,
  TOK_NATIVE_CODE,

  TOK_DEFINE,
  TOK_DEFOPCODE,
  TOK_ERROR,
  TOK_TAIL,
  TOK_INCLUDE,
  TOK_LIST,
  TOK_UNKNOWN, TOK_EMPTY 
  } TokenType;


#define IS_DOLLAR_TOKEN(t) \
((t) == TOK_DOLLAR_DATA_REGISTER \
 || (t) == TOK_DOLLAR_ADDRESS_REGISTER \
 || (t) == TOK_DOLLAR_GENERAL_REGISTER \
 || (t) == TOK_DOLLAR_AMODE \
 || (t) == TOK_DOLLAR_REVERSED_AMODE \
 || (t) == TOK_DOLLAR_NUMBER \
 || (t) == TOK_DOLLAR_AMODE_PTR \
 || (t) == TOK_DOLLAR_REVERSED_AMODE_PTR)

#define IS_CCBIT_TOKEN(t) \
((t) == TOK_CCC || (t) == TOK_CCN || (t) == TOK_CCV || (t) == TOK_CCX \
 || (t) == TOK_CCNZ)


typedef struct {
  TokenType type;
  union {
    const char *string;
    long n;
    struct { 
      unsigned char sgnd;    /* Signed or unsigned?                        */
      unsigned char size;    /* 1 == byte, 2 == short, 4 == long.          */
      unsigned char which;   /* Which field or register.                   */
    } reginfo, amodeinfo, dollarinfo;  /* KEEP THESE THE SAME!  */
                                      /* Types changed blindly. */
    struct {
      unsigned char sgnd; /* Signed or unsigned?                             */
      unsigned char size; /* 0 == untyped, 1 == byte, 2 == short, 4 == long. */
    } derefinfo, swapinfo;
  } u;
  const char *filename;
  unsigned long lineno;
} Token;


extern void init_tokenizer (void);
extern const InputFile *get_input_file (int levels_back);
extern void open_file (const char *file, const char *search_dirs[]);
extern void open_stream (const char *name, FILE *fp);
extern BOOL fetch_next_token (Token *t);
extern BOOL tokens_equal (const Token *t1, const Token *t2);
extern void dump_token (const Token *t);
extern char *unparse_token (const Token *t, char *buf);
extern FILE *current_stream (void);
extern int skip_to_next_token (void);
extern void close_file (void);

#endif  /* Not _token_h_ */
