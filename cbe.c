#include "cbe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cbe_init(struct cbe_context *ctx) {
  for (usz i = 0; i < CBE_REG_COUNT; i++)
    ctx->registers[i] = (struct cbe_register){i, cbe_register_name(i), false};
  slice_init(&ctx->functions);
  slice_init(&ctx->stack_variables);
  slice_init(&ctx->global_variables);
  slice_init(&ctx->symbol_table);
  slice_init(&ctx->string_table);
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

usz cbe_allocate_stack_variable(struct cbe_context *ctx, usz name_index) {
  usz index = ctx->stack_variables.size;
  slice_push(&ctx->stack_variables,
             (struct cbe_stack_variable){.slot = index,
                                         .associated_name_index = name_index});
  return index;
}

usz cbe_find_stack_variable(struct cbe_context *ctx, usz name_index) {
  for (usz i = 0; i < ctx->stack_variables.size; i++) {
    struct cbe_stack_variable var = ctx->stack_variables.items[i];
    if (var.associated_name_index == name_index)
      return i;
  }
  return SIZE_MAX;
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

void cbe_generate(struct cbe_context *ctx, FILE *fp) {
  for (usz i = 0; i < ctx->functions.size; i++) {
    struct cbe_function fn = ctx->functions.items[i];
    cbe_generate_function(ctx, fp, fn);
  }
}

void cbe_generate_function(struct cbe_context *ctx, FILE *fp,
                           struct cbe_function fn) {
  fprintf(fp, "%s:\n", ctx->symbol_table.items[fn.name_index]);
  for (usz i = 0; i < fn.blocks.size; i++) {
    struct cbe_block block = fn.blocks.items[i];
    cbe_generate_block(ctx, fp, block);
  }
}

void cbe_generate_block(struct cbe_context *ctx, FILE *fp,
                        struct cbe_block block) {
  fprintf(fp, ".%s:\n", ctx->symbol_table.items[block.name_index]);
  for (usz i = 0; i < block.instructions.size; i++) {
    struct cbe_instruction instruction = block.instructions.items[i];
    cbe_generate_instruction(ctx, fp, instruction);
  }
}

void cbe_generate_instruction(struct cbe_context *ctx, FILE *fp,
                              struct cbe_instruction inst) {
  switch (inst.tag) {
  case CBE_INST_ALLOC:
    (void)cbe_allocate_stack_variable(ctx, inst.temporary->name_index);
    break; /* %0 = alloc <type> */

  case CBE_INST_STORE: {

  } break; /* store <typed value>, <typed value> */

  case CBE_INST_LOAD: {
  } break; /* */

  case CBE_INST_RET: {
  } break; /* */
  }
}

void cbe_generate_value(struct cbe_context *ctx, FILE *fp,
                        struct cbe_value value) {
  switch (value.tag) {
  case CBE_VALUE_INTEGER:
    fprintf(fp, "%lld", value.integer);
    break;

  case CBE_VALUE_STRING: {
    usz string_index = ctx->string_table.size;
    slice_push(&ctx->string_table, value.string);
    fprintf(fp, "str__%zu", string_index);
  } break;

  case CBE_VALUE_VARIABLE:
    if (cbe_find_stack_variable(ctx, value.variable) != SIZE_MAX)
      fprintf(fp, "[rsp - %zu]", value.variable);
    else {
      fprintf(stderr, "TODO: cause i haven't figured it out yet.. >:(\n");
      exit(1);
    }
    break;
  }
}

void cbe_generate_type(struct cbe_context *ctx, FILE *fp,
                       struct cbe_type type) {}

enum cbe_validation_result validate_function(struct cbe_context *ctx,
                                             struct cbe_function fn) {
  return CBE_VALID_OK;
}

enum cbe_validation_result validate_block(struct cbe_context *ctx,
                                          struct cbe_block block) {
  return CBE_VALID_OK;
}

enum cbe_validation_result validate_instruction(struct cbe_context *ctx,
                                                struct cbe_instruction inst) {
  return CBE_VALID_OK;
}

enum cbe_validation_result validate_value(struct cbe_context *ctx,
                                          struct cbe_value value) {
  return CBE_VALID_OK;
}

enum cbe_validation_result validate_type(struct cbe_context *ctx,
                                         struct cbe_type type) {
  return CBE_VALID_OK;
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
