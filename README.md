# Kanek Foundation Library (KFL)

Foundations library for the Kanek File System (KFS).

KFL provides the low-level building blocks used across KFS: tracing, hashing,
garbage-collected memory, random number generation, bitmap manipulation,
variable data types, configuration parsing, hex dumps, and panic/stack dump
facilities.

The library targets both **user space** (`-DUSER_SPACE`) and **Linux kernel
space** builds, controlled by a compile-time flag.

---

## Modules

| Header | Description |
|--------|-------------|
| `trace.h` | Logging and tracing macros with compile-time and runtime level filtering |
| `hash.h` | Order-preserving 64-bit string hash (`hash_b79`) and xxHash (`xxh32`/`xxh64`) |
| `gc.h` | Garbage-collected allocator with mark-and-sweep collection |
| `list.h` | Intrusive doubly-linked list (Linux kernel style) |
| `map.h` | Bitmap operations — bit/byte set/clear/count/find; foundation for KFS block maps |
| `krand64.h` | Fast 64-bit PRNG |
| `var.h` | Dynamic variable types: int, float, bool, string, array, dict |
| `dict.h` | Python-style hash-map (string keys → `var_t` values) |
| `panic.h` | Stack dump and panic with backtrace |
| `kfl_config.h` | Configuration file parser |
| `utils.h` | String utilities |
| `dumphex.h` | Hexadecimal memory dump |

---

## Building

All build commands run from `src/`:

```bash
cd src

make          # build libkfl.a + all test binaries
make clean    # remove all build artifacts
make runtests # build everything and run all tests
```

The static library produced is **`libkfl.a`**. Link against it with `-lkfl`.

**Compiler flags:**
- `-Wall -DUSER_SPACE -g -O0` — warnings on, user-space build, debug symbols
- `-rdynamic` — included in test binary links so `backtrace_symbols()` resolves function names
- `-fsanitize=address` / `-static-libasan` can be added to `CFLAGS`/`LDFLAGS` for AddressSanitizer runs (see comment at top of Makefile)

---

## Module Descriptions

### 1. Trace and Logging

Macros for debug output and error reporting.  All macros automatically embed
`file:function:line` context.  Compile-time filtering is available via
`-DTRACE_MIN_LEVEL=TRC_LVL_*`; the default passes all levels.

```c
TRACE_DBG(fmt, ...)       // debug message → stdout
TRACE_ERR(fmt, ...)       // error message → stderr
TRACE_SYSERR(fmt, ...)    // error + errno + strerror → stderr
TRACE_ERRNO(fmt, ...)     // compact errno report → stderr
TRACE(file, cls, level, buf, size, fmt, ...)  // runtime-filtered
```

Levels: `TRC_LVL_ALL` → `TRC_LVL_DEBUG` → `TRC_LVL_INFO` → `TRC_LVL_NOTICE`
→ `TRC_LVL_WARNING` → `TRC_LVL_ERROR` → `TRC_LVL_CRITICAL` → `TRC_LVL_ALERT`
→ `TRC_LVL_EMERGENCY`

### 2. Hashing

`hash_b79` encodes up to 10 characters into a 64-bit integer that preserves
lexicographic order — suitable for sorted string lookups, not just equality:

```
hash_b79("cat") < hash_b79("dog") < hash_b79("duck")
```

`xxh32` and `xxh64` are the xxHash family — fast, high-quality general-purpose
hashes used internally for dict bucket selection.

### 3. Garbage Collection

A linked-list allocator that tracks every allocation.  All allocations belong
to a `gc_list_t` context.  The mark-and-sweep interface lets callers mark nodes
for collection, then sweep to free them in one pass.

```c
gc_list_t gc;
gc_list_init(&gc);

void *p  = gc_malloc(&gc, 64);
char *s  = gc_strdup(&gc, "hello");
p        = gc_realloc(&gc, p, 128);

gc_mark(p);        // mark p as garbage
gc_sweep(&gc);     // free all marked nodes

gc_list_destroy(&gc);  // free everything remaining
```

### 4. Fast 64-bit PRNG

A fast, non-cryptographic 64-bit pseudo-random number generator.

```c
set_kseed64(12345);
uint64_t r = krand64(100);   // 0 ≤ r < 100
```

