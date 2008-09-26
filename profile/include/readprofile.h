#ifndef read_profile_h
#define read_profile_h

typedef enum { BO_BIG_ENDIAN, BO_LITTLE_ENDIAN } ByteOrder;
extern unsigned long instruction_count[65536];

extern void read_profile (const char *filename);

#endif  /* Not read_profile_h */
