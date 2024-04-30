#include "cbe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cbe_init(struct cbe_context *ctx) {
  slice_init(&ctx->stacktrace);
  push_stack_frame(ctx);
  ctx->register_pool = (struct cbe_register_pool){
      .head = 0,
      .registers = {CBE_REG_EAX, CBE_REG_EBX},
      .registers_count = 2,
  };
  ctx->current_stack_location = 0;
  slice_init(&ctx->functions);
  slice_init(&ctx->stack_variables);
  slice_init(&ctx->global_variables);
  slice_init(&ctx->types);

  slice_init(&ctx->live_intervals);
  slice_init(&ctx->active_intervals);
  ctx->ip = 0;

  slice_init(&ctx->symbol_table);
  slice_init(&ctx->string_table);
  pop_stack_frame(ctx);
}

cbe_live_intervals
cbe_expire_old_intervals(struct cbe_context *ctx,
                         cbe_live_intervals active_intervals,
                         struct cbe_live_interval *interval) {
  qsort(active_intervals.items, sizeof(struct cbe_live_interval),
        active_intervals.size, cbe_sort_by_end_point);
  for (usz i = 0; i < active_intervals.size; i++) {
    struct cbe_live_interval *active_interval = active_intervals.items[i];
    if (active_interval->end_point >= interval->start_point)
      return active_intervals;
    cbe_free_register(&ctx->register_pool, active_interval->symbol.reg);
    cbe_delete_interval(&active_intervals, i);
    return active_intervals;
  }
  return active_intervals;
}

void cbe_spill_at_interval(struct cbe_context *ctx,
                           cbe_live_intervals active_intervals,
                           struct cbe_live_interval *interval) {
  qsort(active_intervals.items, sizeof(struct cbe_live_interval),
        active_intervals.size, cbe_sort_by_end_point);
  struct cbe_live_interval *spill =
      active_intervals.items[active_intervals.size - 1];
  if (spill->end_point > interval->end_point) {
    CBE_DEBUG("ACTION: SPILL INTERVAL (%p)\n", (void *)spill);
    CBE_DEBUG("ACTION: ALLOCATE REGISTER %s(%d) TO INTERVAL (%p)\n",
              cbe_get_register_name(spill->symbol.reg), spill->symbol.reg,
              (void *)interval);
    interval->symbol.reg = spill->symbol.reg;
    spill->symbol.location = ctx->current_stack_location;
    cbe_delete_interval(&active_intervals, active_intervals.size - 1);
  } else {
    CBE_DEBUG("ACTION: SPILL INTERVAL (%p)\n", (void *)interval);
    interval->symbol.location = ctx->current_stack_location;
  }
  ctx->current_stack_location += 4;
}

static cstr registers[] = {
    "None", "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
};

cstr cbe_get_register_name(enum cbe_register reg) { return registers[reg]; }

static void *cbe_slice_array(void *arr, int elementSize, int start, int end) {
  int size = (end - start + 1) * elementSize;
  void *result = malloc(size);

  if (result == NULL) {
    printf("Memory allocation failed\n");
    exit(1);
  }

  char *src = (char *)arr + start * elementSize;
  memcpy(result, src, size);

  return result;
}

enum cbe_register cbe_get_register(struct cbe_register_pool *pool) {
  if (pool->registers_count == 0)
    return CBE_REG_ERROR;

  enum cbe_register reg = pool->registers[0];
  __auto_type regs = pool->registers;
  int size = (CBE_ARRAY_LEN(regs) - 1 + 1) * sizeof(enum cbe_register);
  char *src = (char *)regs + 1 * sizeof(enum cbe_register);
  memcpy(pool->registers, src, size);
  pool->registers_count--;

  return reg;
}

void cbe_free_register(struct cbe_register_pool *pool, enum cbe_register reg) {
  for (usz i = 0; i < CBE_ARRAY_LEN(pool->registers); i++) {
    if (pool->registers[i] == reg)
      return;
  }

  pool->registers[pool->registers_count++] = reg;
}

bool cbe_register_pool_is_empty(struct cbe_register_pool *pool) {
  return pool->registers_count == 0;
}

int cbe_sort_by_end_point(const void *a, const void *b) {
  return (*(struct cbe_live_interval *)a).end_point <
         (*(struct cbe_live_interval *)b).end_point;
}

void cbe_delete_interval(cbe_live_intervals *intervals, usz index) {
  cbe_live_intervals new_intervals;
  slice_init_with_capacity(&new_intervals, intervals->size);

  for (usz i = 0; i < intervals->size; i++) {
    if (i == index)
      continue;
    slice_push(&new_intervals, intervals->items[i]);
  }

  intervals->items = new_intervals.items;
  intervals->size = new_intervals.size;
}

