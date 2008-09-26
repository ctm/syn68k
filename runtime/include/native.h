#ifndef _native_h_
#define _native_h_

#ifdef GENERATE_NATIVE_CODE

#include "syn68k_private.h"
#include "../native/i386/host-native.h"
#include "block.h"

/* Define a type for a bit mask for host registers.  Each host register
 * will get one bit in this mask.
 */
#if NUM_HOST_REGS <= 8
typedef uint8 host_reg_mask_t;
#elif NUM_HOST_REGS <= 16
typedef uint16 host_reg_mask_t;
#else  /* NUM_HOST_REGS > 16 */
typedef uint32 host_reg_mask_t;
#endif /* NUM_HOST_REGS > 16 */


#define NUM_GUEST_REGS 16
typedef uint16 guest_reg_mask_t;

/* This enum describes the different types of register caching requests. */
typedef enum
{
  REQUEST_REG,           /* Needs host reg, cache if necessary.         */
  REQUEST_SPARE_REG,     /* Give me any spare reg, value unimportant.   */
} request_type_t;

/* Host registers may not always be left in canonical "native" byte order.
 * For efficiency, we lazily perform certain transformations like
 * byte swapping and offsetting.  Registers in memory slots are _always_
 * in MAP_NATIVE byte order.
 */
typedef enum
{
  MAP_NATIVE,       /* Host byte order.                                */
  MAP_OFFSET,       /* Host byte order, but offset by a constant.      */
#ifdef LITTLEENDIAN
  MAP_SWAP16,       /* Host byte order but with low two bytes swapped. */
  MAP_SWAP32,       /* All four bytes are swapped.                     */
#endif  /* LITTLEENDIAN */
} value_mapping_t;

#define MAP_NATIVE_MASK (1L << MAP_NATIVE)
#define MAP_OFFSET_MASK (1L << MAP_OFFSET)
#ifdef LITTLEENDIAN
# define MAP_SWAP16_MASK (1L << MAP_SWAP16)
# define MAP_SWAP32_MASK (1L << MAP_SWAP32)
#endif  /* LITTLEENDIAN */

#ifdef LITTLEENDIAN
# define MAP_ALL_MASK \
(MAP_NATIVE_MASK | MAP_OFFSET_MASK | MAP_SWAP16_MASK | MAP_SWAP32_MASK)
#else
# define MAP_ALL_MASK (MAP_NATIVE_MASK | MAP_OFFSET_MASK)
#endif

#ifdef LITTLEENDIAN
#define ROS_DIRTY_BIT 4
#else  /* !LITTLEENDIAN */
#define ROS_DIRTY_BIT 2
#endif /* !LITTLEENDIAN */

typedef enum
{
  ROS_NATIVE       = MAP_NATIVE,
  ROS_OFFSET       = MAP_OFFSET,
#ifdef LITTLEENDIAN
  ROS_SWAP16       = MAP_SWAP16,
  ROS_SWAP32       = MAP_SWAP32,
#endif /* LITTLEENDIAN */
  ROS_NATIVE_DIRTY = MAP_NATIVE | ROS_DIRTY_BIT,
  ROS_OFFSET_DIRTY = MAP_OFFSET | ROS_DIRTY_BIT,
#ifdef LITTLEENDIAN
  ROS_SWAP16_DIRTY = MAP_SWAP16 | ROS_DIRTY_BIT,
  ROS_SWAP32_DIRTY = MAP_SWAP32 | ROS_DIRTY_BIT,
#endif /* LITTLEENDIAN */
  ROS_UNTOUCHED,
  ROS_UNTOUCHED_DIRTY,   /* Same mapping as before, but now it's dirty. */
  ROS_INVALID,
} reg_output_state_t;

/* This describes constraints for how to transform an input
 * register operand to either a host register or a pointer to the memory
 * slot for that register.  If you can't match the constraints, you can't
 * generate native code from this descriptor.
 */
