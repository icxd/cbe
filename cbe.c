#include "cbe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cbe_init(struct cbe_context *ctx) {
  slice_init(&ctx->stacktrace);
  push_stack_frame(ctx);
  for (usz i = 0; i < CBE_REG_COUNT; i++)
    ctx->registers[i] =
        (struct cbe_register){i, cbe_register_name(ctx, i), false};
  slice_init(&ctx->functions);
  slice_init(&ctx->stack_variables);
  slice_init(&ctx->global_variables);
  slice_init(&ctx->types);
  slice_init(&ctx->symbol_table);
  slice_init(&ctx->string_table);
  pop_stack_frame(ctx);
}

usz cbe_next_unused_register(struct cbe_context *ctx) {
  push_stack_frame(ctx);
  for (usz i = 0; i < CBE_REG_COUNT; i++) {
    struct cbe_register r = ctx->registers[i];
    if (!r.used) {
      pop_stack_frame(ctx);
      return r.index;
    }
  }
  pop_stack_frame(ctx);
  return SIZE_MAX;
}

usz cbe_allocate_register(struct cbe_context *ctx) {
  push_stack_frame(ctx);
  usz index = cbe_next_unused_register(ctx);
  if (index == SIZE_MAX) {
    pop_stack_frame(ctx);
    return SIZE_MAX;
  }
  ctx->registers[index].used = true;
  pop_stack_frame(ctx);
  return index;
}

void cbe_free_register(struct cbe_context *ctx, usz index) {
  push_stack_frame(ctx);
  ctx->registers[index].used = false;
  pop_stack_frame(ctx);
}

usz cbe_allocate_stack_variable(struct cbe_context *ctx, usz name_index) {
  push_stack_frame(ctx);
  usz index = ctx->stack_variables.size;
  struct cbe_value stored_value = {
      .tag = CBE_VALUE_NIL,
      .type_id = cbe_add_type(ctx, (struct cbe_type){CBE_TYPE_RAWPTR})};
  slice_push(&ctx->stack_variables, (struct cbe_stack_variable){
                                        .associated_name_index = name_index,
                                        .slot = index,
                                        .stored_value = stored_value,
                                    });
  pop_stack_frame(ctx);
  return index;
}

usz cbe_find_stack_variable(struct cbe_context *ctx, usz name_index) {
  push_stack_frame(ctx);
  for (usz i = 0; i < ctx->stack_variables.size; i++) {
    struct cbe_stack_variable var = ctx->stack_variables.items[i];
    if (var.associated_name_index == name_index) {
      pop_stack_frame(ctx);
      return i;
    }
  }
  pop_stack_frame(ctx);
  return SIZE_MAX;
}

usz cbe_new_global_variable(struct cbe_context *ctx,
                            struct cbe_global_variable variable) {
  push_stack_frame(ctx);
  usz index = ctx->global_variables.size;
  slice_push(&ctx->global_variables, variable);
  pop_stack_frame(ctx);
  return index;
}

cbe_type_id cbe_add_type(struct cbe_context *ctx, struct cbe_type type) {
  push_stack_frame(ctx);
  usz index = ctx->types.size;
  slice_push(&ctx->types, type);
  pop_stack_frame(ctx);
  return index;
}

usz cbe_find_or_add_symbol(struct cbe_context *ctx, cstr symbol) {
  push_stack_frame(ctx);
  usz index = cbe_find_symbol(ctx, symbol);
  if (index != SIZE_MAX) {
    pop_stack_frame(ctx);
    return index;
  }

  pop_stack_frame(ctx);
  return cbe_add_symbol(ctx, symbol);
}

usz cbe_find_symbol(struct cbe_context *ctx, cstr symbol) {
  push_stack_frame(ctx);
  for (usz i = 0; i < ctx->symbol_table.size; i++) {
    cstr s = ctx->symbol_table.items[i];
    if (strcmp(symbol, s) == 0) {
      pop_stack_frame(ctx);
      return i;
    }
  }

  pop_stack_frame(ctx);
  return SIZE_MAX;
}