usz cbe_allocate_stack_variable(struct cbe_context *ctx, usz name_index) {
  push_stack_frame(ctx);
  usz index = ctx->stack_variables.size;
  struct cbe_value stored_value = {
      .tag = CBE_VALUE_NIL,
      .type_id = cbe_add_type(ctx, (struct cbe_type){.tag = CBE_TYPE_RAWPTR})};
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

usz cbe_find_interval(struct cbe_context *ctx, cstr interval_name) {
  push_stack_frame(ctx);
  for (usz i = 0; i < ctx->live_intervals.size; i++) {
    struct cbe_live_interval interval = ctx->live_intervals.items[i];
    if (interval.symbol.name == interval_name)
      pop_stack_frame(ctx);
    return i;
  }
  pop_stack_frame(ctx);
  return SIZE_MAX;
}

struct cbe_live_interval
cbe_add_or_increment_live_interval(struct cbe_context *ctx,
                                   cstr interval_name) {
  push_stack_frame(ctx);
  usz interval_id = cbe_find_interval(ctx, interval_name);
  if (interval_id == SIZE_MAX) {
    slice_push(&ctx->live_intervals,
               (struct cbe_live_interval){
                   .symbol =
                       (struct cbe_register_symbol){
                           .name = interval_name,
                           .reg = cbe_get_register(&ctx->register_pool),
                           .location = -1},
                   .start_point = ctx->ip,
                   .end_point = ctx->ip,
                   .location = -1,
               });
    interval_id = ctx->live_intervals.size - 1;
  }
  struct cbe_live_interval interval = ctx->live_intervals.items[interval_id];
  interval.end_point += 1;
  pop_stack_frame(ctx);
  return interval;
}

void cbe_generate(struct cbe_context *ctx, FILE *fp) {
  push_stack_frame(ctx);
  for (usz i = 0; i < ctx->functions.size; i++) {
    struct cbe_function fn = ctx->functions.items[i];
    cbe_generate_function(ctx, fp, fn);
  }
  for (usz i = 0; i < ctx->global_variables.size; i++) {
    struct cbe_global_variable variable = ctx->global_variables.items[i];
    cbe_generate_global_variable(ctx, fp, variable);
  }
  pop_stack_frame(ctx);
}

void cbe_generate_global_variable(struct cbe_context *ctx, FILE *fp,
                                  struct cbe_global_variable variable) {
  push_stack_frame(ctx);
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
  // printf("%u\n", inst.tag);
  switch (inst.tag) {
  case CBE_INST_ALLOC:
    (void)cbe_allocate_stack_variable(ctx, inst.temporary.name_index);
    break; /* %0 = alloc <type> */

  case CBE_INST_STORE: {
    __auto_type store = inst.store;
    CBE_ASSERT(*ctx, store.pointer.tag == CBE_VALUE_VARIABLE);
    usz index = cbe_find_stack_variable(ctx, store.pointer.variable);
    CBE_ASSERT(*ctx, index != SIZE_MAX);
    struct cbe_stack_variable stack_variable =
        ctx->stack_variables.items[index];
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

    // enum cbe_register reg = cbe_get_register(&ctx->register_pool);
    // fprintf(fp, "  mov %s, ");
    // cbe_generate_value(
    //     ctx, fp,
    //     (struct cbe_value){.tag = CBE_VALUE_VARIABLE,
    //                        .variable =
    //                        stack_variable.associated_name_index});
    // fprintf(fp, "\n");
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

enum cbe_validation_result cbe_validate(struct cbe_context *ctx) {
  push_stack_frame(ctx);
  for (usz i = 0; i < ctx->functions.size; i++) {
    struct cbe_function fn = ctx->functions.items[i];
    cbe_validate_function(ctx, fn);
  }

  for (usz i = 0; i < ctx->live_intervals.size; i++) {
    struct cbe_live_interval interval = ctx->live_intervals.items[i];
    CBE_DEBUG(
        "(%p). SYMBOL: %s | LOCATION: %d | STARTPOINT: %d | ENDPOINT: %d\n",
        &ctx->live_intervals.items[i], interval.symbol.name,
        interval.symbol.location, interval.start_point, interval.end_point);

    cbe_expire_old_intervals(ctx, ctx->active_intervals,
                             &ctx->live_intervals.items[i]);

    if (cbe_register_pool_is_empty(&ctx->register_pool)) {
      cbe_spill_at_interval(ctx, ctx->active_intervals,
                            &ctx->live_intervals.items[i]);
    } else {
      enum cbe_register reg = cbe_get_register(&ctx->register_pool);
      CBE_DEBUG("ACTION: ALLOCATE REGISTER %s(%d) TO INTERVAL (%p)\n",
                cbe_get_register_name(reg), reg, &ctx->live_intervals.items[i]);
      if (reg != CBE_REG_ERROR)
        ctx->live_intervals.items[i].symbol.reg = reg;
      slice_push(&ctx->active_intervals, &ctx->live_intervals.items[i]);
    }
  }

  pop_stack_frame(ctx);
  return CBE_VALID_OK;
}

enum cbe_validation_result cbe_validate_function(struct cbe_context *ctx,
                                                 struct cbe_function fn) {
  push_stack_frame(ctx);
  for (usz i = 0; i < fn.blocks.size; i++) {
    ctx->ip = 0;
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

  if (inst.has_temporary) {
    struct cbe_live_interval live_interval = cbe_add_or_increment_live_interval(
        ctx, ctx->symbol_table.items[inst.temporary.name_index]);
  }

  ctx->ip++;

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
