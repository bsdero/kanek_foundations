# CLAUDE.md — Kanek Foundation Library (KFL)

## Project Overview

KFL is a C foundations library for the **Kanek File System (KFS)**. It provides low-level utilities used by the KFS storage system: tracing/logging, hashing, garbage collection, random number generation, bitmap manipulation, configuration parsing, and hex dumps.

The library targets both **user space** (`-DUSER_SPACE`) and **Linux kernel space** builds, controlled via the compile-time flag.

## Repository Layout

```
src/           All source and header files + Makefile
  *.h          Public headers (include these to use a module)
  *.c          Implementations and test programs
  Makefile     Build system for the library and tests
LICENSE
README.md
```

## Building

All build commands run from `src/`:

```bash
cd src

make          # build library + all tests + all tools
make tests    # build libkfs.a and test binaries only
make tools    # build libkfs.a and tool binaries only
make clean    # remove all build artifacts
make runtests # build everything and run filesystem smoke tests
```

The static library produced is `libkfs.a`.

**Compiler flags:**
- `-Wall -DUSER_SPACE -g -O0` — warnings on, user-space build, debug symbols, no optimization
- `-fsanitize=address` / `-static-libasan` can be added to `CFLAGS`/`LDFLAGS` for address sanitizer runs (see comment at top of Makefile)

## Modules

| Header | Implementation | Description |
|--------|---------------|-------------|
| `trace.h` | — | Logging/tracing macros (`TRACE_DBG`, `TRACE_ERR`, `TRACE_SYSERR`, `TRACE_ERRNO`, `TRACE`). Levels: ALL→EMERGENCY |
| `hash.h` | `hash.c` | `hash_b79()` — order-preserving 64-bit string hash; `xxh64()`/`xxh32()` — xxHash functions |
| `gc.h` | `gc.c` | Garbage-collected allocator: `gc_malloc`, `gc_free`, `gc_realloc`, `gc_strdup`, `gc_calloc`, `gc_mark`/`gc_sweep` (mark-and-sweep GC) |
| `list.h` | — | Intrusive doubly-linked list (Linux kernel style): `list_add`, `list_add_tail`, `list_del`, `list_for_each`, `list_for_each_safe` |
| `map.h` | `map.c` | Bitmap operations: bit/byte set/clear/count/find-gap (`bm_*` and `byte_*` functions) |
| `krand64.h` | `krand64.c` | Fast 64-bit PRNG: `set_kseed64(seed)`, `krand64(max)` |
| `utils.h` | `utils.c` | String utilities: `trim(s)` |
| — | `dumphex.c` | Hex dump utilities |
| — | `kfs_config.c` | Configuration file parser |
| — | `globals.c` | Global variables |

## Test Programs

Each module has a corresponding test binary built from `test*.c`:

| Binary | Tests |
|--------|-------|
| `testrand` | `krand64` PRNG |
| `testhash` | `hash_b79`, xxHash |
| `testdh` | hex dump (`dumphex`) |
| `testgc` | garbage collector |
| `testmap` | bitmap operations |
| `testdict` | dictionary/hash-map |

Run individual tests directly after building, e.g. `./testgc`.

## Coding Conventions

- C99/GNU C — uses GCC extensions (`typeof`, statement expressions, flexible array members)
- Header guards: `#ifndef _MODULE_H_` style
- Kernel-space portability: `#ifdef USER_SPACE` guards swap `<stdint.h>`/`<string.h>` for `<linux/types.h>`/`<linux/string.h>`
- `container_of` macro (Linux kernel style) used for list navigation in `gc.h`
- All public API functions are declared in the corresponding `.h` file

## Key Design Notes

- **`hash_b79`** is order-preserving: `hash("cat") < hash("dog") < hash("duck")` — suitable for sorted string lookups, not just equality checks
- **GC list** (`gc_list_t`) must be initialized with `gc_list_init()` before use and destroyed with `gc_list_destroy()`; all allocations are tracked per-list
- **Bitmap API** (`map.h`) operates on raw `unsigned char *` buffers and is the foundation for KFS block/inode allocation maps
- **Trace macros** print `file:function:line` context automatically; `TRACE_SYSERR` also prints `errno` and `strerror`