usz cbe_add_symbol(struct cbe_context *ctx, cstr symbol) {
  push_stack_frame(ctx);
  usz index = ctx->symbol_table.size;
  slice_push(&ctx->symbol_table, symbol);
  pop_stack_frame(ctx);
  return index;
}

void cbe_generate(struct cbe_context *ctx, FILE *fp) {
  push_stack_frame(ctx);
  for (usz i = 0; i < ctx->functions.size; i++) {
    struct cbe_function fn = ctx->functions.items[i];
    cbe_generate_function(ctx, fp, fn);
  }
  pop_stack_frame(ctx);
}

void cbe_generate_function(struct cbe_context *ctx, FILE *fp,
                           struct cbe_function fn) {
  push_stack_frame(ctx);
  fprintf(fp, "%s:\n", ctx->symbol_table.items[fn.name_index]);
  for (usz i = 0; i < fn.blocks.size; i++) {
    struct cbe_block block = fn.blocks.items[i];
    cbe_generate_block(ctx, fp, block);
  }
  pop_stack_frame(ctx);
}

void cbe_generate_block(struct cbe_context *ctx, FILE *fp,
                        struct cbe_block block) {
  push_stack_frame(ctx);
  fprintf(fp, ".%s:\n", ctx->symbol_table.items[block.name_index]);
  for (usz i = 0; i < block.instructions.size; i++) {
    struct cbe_instruction instruction = block.instructions.items[i];
    cbe_generate_instruction(ctx, fp, instruction);
  }
  pop_stack_frame(ctx);
}

void cbe_generate_instruction(struct cbe_context *ctx, FILE *fp,
                              struct cbe_instruction inst) {
  push_stack_frame(ctx);
  switch (inst.tag) {
  case CBE_INST_ALLOC:
    // Do nothing since the allocation happens in the validator
    break; /* %0 = alloc <type> */

  case CBE_INST_STORE: {
    __auto_type store = inst.store;
    CBE_ASSERT(*ctx, store.pointer.tag == CBE_VALUE_VARIABLE);
    printf("%zu\n", store.pointer.variable);
    usz index = cbe_find_stack_variable(ctx, store.pointer.variable);
    CBE_ASSERT(*ctx, index != SIZE_MAX);
    struct cbe_stack_variable stack_variable =
        ctx->stack_variables.items[index];
    stack_variable.stored_value = store.value;
    fprintf(fp, "  mov ");
    cbe_generate_value(
        ctx, fp,
        (struct cbe_value){.tag = CBE_VALUE_VARIABLE,
                           .variable = stack_variable.associated_name_index});
    fprintf(fp, ", ");
    cbe_generate_value(ctx, fp, store.value);
    fprintf(fp, "\n");
  } break; /* store <typed value>, <typed temporary> */

  case CBE_INST_LOAD: {
    __auto_type load = inst.load;
    CBE_ASSERT(*ctx, load.pointer.tag == CBE_VALUE_VARIABLE);
    usz index = cbe_find_stack_variable(ctx, load.pointer.variable);
    CBE_ASSERT(*ctx, index != SIZE_MAX);
    struct cbe_stack_variable stack_variable =
        ctx->stack_variables.items[index];
    fprintf(fp, "  mov ");
    cbe_generate_value(
        ctx, fp,
        (struct cbe_value){.tag = CBE_VALUE_VARIABLE,
                           .variable = inst.temporary.name_index});
    fprintf(fp, ", ");
    cbe_generate_value(
        ctx, fp,
        (struct cbe_value){.tag = CBE_VALUE_VARIABLE,
                           .variable = stack_variable.associated_name_index});
    fprintf(fp, "\n");
  } break; /* %0 = load <typed temporary> */

  case CBE_INST_RET: {
  } break; /* */
  }
  pop_stack_frame(ctx);
}

