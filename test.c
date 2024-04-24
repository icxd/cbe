#include "cbe.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  a_init(64 * 1024);

  struct cbe_context ctx;
  cbe_init(&ctx);

  cbe_type_id int_type = cbe_add_type(&ctx, (struct cbe_type){CBE_TYPE_INT});
  cbe_type_id int_ptr_type = cbe_add_type(
      &ctx, (struct cbe_type){
                .tag = CBE_TYPE_PTR,
                .ptr = cbe_add_type(&ctx, (struct cbe_type){CBE_TYPE_INT})});

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
      .temporary = {.name_index = cbe_add_symbol(&ctx, "ptr"),
                    .type_id = int_ptr_type},
      .alloc = {int_type}};

  struct cbe_instruction store = {
      .tag = CBE_INST_STORE,
      .store = {.value = (struct cbe_value){.tag = CBE_VALUE_INTEGER,
                                            .type_id = int_type,
                                            .integer = 123},
                .pointer = (struct cbe_value){
                    .tag = CBE_VALUE_VARIABLE,
                    .type_id = int_ptr_type,
                    .variable = cbe_find_symbol(&ctx, "ptr")}}};

  struct cbe_instruction load = {
      .tag = CBE_INST_LOAD,
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

  cbe_validate_function(&ctx, main);

  cbe_debug_registers(&ctx);
  cbe_debug_symbol_table(&ctx);
  cbe_debug_stack_variables(&ctx);

  FILE *fp = fopen("out.s", "w");
  cbe_generate(&ctx, fp);
  fclose(fp);
}
