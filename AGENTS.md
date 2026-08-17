# AGENTS.md

This file provides guidance to agentic development tools when working with code in this repository.

## Project Overview

KFL (Kanek Foundation Library) is a C foundations library for the **Kanek
File System (KFS)**. It provides low-level building blocks used across KFS:
tracing, hashing, garbage-collected memory, random number generation, bitmap
manipulation, dynamic variable types, dictionaries, configuration parsing,
hex dumps, panic/stack-dump facilities, and CRC-32C checksums.

The library targets both **user space** (`-DUSER_SPACE`) and **Linux kernel
space** builds, controlled by a compile-time flag. All allocating functions
across modules take a `ta_list_t *gc` argument and track their memory in
that tracked-allocator context — see "Tracked allocator is the backbone"
below.

## Repository Layout

```
src/            All source and header files + Makefile
  *.h           Public headers (include these to use a module)
  *.c           Implementations and test programs (test*.c)
  testdata/     Fixture files for the config parser tests
  Makefile      Build system for the library and tests
API.md          Full API reference (all public symbols, by module)
CODING_STYLE.md Binding C style rules — read before writing/editing code
README.md       Module descriptions and usage examples
LICENSE
```

## Building

All build commands run from `src/`:

```bash
cd src

make          # build libkfl.a + all test binaries
make clean    # remove all build artifacts
make runtests # build everything and run the test suite
```

The static library produced is `libkfl.a`; link against it with `-lkfl`.
There is no install target — consumers link `libkfl.a` directly.

**Compiler flags:**
- `-Wall -DUSER_SPACE -g -O0` — warnings on, user-space build, debug symbols
- `-rdynamic` is included in test binary links so `backtrace_symbols()`
  resolves function names in `panic.c`
- `-fsanitize=address` / `-static-libasan` can be added to `CFLAGS`/`LDFLAGS`
  for AddressSanitizer runs (see comment at top of `Makefile`)
- `crc32c.o` is built with `-msse4.2` automatically when the compiler
  supports it (detected via a Makefile probe); falls back to the portable
  path otherwise

### Running a single test

Each module has its own `test*.c` → binary; there is no test filtering
flag, so build and run the specific binary directly:

```bash
cd src
make testmap && ./testmap     # e.g. bitmap tests only
```

`make runtests` only runs a subset (`testrand`, `testhash`, `testdh`,
`testgc`, `testta`, `testmap`, `testutils`, `testcrc32c`, `testdict`) —
`testvar`, `testpanic`, `testconfig`, and `test_dumphex` must be built and
run individually.

## Architecture

### Tracked allocator is the backbone

`ta.h`/`ta.c` implements a per-context tracked heap allocator
(`ta_list_t`). Every other allocating module — `var_t`, `dict_t`,
`kfl_cfg_t` — is built on top of it rather than calling `malloc`/`free`
directly:

```c
ta_list_t gc;
ta_list_init(&gc);
/* ... use var_*, dict_*, kfl_cfg_* with &gc ... */
ta_list_destroy(&gc);   /* frees everything allocated under this context */
```

A `ta_list_t` must be initialized before use. Nodes can also be marked
(`ta_mark`) and reclaimed in bulk with `ta_sweep()` for mark-and-sweep GC
semantics within one context. `list.h` (intrusive doubly-linked list,
Linux-kernel style) is what links tracked nodes together and underlies
`dict_t`'s buckets; `container_of` is used throughout `ta.c` for list-node
recovery.

### Layered module dependencies

```
list.h  ─┬─> ta.h ─┬─> var.h ─┬─> dict.h ─> cfg_parser.h
         └──────────┘         └─(var_dict_new wraps a dict_t)
```

- `var_t` (`var.h`) is a tagged union (`VAR_NULL/INT/FLOAT/BOOL/STR/ARRAY/DICT`)
  allocated through a `ta_list_t`. Arrays grow dynamically (capacity
  doubles on overflow).
- `dict_t` (`dict.h`) is a Python-style string-keyed hash map of `var_t *`,
  bucketed with `xxh32` (from `hash.h`); default 64 buckets
  (`dict_new`), or a caller-chosen power-of-2 bucket count via
  `dict_new_sized()` for known load factors.
- `cfg_parser.h`/`cfg_parser.c` parses config files into a `kfl_cfg_t`
  (a `dict_t` of `var_t`), supporting `#` comments, arrays, variable
  references, `+`/`+=` string concatenation, `include`, and `display`
  directives. See `README.md` for the config file grammar/example.
- `crc32c.h` is used to protect on-disk structs across the whole KANEK
  stack, not just this repo — treat its output format as a stable
  interface.
- `panic.h`/`panic.c` prints a message plus backtrace (user-space only,
  needs `-rdynamic`) and exits; in kernel-space builds the backtrace is
  skipped and a note is printed instead.
- `gc.h` is a deprecated shim that just includes `ta.h` — new code should
  include `ta.h` directly.

### Kernel/user-space portability

`#ifdef USER_SPACE` guards swap standard headers (`<stdint.h>`,
`<string.h>`) for kernel equivalents (`<linux/types.h>`,
`<linux/string.h>`) so the same source builds in Linux kernel space when
`-DUSER_SPACE` is omitted.

### Not part of the build

`src/kfs_config.c` includes `kfs.h`/`kfs_config.h`, which do not exist in
this repo, and is not referenced by the Makefile — it's a stray/orphaned
file from the downstream KFS project, not a buildable part of KFL.

## Coding conventions

**`CODING_STYLE.md` is binding and overrides this file where they
overlap** — read it before writing or editing any `.c`/`.h` file. Key
rules not obvious from reading nearby code: every control-flow body is
braced on its own line even for one statement; pointer-vs-NULL
comparisons are always explicit (`p == NULL`, never `!p`); 78-char line
cap; no tabs, 4-space indent; function-definition braces are K&R style
(same line) while control-flow braces are not; `return(value);` — always
parenthesized; `*` binds to the variable (`char *p`); comments are
`/* */`, never `//`; struct typedefs are anonymous with no `_s` tag; no
file-header banner comments; every early-return failure path logs via
`TRACE_ERR`/`TRACE_SYSERR`/`TRACE_ERRNO` first; space after `(`, none
before `)`.

Other conventions:
- C99/GNU C — uses GCC extensions (`typeof`, statement expressions,
  flexible array members)
- Header guards: `#ifndef _MODULE_H_` style
- All public API functions are declared in the corresponding `.h` file
  (cross-reference `API.md` for full signatures/semantics per module)

## Key Design Notes

- **`hash_b79`** (`hash.h`) is order-preserving:
  `hash("cat") < hash("dog") < hash("duck")` — suitable for sorted string
  lookups, not just equality checks. `xxh32`/`xxh64` are the general-purpose
  hashes (xxHash family), used internally for `dict_t` bucket selection.
- **Bitmap API** (`map.h`) operates on raw `unsigned char *` buffers
  (`bm_*`/`byte_*` functions: set/clear/count/find-gap) and is the
  foundation for KFS block/inode allocation maps.
- **Trace macros** (`trace.h`) print `file:function:line` context
  automatically; `TRACE_SYSERR` also prints `errno` and `strerror`.
  Levels run `TRC_LVL_ALL → DEBUG → INFO → NOTICE → WARNING → ERROR →
  CRITICAL → ALERT → EMERGENCY`; compile-time filtering via
  `-DTRACE_MIN_LEVEL=TRC_LVL_*`.
