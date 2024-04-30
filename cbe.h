#ifndef CBE_H
#define CBE_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CBE_LOG_DEBUG_MSG

#ifndef CBE_ALLOC
#define CBE_ALLOC a_alloc
#endif // CBE_ALLOC

#ifndef CBE_REALLOC
#define CBE_REALLOC a_realloc
#endif // CBE_REALLOC

#define CBE_STR_HELPER(s) #s
#define CBE_STR(s) CBE_STR_HELPER(s)

#define CBE_ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

#define CBE_SWAP(a, b)                                                         \
  do {                                                                         \
    __typeof__(a) tmp = (a);                                                   \
    (a) = (b);                                                                 \
    (b) = tmp;                                                                 \
  } while (0)

#define CBE_ASSERT(ctx, ...)                                                   \
  do {                                                                         \
    if (!(__VA_ARGS__)) {                                                      \
      fprintf(stderr,                                                          \
              "\033[0;1m%s:%d: \033[31;1mASSERTION FAILED: \033[0;0m" CBE_STR( \
                  __VA_ARGS__) "\n",                                           \
              __FILE__, __LINE__);                                             \
      print_stacktrace(ctx);                                                   \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#ifdef CBE_LOG_DEBUG_MSG
#define CBE_DEBUG(s, ...)                                                      \
  fprintf(stderr, "\033[0;1m%s:%d: \033[34;1mDEBUG: \033[0;0m" s, __FILE__,    \
          __LINE__, __VA_ARGS__)
#else
#define CBE_DEBUG(...)
#endif

#define slice_init_capacity 256

#define slice(T)                                                               \
  struct {                                                                     \
    T *items;                                                                  \
    usz capacity, size;                                                        \
  }

#define slice_init(s)                                                          \
  do {                                                                         \
    (s)->items = (__typeof__(*(s)->items) *)CBE_ALLOC(sizeof(*(s)->items) *    \
                                                      slice_init_capacity);    \
    (s)->capacity = slice_init_capacity;                                       \
    (s)->size = 0;                                                             \
  } while (0)

#define slice_init_with_capacity(s, cap)                                       \
  do {                                                                         \
    usz capacity = (cap);                                                      \
    (s)->items =                                                               \
        (__typeof__(*(s)->items) *)CBE_ALLOC(sizeof(*(s)->items) * capacity);  \
    (s)->capacity = capacity;                                                  \
    (s)->size = 0;                                                             \
  } while (0)

#define slice_push(s, ...)                                                     \
  do {                                                                         \
    if ((s)->size >= (s)->capacity) {                                          \
      usz old_capacity = (s)->capacity;                                        \
      (s)->capacity *= 2;                                                      \
      (s)->items = (__typeof__(*(s)->items) *)CBE_REALLOC(                     \
          (s)->items, old_capacity, sizeof(*(s)->items) * (s)->capacity);      \
    }                                                                          \
    (s)->items[(s)->size++] = (__VA_ARGS__);                                   \
  } while (0)

#define slice_pop(s) (s)->items[(s)->size--]

typedef const char *cstr;

typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef long isz;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long usz;

struct cbe_stack_frame {
  cstr file, fn;
  usz line;
};

#define CBE_ASSERTION_STACKTRACE

#ifdef CBE_ASSERTION_STACKTRACE
#define push_stack_frame(ctx)                                                  \
  do {                                                                         \
    slice_push(&ctx->stacktrace,                                               \
               (struct cbe_stack_frame){__FILE__, __func__, __LINE__});        \
  } while (0)

#define pop_stack_frame(ctx)                                                   \
  do {                                                                         \
    (void)slice_pop(&ctx->stacktrace);                                         \
  } while (0)
#else
#define push_stack_frame(ctx)
#define pop_stack_frame(ctx)
#endif // CBE_ASSERTION_STACKTRACE

enum cbe_register {
  CBE_REG_NONE,

  CBE_REG_EAX,
  CBE_REG_EBX,
  CBE_REG_ECX,
  CBE_REG_EDX,

  CBE_REG_ESI,
  CBE_REG_EDI,
  CBE_REG_EBP,
  CBE_REG_ESP,

  CBE_REG_ERROR,
  CBE_REG_COUNT,
};

struct cbe_register_symbol {
  cstr name;
  enum cbe_register reg;
  int location;
};

typedef usz cbe_interval_id;
struct cbe_live_interval {
  struct cbe_register_symbol symbol;
  int location;
  int start_point, end_point;
};
typedef slice(struct cbe_live_interval *) cbe_live_intervals;

typedef slice(struct cbe_live_interval) cbe_by_start_point;
typedef slice(struct cbe_live_interval *) cbe_by_end_point;

int cbe_sort_by_end_point(const void *, const void *);

void cbe_delete_interval(cbe_live_intervals *, usz);

struct cbe_register_pool {
  usz head;
  enum cbe_register registers[CBE_REG_COUNT];
  usz registers_count;
};

cstr cbe_get_register_name(enum cbe_register);
enum cbe_register cbe_get_register(struct cbe_register_pool *);
void cbe_free_register(struct cbe_register_pool *, enum cbe_register);
bool cbe_register_pool_is_empty(struct cbe_register_pool *);

enum cbe_type_tag {
  CBE_TYPE_BYTE,
  CBE_TYPE_SHORT,
  CBE_TYPE_INT,
  CBE_TYPE_LONG,

  CBE_TYPE_RAWPTR,
  CBE_TYPE_PTR,

  CBE_TYPE_VOID,
};

typedef usz cbe_type_id;
struct cbe_type {
  enum cbe_type_tag tag;
  // union {
  cbe_type_id ptr;
  // };
};

enum cbe_value_tag {
  CBE_VALUE_NIL,
  CBE_VALUE_INTEGER,
  CBE_VALUE_STRING,
  CBE_VALUE_VARIABLE,
};
struct cbe_value {
  enum cbe_value_tag tag;
  cbe_type_id type_id;
  union {
    i64 integer;
    cstr string;
    usz variable;
  };
};

struct cbe_stack_variable {
  usz associated_name_index;
  // [rsp - (8 * (slot + 1))]
  usz slot;
  struct cbe_value stored_value;
};

struct cbe_temporary {
  usz name_index;
  cbe_type_id type_id;
  cbe_interval_id interval_id;
};

enum cbe_instruction_tag {
  CBE_INST_ALLOC, /* %0 = alloc <type> */
  CBE_INST_STORE, /* store <typed value>, <typed value> */
  CBE_INST_LOAD,  /* */
  CBE_INST_RET,   /* */
};
struct cbe_instruction {
  enum cbe_instruction_tag tag;
  bool has_temporary;
  struct cbe_temporary temporary;
  union {
    struct {
      cbe_type_id type;
    } alloc;
    struct {
      struct cbe_value value, pointer;
    } store;
    struct {
      struct cbe_value pointer;
    } load;
    struct {
      struct cbe_value *value;
    } ret;
  };
};

struct cbe_block {
  usz name_index;
  slice(struct cbe_instruction) instructions;
};

struct cbe_function {
  usz name_index;
  cbe_type_id type_id;
  slice(struct cbe_block) blocks;
};

struct cbe_global_variable {
  usz name_index;
  struct cbe_value value;
  bool constant; // Basically just specifies if the global is defined in the
                 // rodata or data section.
};

struct cbe_context {
  struct cbe_register_pool register_pool;
  int current_stack_location;

  slice(struct cbe_stack_frame) stacktrace;
  slice(struct cbe_function) functions;
  slice(struct cbe_stack_variable) stack_variables;
  slice(struct cbe_global_variable) global_variables;
  slice(struct cbe_type) types;

  slice(struct cbe_live_interval) live_intervals;
  cbe_live_intervals active_intervals;
  usz ip; // instruction pointer used for register allocation.

  slice(cstr) symbol_table;
  slice(cstr) string_table;
};

static void print_stacktrace(struct cbe_context ctx) {
  for (usz i = ctx.stacktrace.size; i > 0; i--) {
    struct cbe_stack_frame frame = ctx.stacktrace.items[i - 1];
    printf("  called from %s (%s:%zu)\n", frame.fn, frame.file, frame.line);
  }
}

enum cbe_validation_result {
  CBE_VALID_OK,
  CBE_VALID_TYPE_MISMATCH,
};

void cbe_init(struct cbe_context *);

cbe_live_intervals cbe_expire_old_intervals(struct cbe_context *,
                                            cbe_live_intervals,
                                            struct cbe_live_interval *);
void cbe_spill_at_interval(struct cbe_context *, cbe_live_intervals,
                           struct cbe_live_interval *);

usz cbe_allocate_stack_variable(struct cbe_context *, usz);
usz cbe_find_stack_variable(struct cbe_context *, usz);

usz cbe_new_global_variable(struct cbe_context *, struct cbe_global_variable);

cbe_type_id cbe_add_type(struct cbe_context *, struct cbe_type);

usz cbe_find_or_add_symbol(struct cbe_context *, cstr);
usz cbe_find_symbol(struct cbe_context *, cstr);
usz cbe_add_symbol(struct cbe_context *, cstr);

struct cbe_live_interval
cbe_add_or_increment_live_interval(struct cbe_context *, cstr);

void cbe_generate(struct cbe_context *, FILE *);
void cbe_generate_global_variable(struct cbe_context *, FILE *,
                                  struct cbe_global_variable);
void cbe_generate_function(struct cbe_context *, FILE *, struct cbe_function);
void cbe_generate_block(struct cbe_context *, FILE *, struct cbe_block);
void cbe_generate_instruction(struct cbe_context *, FILE *,
                              struct cbe_instruction);
void cbe_generate_value(struct cbe_context *, FILE *, struct cbe_value);
void cbe_generate_type(struct cbe_context *, FILE *, struct cbe_type);

enum cbe_validation_result cbe_validate(struct cbe_context *);
enum cbe_validation_result cbe_validate_function(struct cbe_context *,
                                                 struct cbe_function);
enum cbe_validation_result cbe_validate_block(struct cbe_context *,
                                              struct cbe_block);
enum cbe_validation_result cbe_validate_instruction(struct cbe_context *,
                                                    struct cbe_instruction);
enum cbe_validation_result cbe_validate_value(struct cbe_context *,
                                              struct cbe_value);
enum cbe_validation_result cbe_validate_type(struct cbe_context *,
                                             struct cbe_type);

const char *cbe_register_name(struct cbe_context *, usz);

void cbe_debug_registers(struct cbe_context *);
void cbe_debug_stack_variables(struct cbe_context *);
void cbe_debug_symbol_table(struct cbe_context *);

#endif // CBE_H