typedef struct
{
#ifdef __GNUC__
  BOOL legitimate_p  :1;  /* 0 ==> dummy entry that just terminates array.  */
#else   /* !__GNUC__ */
  uint8 legitimate_p :1;
#endif  /* !__GNUC__ */
  uint8 add8_p             :1;  /* Add 8 to this operand? (address register)*/
  uint8 operand_num        :2;  /* Which operand contains the guest reg #.  */
  uint8 acceptable_mapping :4;  /* Bit mask of acceptable value_mapping_t's.*/

  /* Information about the register on input. */
#ifdef __GNUC__
  request_type_t request_type :8;  /* See above enum for what this means. */
  reg_output_state_t output_state :8;
#else   /* !__GNUC__ */
  uint8 request_type;
  uint8 output_state;
#endif  /* !__GNUC__ */

  host_reg_mask_t regset;       /* Specifies set of allowable host regs.    */
} reg_operand_info_t;

#define MAX_REG_OPERANDS 2
#define MAX_COMPILE_FUNCS 5


#define USE_SCRATCH_REG       (-1)
#define USE_BLOCK             (-2)
#define USE_ZERO              (-3)
#define USE_MINUS_ONE         (-4)
#define USE_M68K_PC           (-5)
#define USE_M68K_PC_PLUS_TWO  (-6)
#define USE_M68K_PC_PLUS_FOUR (-7)
#define USE_0xFFFF            (-8)

#define MAX_M68K_OPERANDS 4

typedef struct
{
  int8 operand_num[MAX_M68K_OPERANDS];
} oporder_t;


typedef struct
{
#ifdef RUNTIME
  int (*func)();   /* Returns nonzero->abort. */
#else  /* !RUNTIME */
  char *func;
#endif /* !RUNTIME */
  oporder_t order;
} compile_func_t;


typedef struct _guest_code_descriptor_t
{
#ifndef RUNTIME
  const char *name;    /* Used while programmatically generating these. */
  BOOL static_p;
#endif
  reg_operand_info_t reg_op_info[MAX_REG_OPERANDS + 1];
  uint8 cc_in;                  /* Bit mask: must be cached on input.    */
  uint8 cc_out;                 /* Bit mask: valid & cached on output.   */
  host_reg_mask_t scratch_reg;  /* If nonzero, allocate a scratch reg.   */
  compile_func_t compile_func[MAX_COMPILE_FUNCS];
#ifdef RUNTIME
  const
#endif
  struct _guest_code_descriptor_t *next;  /* Next in linked list. */
} guest_code_descriptor_t;


#define NO_REG (-1)

typedef struct
{
  int8 host_reg;       /* Which host register cached in, or NO_REG.          */
#ifdef __GNUC__
  value_mapping_t mapping :8; /* If cached, how to map value to canon value. */
  BOOL dirty_without_offset_p :8; /* Must be spilled even if nonzero offset? */
#else  /* !__GNUC__ */
  uint8 mapping;
  uint8 dirty_without_offset_p;
#endif /* !__GNUC__ */
  int32 offset;        /* Add to reg to get canon value if ST_OFFSET.        */
} guest_reg_status_t;


typedef struct
{
  guest_reg_status_t guest_reg_status[NUM_GUEST_REGS];
  int8 host_reg_to_guest_reg[NUM_HOST_REGS]; /* Might be NO_REG.           */
  uint8 cached_cc;    /* CC bits that are cached. */
  uint8 dirty_cc;     /* CC bits that are dirty.  */
} cache_info_t;




#define COMMON_ARGS \
cache_info_t *c, host_code_t **codep, unsigned cc_spill_if_changed, \
unsigned cc_to_compute, int scratch_reg

#define COMMON_TYPES \
cache_info_t *, host_code_t **, unsigned, unsigned, int

#define COMMON_ARG_NAMES \
c, codep, cc_spill_if_changed, cc_to_compute, scratch_reg



extern void native_code_init (void);
extern void host_native_code_init (void);
extern void host_backpatch_native_to_synth_stub (Block *b,
						 host_code_t *stub,
						 uint32 *synth_opcode);
