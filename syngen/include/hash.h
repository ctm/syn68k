#ifndef _hash_h_
#define _hash_h_

#define HASH_BUCKETS 253
#define INITIAL_SYMBOLS 7

typedef union {
  long n;
  void *p;
} SymbolInfo;

typedef struct {
  const char *name;
  SymbolInfo value;
} Symbol;

typedef struct {
  int num_symbols, max_symbols;
  Symbol symbol[INITIAL_SYMBOLS];   /* variable length array. */
} Bucket;

typedef struct {
  Bucket *bucket[HASH_BUCKETS];
} SymbolTable;


typedef enum { HASH_NOERR, HASH_DUPLICATE, HASH_NOTFOUND } HashErr;


extern SymbolTable *make_symbol_table (void);
extern void free_symbol_table (SymbolTable *s);
extern HashErr insert_symbol (SymbolTable *s, const char *name,
			      SymbolInfo val);
extern HashErr lookup_symbol (const SymbolTable *s, const char *name,
			      SymbolInfo *val, const char **original_name_ptr);
extern void dump_symbol_table (const SymbolTable *s);

#endif  /* Not hash_h_ */
