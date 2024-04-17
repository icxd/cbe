#ifndef CBE_H
#define CBE_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CBE_ALLOC
#define CBE_ALLOC a_alloc
#endif // CBE_ALLOC

#ifndef CBE_REALLOC
#define CBE_REALLOC a_realloc
#endif // CBE_REALLOC

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

typedef const char *cstr;
typedef unsigned long usz;

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

struct cbe_stack_variable {
  // [rsp - (8 * (slot + 1))]
  usz slot;
};

enum cbe_type_tag {
  CBE_TYPE_BYTE,
  CBE_TYPE_SHORT,
  CBE_TYPE_INT,
  CBE_TYPE_LONG,

  CBE_TYPE_RAWPTR,
  CBE_TYPE_PTR,
};

typedef usz cbe_type_id;
struct cbe_type {
  enum cbe_type_tag tag;
  union {
    cbe_type_id ptr;
  };
};

struct cbe_global_variable {
  bool constant; // Basically just specifies if the global is defined in the
                 // rodata or data section.
};

struct cbe_context {
  struct cbe_register registers[CBE_REG_COUNT];
  slice(struct cbe_stack_variable) stack_variables;
  slice(struct cbe_global_variable) global_variables;
};

void cbe_init(struct cbe_context *);

usz cbe_next_unused_register(struct cbe_context *);
usz cbe_allocate_register(struct cbe_context *);
void cbe_free_register(struct cbe_context *, usz);

usz cbe_allocate_stack_variable(struct cbe_context *);

const char *cbe_register_name(usz);

void cbe_debug_registers(struct cbe_context *);
void cbe_debug_stack_variables(struct cbe_context *);

#endif // CBE_H