#ifdef SYNCHRONOUS_INTERRUPTS
extern void host_backpatch_check_interrupt_stub (Block *b, host_code_t *stub);
#endif
extern BOOL generate_native_code (const guest_code_descriptor_t *first_desc,
				  cache_info_t *cache_info,
				  const int32 *orig_operands,
				  host_code_t **code,
				  uint8 cc_input, uint8 cc_live,
				  uint8 cc_to_set,
				  BOOL ends_block_p, Block *block,
				  const uint16 *m68k_code);
extern int host_setup_cached_reg (COMMON_ARGS, int guest_reg,
				  unsigned acceptable_mappings,
				  host_reg_mask_t acceptable_regs);
extern int host_movel_reg_reg (COMMON_ARGS, int32 src_reg, int32 dst_reg);

#ifndef host_alloc_reg
extern int host_alloc_reg (cache_info_t *c, host_code_t **code,
			   unsigned cc_spill_if_changed,
			   host_reg_mask_t legal);
#endif

#ifndef host_cache_reg
extern void host_cache_reg (cache_info_t *c, host_code_t **code, 
			    unsigned cc_spill_if_changed,
			    int guest_reg, int host_reg);
#endif

#ifndef host_unoffset_reg
extern void host_unoffset_reg (cache_info_t *c, host_code_t **code,
			       unsigned cc_spill_if_changed,
			       int guest_reg);
#endif

#ifndef host_unoffset_regs
extern void host_unoffset_regs (cache_info_t *c, host_code_t **code,
				unsigned cc_spill_if_changed);
#endif

#ifndef host_spill_reg
extern inline void host_spill_reg (cache_info_t *c, host_code_t **code,
				   unsigned cc_dont_touch, int guest_reg);
#endif

#ifndef host_spill_regs
extern void host_spill_regs (cache_info_t *c, host_code_t **code,
			     unsigned cc_spill_if_changed);
#endif

#ifndef host_spill_cc_bits
extern void host_spill_cc_bits (cache_info_t *c, host_code_t **code,
				unsigned cc);
#endif

extern void make_dirty_regs_native (cache_info_t *c, host_code_t **codep,
				    unsigned cc_spill_if_changed);

#define SPILL_CC_BITS(c, code, cc) \
     do { if (cc) host_spill_cc_bits (c, code, cc); } while (0)

#ifdef LITTLEENDIAN
extern int host_swap16       (COMMON_ARGS, int32 host_reg);
extern int host_swap16_to_32 (COMMON_ARGS, int32 host_reg);
extern int host_swap32       (COMMON_ARGS, int32 host_reg);
extern int host_swap32_to_16 (COMMON_ARGS, int32 host_reg);
#else  /* !LITTLEENDIAN */
#define host_swap16(COMMON_ARG_NAMES)        0
#define host_swap16_to_32(COMMON_ARG_NAMES)  0
#define host_swap32(COMMON_ARG_NAMES)        0
#define host_swap32_to_16(COMMON_ARG_NAMES)  0
#endif /* !LITTLEENDIAN */

#define HOST_CODE_T_BITS (sizeof (host_code_t) * 8)

extern cache_info_t empty_cache_info;

#define NATIVE_START_BYTE_OFFSET   (sizeof (void *) + sizeof (Block *))
#define NATIVE_CODE_TRIED(b) \
  (*(Block **)((b)->compiled_code + PTR_WORDS) == NULL)

extern host_code_t native_to_synth_stub[];

#ifdef SYNCHRONOUS_INTERRUPTS
extern host_code_t check_interrupt_stub[];
#endif

#define NATIVE_PREAMBLE_WORDS \
((((CHECK_INTERRUPT_STUB_BYTES + NATIVE_TO_SYNTH_STUB_BYTES + 1) / 2) + 1) \
 & ~1)

#endif  /* GENERATE_NATIVE_CODE */

#endif  /* !native_h_ */
