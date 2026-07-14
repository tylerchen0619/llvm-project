//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Tests unwinding where the signal handler is a VAPI function.
//
// `exit` is used as the signal handler and the unwinding is done in an atexit handler.
// `__builtin_debugtrap` is used because `raise` is also a VAPI function.

// REQUIRES: target={{.+}}-aix{{.*}}
// REQUIRES: has-filecheck

// ADDITIONAL_COMPILE_FLAGS: -fno-inline -fno-exceptions

// RUN: %{build}
// RUN: %{exec} %t.exe 2>&1 \
// RUN: | FileCheck --check-prefix=CHECK \
// RUN:       %if libunwind-assertions-enabled %{ --check-prefix=DEBUG %} \
// RUN:       %s

#include <errno.h>
#include <libunwind.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void my_atexit_handler(void) {
  fprintf(stderr, "Retrieve a cursor to `main` by stepping up.\n");
  // CHECK-LABEL: Retrieve a cursor to `main`
  unw_context_t context;
  unw_cursor_t cursor;
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);
  // Step from `my_atexit_handler` up to `exit`.
  unw_step(&cursor);
  // DEBUG-LABEL: libunwind: stepWithTBTable: Look up traceback table of func=my_atexit_handler
  // Step from `exit` (signal handler, VAPI) up to `main`.
  unw_step(&cursor);
  // DEBUG-LABEL: libunwind: stepWithTBTable: Look up traceback table of func=exit

  fprintf(stderr, "Resume `main` at the call to `trapper`.\n");
  // CHECK-LABEL: Resume `main`
  unw_resume(&cursor);
  __builtin_unreachable();
  // DEBUG: libunwind: VAPI: executing return glue
}

void trapper(void) {
  __builtin_debugtrap();
  _Exit(EXIT_FAILURE);
}

int main(void) {
  if (setenv("LIBUNWIND_PRINT_UNWINDING", "1", true) != 0) {
    perror("setenv");
    abort();
  }
  if (atexit(my_atexit_handler) != 0) {
    perror("atexit");
    abort();
  }
  if (signal(SIGTRAP, exit) == SIG_ERR) {
    perror("signal");
    abort();
  }
  trapper();
  _Exit(0);
}
