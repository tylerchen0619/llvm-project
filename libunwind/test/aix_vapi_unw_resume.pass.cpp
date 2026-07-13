//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// REQUIRES: target={{.+}}-aix{{.*}}
// REQUIRES: has-filecheck

// ADDITIONAL_COMPILE_FLAGS: -fno-inline -fno-exceptions

// RUN: %{build}
// RUN: %{exec} %t.exe 2>&1 | FileCheck %s

// Tests use of the libunwind C API to step up from a context where the VAPI is
// active and to resume contexts where
// - the VAPI is active (and thus VAPI return glue is not called) and
// - where the VAPI is not active (and thus VAPI return glue _is_ called).
//
// In the latter case, which applies not just to the caller of the Virtual API
// but also to its ancestors, the return glue should always be called (i.e.,
// each time, without regard for whether the VAPI is currently active).

#include <assert.h>
#include <libunwind.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" void *returns_twice_bsearch
    [[gnu::returns_twice]] (const void *, const void *, size_t, size_t,
                            int (*)(const void *,
                                    const void *)) __asm__("bsearch");

static unw_cursor_t bsearch_cursor;
static unw_cursor_t bsearch_caller_cursor;
static unw_cursor_t main_cursor;

extern "C" int cmp(const void *pa, const void *pb) {
  (void)pa;
  (void)pb;

  fprintf(
      stderr,
      "Populate global cursors for `bsearch`, `bsearch_caller`, and `main`.\n");
  unw_context_t context;
  unw_cursor_t cursor;
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);
  // Step from `cmp` up to `bsearch`.
  unw_step(&cursor);
  bsearch_cursor = cursor;
  // Step from `bsearch` up to `bsearch_caller`.
  unw_step(&cursor);
  bsearch_caller_cursor = cursor;
  // Step from `bsearch_caller` up to `main`.
  unw_step(&cursor);
  main_cursor = cursor;

  // Test resuming context where VAPI is active.
  fprintf(stderr,
          "Return to `bsearch` with r3 set to 0 using the global cursor.\n");
  unw_set_reg(&bsearch_cursor, UNW_PPC64_R3, (unw_word_t)0);
  unw_resume(&bsearch_cursor);
}

char bsearch_caller_ret;
void *bsearch_caller(void) {
  volatile int state = 0;

  char c;
  char buf[3];
  assert(returns_twice_bsearch(&c, buf, 1, 1, cmp) == &buf[state]);

  // Test resuming context where VAPI is not active. The VAPI return glue should
  // be used each time without regard for whether the VAPI is currently active.
  if (++state < 3) {
    fprintf(stderr,
            "Return to `bsearch_caller` at the invocation of "
            "`returns_twice_bsearch` (really `bsearch`) with r3 set to "
            "&buf[%d] using the global cursor.\n",
            state);
    unw_set_reg(&bsearch_caller_cursor, UNW_PPC64_R3, (unw_word_t)&buf[state]);
    unw_resume(&bsearch_caller_cursor);
  }

  // Test resuming context where VAPI is not active, one frame up from the VAPI
  // caller.
  fprintf(stderr, "Return to `main` at the invocation of `bsearch_caller` with "
                  "r3 set to `&bsearch_caller_ret` using the global cursor.\n");
  unw_set_reg(&main_cursor, UNW_PPC64_R3, (unw_word_t)&bsearch_caller_ret);
  unw_resume(&main_cursor);
}

int main(void) {
  if (setenv("LIBUNWIND_PRINT_UNWINDING", "1", true) != 0) {
    perror("setenv");
    abort();
  }
  assert(bsearch_caller() == &bsearch_caller_ret);
}
