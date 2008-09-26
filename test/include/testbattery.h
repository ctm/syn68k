#ifndef _testbattery_h_
#define _testbattery_h_

typedef struct {
  const char *name;
  void (*code_creator)(uint16 *);
  int cc_mask;
  int code_words;
  int might_change_memory;
  int max_times_to_call;  /* 0 if no limit */
} TestInfo;

#define C_BIT 0x01
#define V_BIT 0x02
#define Z_BIT 0x04
#define N_BIT 0x08
#define X_BIT 0x10
#define ALL_CCS 0x1F
#define NO_CCS 0x00

#define MOVE16_CHANGE_MEMORY 2	/* nasty hack because move16 isn't guaranteed
				   to move memory under NS 3.1 */
#define MIGHT_CHANGE_MEMORY 1
#define WONT_CHANGE_MEMORY  0

#define NO_LIMIT 0

extern const TestInfo test_info[];

#endif  /* Not _testbattery_h_ */
