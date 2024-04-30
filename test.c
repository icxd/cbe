#include "cbe.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  a_init(64 * 1024);

  struct cbe_context ctx;
  cbe_init(&ctx);

  // {
  //   struct cbe_live_interval live_intervals[] = {
  //       (struct cbe_live_interval){
  //           .symbol = (struct cbe_register_symbol){.name = "a", .location =
  //           -1}, .start_point = 1, .end_point = 4},
  //       (struct cbe_live_interval){
  //           .symbol = (struct cbe_register_symbol){.name = "b", .location =
  //           -1}, .start_point = 2, .end_point = 6},
  //       (struct cbe_live_interval){
  //           .symbol = (struct cbe_register_symbol){.name = "c", .location =
  //           -1}, .start_point = 3, .end_point = 10},
  //       (struct cbe_live_interval){
  //           .symbol = (struct cbe_register_symbol){.name = "d", .location =
  //           -1}, .start_point = 5, .end_point = 9},
  //       (struct cbe_live_interval){
  //           .symbol = (struct cbe_register_symbol){.name = "e", .location =
  //           -1}, .start_point = 7, .end_point = 8},
  //   };

  //   cbe_live_intervals active_intervals;
  //   slice_init(&active_intervals);

  //   // FIXME: sort live intervals
  //   for (usz i = 0; i < CBE_ARRAY_LEN(live_intervals); i++) {
  //     struct cbe_live_interval interval = live_intervals[i];
  //     CBE_DEBUG(
  //         "(%p). SYMBOL: %s | LOCATION: %d | STARTPOINT: %d | ENDPOINT:
  //         %d\n", &live_intervals[i], interval.symbol.name,
  //         interval.symbol.location, interval.start_point,
  //         interval.end_point);

  //     cbe_expire_old_intervals(&ctx, active_intervals, &live_intervals[i]);

  //     if (cbe_register_pool_is_empty(&ctx.register_pool)) {
  //       cbe_spill_at_interval(&ctx, active_intervals, &live_intervals[i]);
  //     } else {
  //       enum cbe_register reg = cbe_get_register(&ctx.register_pool);
  //       CBE_DEBUG("ACTION: ALLOCATE REGISTER %s(%d) TO INTERVAL (%p)\n",
  //                 cbe_get_register_name(reg), reg, &live_intervals[i]);
  //       if (reg != CBE_REG_ERROR)
  //         live_intervals[i].symbol.reg = reg;
  //       slice_push(&active_intervals, &live_intervals[i]);
  //     }
  //   }

  //   for (usz i = 0; i < CBE_ARRAY_LEN(live_intervals); i++) {
  //     struct cbe_live_interval interval = live_intervals[i];
  //     CBE_DEBUG(
  //         "symbol{name: %s, reg: %s, location: %d}\n", interval.symbol.name,
  //         cbe_get_register_name(interval.symbol.reg),
  //         interval.symbol.location);
  //   }

  //   return 0;
  // }

  cbe_type_id int_type = cbe_add_type(&ctx, (struct cbe_type){CBE_TYPE_INT});
  cbe_type_id int_ptr_type = cbe_add_type(
      &ctx,
      (struct cbe_type){
          .tag = CBE_TYPE_PTR,
          .ptr = cbe_add_type(&ctx, (struct cbe_type){.tag = CBE_TYPE_INT})});

  struct cbe_function main = {
      .name_index = cbe_add_symbol(&ctx, "main"),
      .type_id = int_type,
  };
  slice_init(&main.blocks);

  struct cbe_block entry = {
      .name_index = cbe_add_symbol(&ctx, "entry"),
  };
  slice_init(&entry.instructions);

  struct cbe_instruction alloc = {
      .tag = CBE_INST_ALLOC,
      .has_temporary = true,
      .temporary = {.name_index = cbe_add_symbol(&ctx, "ptr"),
                    .type_id = int_ptr_type},
      .alloc = {int_type}};

  struct cbe_instruction store = {
      .tag = CBE_INST_STORE,
      .has_temporary = false,
      .store = {.value = (struct cbe_value){.tag = CBE_VALUE_INTEGER,
                                            .type_id = int_type,
                                            .integer = 123},
                .pointer = (struct cbe_value){
                    .tag = CBE_VALUE_VARIABLE,
                    .type_id = int_ptr_type,
                    .variable = cbe_find_symbol(&ctx, "ptr")}}};

  struct cbe_instruction load = {
      .tag = CBE_INST_LOAD,
      .has_temporary = true,
      .temporary = {.name_index = cbe_add_symbol(&ctx, "value"),
                    .type_id = int_type},
      .load = {.pointer = (struct cbe_value){
                   .tag = CBE_VALUE_VARIABLE,
                   .type_id = int_ptr_type,
                   .variable = cbe_find_symbol(&ctx, "ptr")}}};

  slice_push(&entry.instructions, alloc);
  slice_push(&entry.instructions, store);
  slice_push(&entry.instructions, load);

  slice_push(&main.blocks, entry);

  slice_push(&ctx.functions, main);

  cbe_validate(&ctx);

  FILE *fp = fopen("out.s", "w");
  cbe_generate(&ctx, fp);
  fclose(fp);

  cbe_debug_symbol_table(&ctx);
  cbe_debug_stack_variables(&ctx);
}
