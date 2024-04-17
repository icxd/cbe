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

  (void)cbe_allocate_stack_variable(&ctx);
  cbe_debug_stack_variables(&ctx);
  (void)cbe_allocate_stack_variable(&ctx);
  cbe_debug_stack_variables(&ctx);
  (void)cbe_allocate_stack_variable(&ctx);
  cbe_debug_stack_variables(&ctx);
  (void)cbe_allocate_stack_variable(&ctx);
  cbe_debug_stack_variables(&ctx);
}
