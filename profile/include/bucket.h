#ifndef _bucket_h_
#define _bucket_h_

#define NO_AMODE (-1)

typedef struct {
  const char *name;             /* Descriptive name for instruction.         */
  unsigned short litmask, litbits;  /* Mask/values for fixed bits in 68k op. */
  unsigned long count;          /* Frequency of execution.                   */
} Bucket;

extern Bucket bucket[];
extern const unsigned short bucket_map[65536];
extern const int num_buckets;

#endif  /* Not _bucket_h_ */
