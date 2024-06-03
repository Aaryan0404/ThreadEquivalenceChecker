#ifndef EQUIV_CHECKER_H
#define EQUIV_CHECKER_H

#include "rpi.h"
#include "equiv-checker-util.h"
#include "interleaver.h"

void equiv_checker_init(void);

void equiv_checker_run(
  function_exec *executables,
  uint32_t n_func,
  uint32_t ncs,
  init_memory_func init,
  set_t* additional_shared_memory
);
#endif
