#ifndef CBE_H
#define CBE_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef CBE_ALLOC
#define CBE_ALLOC a_alloc
#endif // CBE_ALLOC

#ifndef CBE_REALLOC
#define CBE_REALLOC a_realloc
#endif // CBE_REALLOC

#define _STR(x) #x
#define STR(x) _STR(x)

#define CBE_ASSERT(ctx, ...)                                                   \
  do {                                                                         \
    if (!(__VA_ARGS__)) {                                                      \
      fprintf(stderr,                                                          \
              "%s:%d (%s): \033[31;1mASSERTION FAILED: \033[0;0m" STR(         \
                  __VA_ARGS__) "\n",                                           \
              __FILE__, __LINE__, __func__);                                   \
      print_stacktrace(ctx);                                                   \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define slice_init_capacity 16

#define slice(T)                                                               \
  struct {                                                                     \
    T *items;                                                                  \
    usz capacity, size;                                                        \
  }

#define slice_init(s)                                                          \
  do {                                                                         \
    (s)->items = (__typeof__(*(s)->items) *)CBE_ALLOC(slice_init_capacity);    \
    (s)->capacity = slice_init_capacity;                                       \
    (s)->size = 0;                                                             \
  } while (0)

#define slice_push(s, ...)                                                     \
  do {                                                                         \
    if ((s)->size >= (s)->capacity) {                                          \
      usz old_capacity = (s)->capacity;                                        \
      (s)->capacity *= 2;                                                      \
      (s)->items = (__typeof__(*(s)->items) *)CBE_REALLOC(                     \
          (s)->items, old_capacity, (s)->capacity);                            \
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

// #define CBE_ASSERTION_STACKTRACE

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

enum {
  CBE_REG_EAX,
  CBE_REG_EBX,
  CBE_REG_ECX,
  CBE_REG_EDX,

  CBE_REG_ESI,
  CBE_REG_EDI,
  CBE_REG_EBP,
  CBE_REG_ESP,

  CBE_REG_R8,
  CBE_REG_R9,
  CBE_REG_R10,
  CBE_REG_R11,
  CBE_REG_R12,
  CBE_REG_R13,
  CBE_REG_R14,
  CBE_REG_R15,

  CBE_REG_COUNT,
};

struct cbe_register {
  usz index;
  cstr name;
  bool used;
};

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
  union {
    cbe_type_id ptr;
  };
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

enum cbe_instruction_tag {
  CBE_INST_ALLOC, /* %0 = alloc <type> */
  CBE_INST_STORE, /* store <typed value>, <typed value> */
  CBE_INST_LOAD,  /* */
  CBE_INST_RET,   /* */
};
struct cbe_instruction {
  enum cbe_instruction_tag tag;
  struct {
    usz name_index;
    cbe_type_id type_id;
  } temporary;
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
  struct cbe_register registers[CBE_REG_COUNT];
  slice(struct cbe_stack_frame) stacktrace;
  slice(struct cbe_function) functions;
  slice(struct cbe_stack_variable) stack_variables;
  slice(struct cbe_global_variable) global_variables;
  slice(struct cbe_type) types;
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

usz cbe_next_unused_register(struct cbe_context *);
usz cbe_allocate_register(struct cbe_context *);
void cbe_free_register(struct cbe_context *, usz);

usz cbe_allocate_stack_variable(struct cbe_context *, usz);
usz cbe_find_stack_variable(struct cbe_context *, usz);

usz cbe_new_global_variable(struct cbe_context *, struct cbe_global_variable);

cbe_type_id cbe_add_type(struct cbe_context *, struct cbe_type);

usz cbe_find_or_add_symbol(struct cbe_context *, cstr);
usz cbe_find_symbol(struct cbe_context *, cstr);
usz cbe_add_symbol(struct cbe_context *, cstr);

void cbe_generate(struct cbe_context *, FILE *);
void cbe_generate_global_variable(struct cbe_context *, FILE *,
                                  struct cbe_global_variable);
void cbe_generate_function(struct cbe_context *, FILE *, struct cbe_function);
void cbe_generate_block(struct cbe_context *, FILE *, struct cbe_block);
void cbe_generate_instruction(struct cbe_context *, FILE *,
                              struct cbe_instruction);
void cbe_generate_value(struct cbe_context *, FILE *, struct cbe_value);
void cbe_generate_type(struct cbe_context *, FILE *, struct cbe_type);

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
