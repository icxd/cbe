#include "cbe.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  a_init(64 * 1024);

  struct cbe_context ctx;
  cbe_init(&ctx);

  usz eax = cbe_allocate_register(&ctx);
  cbe_debug_registers(&ctx);

  (void)cbe_allocate_register(&ctx);
  cbe_debug_registers(&ctx);

  cbe_free_register(&ctx, eax);
  cbe_debug_registers(&ctx);

  (void)cbe_allocate_register(&ctx);
  cbe_debug_registers(&ctx);

  (void)cbe_allocate_stack_variable(&ctx, cbe_add_symbol(&ctx, "x"));
  cbe_debug_stack_variables(&ctx);

  cbe_type_id int_type = cbe_add_type(&ctx, (struct cbe_type){CBE_TYPE_INT});
  cbe_type_id int_ptr_type = cbe_add_type(
      &ctx, (struct cbe_type){
                .tag = CBE_TYPE_PTR,
                .ptr = cbe_add_type(&ctx, (struct cbe_type){CBE_TYPE_INT})});

  struct cbe_instruction alloc = {
      .tag = CBE_INST_ALLOC,
      .temporary = {.name_index = cbe_add_symbol(&ctx, "ptr"),
                    .type_id = int_ptr_type},
      .alloc = {int_type},
  };
  cbe_generate_instruction(&ctx, stdout, alloc);

  struct cbe_instruction store = {
      .tag = CBE_INST_STORE,
      .store = {
          .value = (struct cbe_value){.tag = CBE_VALUE_INTEGER,
                                      .type_id = int_type,
                                      .integer = 123},
          .pointer =
              (struct cbe_value){.tag = CBE_VALUE_VARIABLE,
                                 .type_id = int_ptr_type,
                                 .variable = cbe_find_symbol(&ctx, "ptr")},
      }};

  cbe_generate_instruction(&ctx, stdout, store);
  cbe_debug_stack_variables(&ctx);
}