### 5. Variable Data Types and Dictionaries

Dynamic typed variables (`var_t`) and Python-style hash-maps (`dict_t`).

Supported types: `VAR_NULL`, `VAR_INT` (int64), `VAR_FLOAT` (double),
`VAR_BOOL`, `VAR_STR`, `VAR_ARRAY`, `VAR_DICT`.

```c
gc_list_t gc;
gc_list_init(&gc);

var_t *i = var_int(&gc, 42);
var_t *s = var_str(&gc, "hello");
var_t *a = var_array(&gc);
var_push(&gc, a, i);
var_push(&gc, a, s);

var_t *d = var_dict_new(&gc);
dict_set(d->dict, "key", var_int(&gc, 99));

var_print(a);   // [42, "hello"]
var_print(d);   // {"key": 99}
```

Arrays grow dynamically (capacity doubles on overflow).  Dictionaries use a
64-bucket hash table keyed by `xxh32`.  All allocations are tracked by the
supplied `gc_list_t`.

### 6. Configuration File Parser

Parses configuration files into a `dict_t` of `var_t` values.  Supports:

- `#` bash-style comments (full-line and inline)
- Integers, floats, booleans (`true`/`false`), quoted strings
- Array literals: `MY_ARRAY = [ 0, 1, "two", 3.14 ]`
- Variable references on the RHS: `COPY = ORIGINAL`
- Array indexing: `VAL = MY_ARRAY[2]`
- `+` string concatenation (with automatic type coercion)
- `+=` append operator
- `include filename` directive (relative paths, depth-limited)
- `display expr` directive (prints to stdout during load)

Example config file:

```bash
# project config

VERSION="1.0"
PRODUCT_DIR="/opt/myapp/"
LOG_DIR = PRODUCT_DIR + "log/"

MY_ARRAY = [ 0, 1, 2, "three", "four" ]
THIRD    = MY_ARRAY[3]          # "three"

BANNER = "MyApp v" + VERSION
BANNER += " — ready"

include secrets.cfg

display "Loaded " + VERSION
```

```c
gc_list_t gc;
gc_list_init(&gc);

kfl_cfg_t *cfg = kfl_cfg_new(&gc);
kfl_cfg_load(cfg, "project.cfg");

var_t *v = kfl_cfg_get(cfg, "VERSION");
printf("%s\n", var_to_str(&gc, v));   // 1.0

gc_list_destroy(&gc);
```

### 7. Hexadecimal Dumps

Debug utilities for memory inspection.

```c
dumphex(ptr, size);       // annotated hex + ASCII grid
dump_uint32(ptr, size);   // 32-bit word table with ASCII
dump_uint64(ptr, size);   // 64-bit word table with ASCII
```

### 8. Panic and Stack Dump

For unrecoverable errors.  Prints the message, a full backtrace (user-space
only, requires `-rdynamic`), then exits with the given return code.

```c
panic(rc, "disk full");          // print + backtrace + exit(rc)
PANIC(rc, "unexpected state");   // same, but prepends file:func:line

stackdump(stderr);               // print backtrace only, does not exit
```

In kernel-space builds (`-DUSER_SPACE` absent) the backtrace is skipped and a
note is printed instead.

### 9. Bitmap Operations

Bit-level primitives for block and inode allocation maps.

```c
unsigned char bm[16] = {0};    // 128-bit bitmap, all free

bm_set_bit(bm, 128, 5, SETBIT);         // set bit 5
bm_set_extent(bm, 128, 10, 8, SETBIT);  // set bits 10–17

uint64_t addr;
bm_find(bm, 128, 0, 128, 4, &addr);     // find 4 contiguous free bits
```

---

## Test Programs

| Binary | Tests |
|--------|-------|
| `testrand` | `krand64` distribution |
| `testhash` | `hash_b79`, `xxh32`, `xxh64` |
| `testdh` | hex dump output |
| `test_dumphex` | hex dump utilities |
| `testgc` | garbage collector |
| `testmap` | bitmap operations |
| `testvar` | variable types and dictionaries |
| `testpanic` | panic, PANIC macro, stackdump |
| `testconfig` | configuration file parser |

Run all tests:

```bash
cd src && make runtests
```

---

## License

See `LICENSE`.