void cbe_generate_value(struct cbe_context *ctx, FILE *fp,
                        struct cbe_value value) {
  push_stack_frame(ctx);
  switch (value.tag) {
  case CBE_VALUE_NIL:
    fprintf(fp, "0");
    break;

  case CBE_VALUE_INTEGER:
    fprintf(fp, "%lld", value.integer);
    break;

  case CBE_VALUE_STRING: {
    usz string_index = ctx->string_table.size;
    slice_push(&ctx->string_table, value.string);
    fprintf(fp, "str__%zu", string_index);
  } break;

  case CBE_VALUE_VARIABLE: {
    CBE_ASSERT(*ctx, cbe_find_stack_variable(ctx, value.variable) != SIZE_MAX);
    fprintf(fp, "[rsp - %zu]", 4 * (value.variable + 1));
    break;
  }
  }
  pop_stack_frame(ctx);
}

void cbe_generate_type(struct cbe_context *ctx, FILE *fp,
                       struct cbe_type type) {}

enum cbe_validation_result cbe_validate_function(struct cbe_context *ctx,
                                                 struct cbe_function fn) {
  push_stack_frame(ctx);
  for (usz i = 0; i < fn.blocks.size; i++) {
    struct cbe_block block = fn.blocks.items[i];
    cbe_validate_block(ctx, block);
  }
  pop_stack_frame(ctx);
  return CBE_VALID_OK;
}

enum cbe_validation_result cbe_validate_block(struct cbe_context *ctx,
                                              struct cbe_block block) {
  push_stack_frame(ctx);
  for (usz i = 0; i < block.instructions.size; i++) {
    struct cbe_instruction instruction = block.instructions.items[i];
    cbe_validate_instruction(ctx, instruction);
  }
  pop_stack_frame(ctx);
  return CBE_VALID_OK;
}

enum cbe_validation_result
cbe_validate_instruction(struct cbe_context *ctx, struct cbe_instruction inst) {
  push_stack_frame(ctx);
  switch (inst.tag) {
  case CBE_INST_ALLOC:
    printf("allocating for %zu\n", inst.temporary.name_index);
    (void)cbe_allocate_stack_variable(ctx, inst.temporary.name_index);
    break;

  default:
    break;
  }
  pop_stack_frame(ctx);
  return CBE_VALID_OK;
}

enum cbe_validation_result cbe_validate_value(struct cbe_context *ctx,
                                              struct cbe_value value) {
  return CBE_VALID_OK;
}

enum cbe_validation_result cbe_validate_type(struct cbe_context *ctx,
                                             struct cbe_type type) {
  return CBE_VALID_OK;
}

const char *cbe_register_name(struct cbe_context *ctx, usz index) {
  push_stack_frame(ctx);
  if (index >= CBE_REG_COUNT) {
    pop_stack_frame(ctx);
    return "(null)";
  }
  static const char *names[] = {"eax", "ebx", "ecx", "edx", "esi", "edi",
                                "ebp", "esp", "r8",  "r9",  "r10", "r11",
                                "r12", "r13", "r14", "r15"};
  pop_stack_frame(ctx);
  return names[index];
}

void cbe_debug_registers(struct cbe_context *ctx) {
  push_stack_frame(ctx);
  printf("Registers: \n  ");
  for (usz i = 0; i < CBE_REG_COUNT; i++) {
    struct cbe_register r = ctx->registers[i];
    printf("%4s (%s)", cbe_register_name(ctx, i), r.used ? "used" : "free");

    if ((i + 1) % 4 == 0)
      printf("\n  ");
    else
      printf(", ");
  }
  printf("\n");
  pop_stack_frame(ctx);
}

void cbe_debug_stack_variables(struct cbe_context *ctx) {
  push_stack_frame(ctx);
  printf("Stack-allocated variables: \n");
  for (usz i = 0; i < ctx->stack_variables.size; i++) {
    struct cbe_stack_variable variable = ctx->stack_variables.items[i];
    printf("  {name_index = %zu, slot = %zu}\n", variable.associated_name_index,
           variable.slot);
  }
  printf("\n");
  pop_stack_frame(ctx);
}

void cbe_debug_symbol_table(struct cbe_context *ctx) {
  push_stack_frame(ctx);
  printf("Symbol table:\n");
  for (usz i = 0; i < ctx->symbol_table.size; i++) {
    cstr symbol = ctx->symbol_table.items[i];
    printf("  %s (index = %zu)\n", symbol, i);
  }
  printf("\n");
  pop_stack_frame(ctx);
}
