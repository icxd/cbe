#include "cbe.h"
#include <stdio.h>
#include <string.h>

void cbe_init(struct cbe_context *ctx) {
  for (usz i = 0; i < CBE_REG_COUNT; i++)
    ctx->registers[i] = (struct cbe_register){i, cbe_register_name(i), false};
  slice_init(&ctx->stack_variables);
  slice_init(&ctx->global_variables);
  slice_init(&ctx->symbol_table);
}

usz cbe_next_unused_register(struct cbe_context *ctx) {
  for (usz i = 0; i < CBE_REG_COUNT; i++) {
    struct cbe_register r = ctx->registers[i];
    if (!r.used)
      return r.index;
  }
  return SIZE_MAX;
}

usz cbe_allocate_register(struct cbe_context *ctx) {
  usz index = cbe_next_unused_register(ctx);
  if (index == SIZE_MAX)
    return SIZE_MAX;
  ctx->registers[index].used = true;
  return index;
}

void cbe_free_register(struct cbe_context *ctx, usz index) {
  ctx->registers[index].used = false;
}

usz cbe_allocate_stack_variable(struct cbe_context *ctx) {
  usz index = ctx->stack_variables.size;
  slice_push(&ctx->stack_variables, (struct cbe_stack_variable){.slot = index});
  return index;
}

usz cbe_new_global_variable(struct cbe_context *ctx,
                            struct cbe_global_variable variable) {
  usz index = ctx->global_variables.size;
  slice_push(&ctx->global_variables, variable);
  return index;
}

usz cbe_find_or_add_symbol(struct cbe_context *ctx, cstr symbol) {
  usz index = cbe_find_symbol(ctx, symbol);
  if (index != SIZE_MAX)
    return index;

  return cbe_add_symbol(ctx, symbol);
}

usz cbe_find_symbol(struct cbe_context *ctx, cstr symbol) {
  for (usz i = 0; i < ctx->symbol_table.size; i++) {
    cstr s = ctx->symbol_table.items[i];
    if (strncmp(s, symbol, strlen(symbol)) == 0)
      return i;
  }

  return SIZE_MAX;
}

usz cbe_add_symbol(struct cbe_context *ctx, cstr symbol) {
  slice_push(&ctx->symbol_table, symbol);
  return ctx->symbol_table.size - 1;
}

const char *cbe_register_name(usz index) {
  if (index >= CBE_REG_COUNT)
    return "(null)";
  static const char *names[] = {"eax",  "ebx",  "ecx",  "edx", "esi",  "edi",
                                "ebp",  "esp",  "r8d",  "r9d", "r10d", "r11d",
                                "r12d", "r13d", "r14d", "r15d"};
  return names[index];
}

void cbe_debug_registers(struct cbe_context *ctx) {
  printf("Registers: \n  ");
  for (usz i = 0; i < CBE_REG_COUNT; i++) {
    struct cbe_register r = ctx->registers[i];
    printf("%4s (%s)", cbe_register_name(i), r.used ? "used" : "free");

    if ((i + 1) % 4 == 0)
      printf("\n  ");
    else
      printf(", ");
  }
  printf("\n");
}

void cbe_debug_stack_variables(struct cbe_context *ctx) {
  printf("Stack-allocated variables: \n");
  for (usz i = 0; i < ctx->stack_variables.size; i++) {
    struct cbe_stack_variable variable = ctx->stack_variables.items[i];
    printf("  {slot = %zu}\n", variable.slot);
  }
  printf("\n");
}
