# KFL API Reference

All public symbols live in `libkfl.a`.  Include the relevant header and link
with `-lkfl`.  All allocating functions that accept a `ta_list_t *gc` argument
track their memory in that tracked allocator list; call `ta_list_destroy(&gc)` to free
everything at once.

---

## Table of Contents

1. [Trace — `trace.h`](#1-trace--traceh)
2. [Hash — `hash.h`](#2-hash--hashh)
3. [Tracked Allocator — `ta.h`](#3-tracked-allocator--tah)
4. [Linked List — `list.h`](#4-linked-list--listh)
5. [Bitmap — `map.h`](#5-bitmap--maph)
6. [Random Number Generator — `krand64.h`](#6-random-number-generator--krand64h)
7. [Variable Types — `var.h`](#7-variable-types--varh)
8. [Dictionary — `dict.h`](#8-dictionary--dicth)
9. [Panic and Stack Dump — `panic.h`](#9-panic-and-stack-dump--panich)
10. [Config Parser — `cfg_parser.h`](#10-config-parser--cfg_parserh)
11. [Utilities — `utils.h`](#11-utilities--utilsh)
12. [Hex Dump — `dumphex.h`](#12-hex-dump--dumphexh)
13. [CRC-32C — `crc32c.h`](#13-crc-32c--crc32ch)

---

## 1. Trace — `trace.h`

Logging and diagnostic macros.  All macros automatically embed
`file:function:line` in the output.

**Compile-time filtering:**
Define `-DTRACE_MIN_LEVEL=TRC_LVL_*` to eliminate all macro calls below that
level at compile time.  Default is `TRC_LVL_ALL` (nothing stripped).

**Level constants** (ascending severity):

| Constant | Value |
|----------|-------|
| `TRC_LVL_ALL` | 0 |
| `TRC_LVL_DEBUG` | 1 |
| `TRC_LVL_INFO` | 2 |
| `TRC_LVL_NOTICE` | 3 |
| `TRC_LVL_WARNING` | 4 |
| `TRC_LVL_ERROR` | 5 |
| `TRC_LVL_CRITICAL` | 6 |
| `TRC_LVL_ALERT` | 7 |
| `TRC_LVL_EMERGENCY` | 8 |

**Runtime state:**

```c
trace_t global_trace;   // defined in trace.c
```

| Field | Type | Description |
|-------|------|-------------|
| `trc_class` | `uint64_t` | Bitmask of active subsystems. Set to `UINT64_MAX` to enable all. |
| `level` | `uint16_t` | Minimum level to emit at runtime. |

---

### Macros

#### `TRACE_DBG(fmt, ...)`
Prints a debug message to **stdout**.  Prefixed with `file:func:line:`.
Compiled out when `TRC_LVL_DEBUG < TRACE_MIN_LEVEL`.

#### `TRACE_ERR(fmt, ...)`
Prints an error message to **stderr**.  Prefixed with `ERROR:file:func:line:`.
Compiled out when `TRC_LVL_ERROR < TRACE_MIN_LEVEL`.

#### `TRACE_SYSERR(fmt, ...)`
Prints an error message to **stderr** including the current `errno` value and
`strerror(errno)`.  Prefixed with `SYSERR:errno:strerror:file:func:line:`.
Use immediately after a failing system call.

#### `TRACE_ERRNO(fmt, ...)`
Compact errno report to **stderr**.  Prefixed with `ERRNO: errno:strerror:`.

#### `TRACE_STR(str, size, fmt, ...)`
Formats `file:func:line: message\n` into the caller-supplied buffer `str` of
`size` bytes using `snprintf`.  Does not write to any file descriptor.

#### `TRACE(file, trc_cls, level, str, size, fmt, ...)`
Runtime-filtered trace.  Emits only when `level >= global_trace.level` AND
`(trc_cls & global_trace.trc_class) != 0`.

| Parameter | Description |
|-----------|-------------|
| `file` | `FILE *` to write to |
| `trc_cls` | Subsystem class bitmask for this call site |
| `level` | `TRC_LVL_*` severity |
| `str` | `char` buffer to format into |
| `size` | `sizeof(str)` |
| `fmt, ...` | `printf`-style format and arguments |

---

### Functions

#### `int trace(FILE *file, const char *str)`
Writes `str` to `file` and flushes.  Called internally by the `TRACE` macro;
may also be called directly.

- **Returns** `0` on success, `-1` on error.

---

## 2. Hash — `hash.h`

#### `uint64_t hash_b79(char *s)`
Order-preserving 64-bit hash of up to 10 characters of `s`.  Maps the ASCII
character set to a 79-symbol alphabet and encodes the result as a base-79
integer.

**Key property:** the numeric ordering of hash values matches lexicographic
string ordering for printable-ASCII strings up to 10 characters:

```
hash_b79("cat") < hash_b79("dog") < hash_b79("duck")
```

- **Returns** `0` if `s` is `NULL`.
- No collisions for strings that use only alphanumerics and common punctuation
  within the 10-character limit.

---

#### `uint64_t xxh64(const void *input, size_t len, uint64_t seed)`
xxHash 64-bit non-cryptographic hash.  Fast general-purpose hash suitable for
hash tables and checksums.

- **Returns** a 64-bit hash of `len` bytes starting at `input`.

---

#### `uint32_t xxh32(const void *input, size_t len, uint32_t seed)`
xxHash 32-bit variant.  Used internally for dict bucket selection.

- **Returns** a 32-bit hash of `len` bytes starting at `input`.

---

## 3. Tracked Allocator — `ta.h`

A tracked heap allocator. Every allocation belongs to a `ta_list_t` context.
All allocations in a context are freed together with `ta_list_destroy()`. Use
`ta_list_reset()` to reuse a context across repeated operations without
reallocating the context head.

Allocate the context on the stack or as a struct member, initialise with
`ta_list_init`, and destroy with `ta_list_destroy`.

```c
ta_list_t gc;
ta_list_init(&gc);
// ... use ta_malloc, ta_strdup, etc. ...
ta_list_destroy(&gc);
```

---

### Lifecycle

#### `int ta_list_init(ta_list_t *ll)`
Initialises an empty tracked allocator list.  Must be called before any allocation.

- **Returns** `0`.

#### `void ta_list_destroy(ta_list_t *ll)`
Frees every allocation in `ll` and re-initialises the list head.  After this
call the list is empty and can be reused.

#### `void ta_list_reset(ta_list_t *ll)`
Frees all allocations in `ll` and re-initialises it **without** freeing the
list head itself.  Equivalent to `ta_list_destroy()` followed by
`ta_list_init()` but avoids reallocating the list head.  Use for allocator
contexts that are reused across many repeated operations (e.g. processing one
KV lookup in a tight loop) to avoid per-operation list head alloc overhead.

#### `size_t ta_list_total_mem(ta_list_t *ll)`
Returns the sum of user-data sizes (bytes) of all live nodes in `ll`.

#### `int ta_dump_list(ta_list_t *ll)`
Prints a formatted hex dump of every node in `ll` to stdout.  Useful for
debugging memory layout and tracking individual allocations.

- **Returns** `0`.

---

### Allocation

#### `void *ta_malloc(ta_list_t *ll, size_t size)`
Allocates `size` bytes tracked by `ll`.  The returned pointer is to the
user-data area; the TA bookkeeping header is hidden before it.

- **Returns** a pointer to the allocated memory, or `NULL` on failure.

#### `void ta_free(void *p)`
Frees a single TA-tracked allocation `p` (obtained from any of the `ta_*`
allocation functions).  Unlinks `p` from its tracked allocator list.

#### `void *ta_realloc(ta_list_t *ll, void *ptr, size_t size)`
Resizes a TA-tracked allocation.  The old node is unlinked before the `realloc`
call so the list is never left with a stale pointer.  On failure the original
allocation is re-linked and returned as valid.

- **Returns** a pointer to the resized memory, or `NULL` on failure (the
  original block remains valid in that case).

#### `void *ta_calloc(ta_list_t *ll, size_t nelements, size_t elementSize)`
Allocates `nelements * elementSize` bytes, zero-initialised.

- **Returns** a pointer to the zeroed memory, or `NULL` on failure.

#### `char *ta_strdup(ta_list_t *ll, char *p)`
Duplicates the string `p` into a new TA-tracked allocation.

- **Returns** the duplicated string, or `NULL` if `p` is `NULL` or allocation fails.

#### `char *ta_strndup(ta_list_t *ll, char *p, int n)`
Duplicates the first `n` bytes of `p` into a new TA-tracked allocation and
null-terminates the result.

- **Returns** the duplicated string, or `NULL` on failure.

#### `char *ta_strncat(ta_list_t *ll, char *p, char *q)`
Appends string `q` to the TA-tracked string `p` by growing `p` in place via
`ta_realloc`.  The returned pointer replaces `p` — do not use `p` after this
call.

- **Returns** the concatenated string, or `NULL` on failure (original `p`
  remains valid).

#### `void *ta_memclone(ta_list_t *ll, void *p, int n)`
Copies `n` bytes from `p` into a new TA-tracked allocation.

- **Returns** the new copy, or `NULL` on failure.

---

### Mark and Sweep

`ta_mark` / `ta_sweep` implement a simple two-phase collection pass.  The
convention in KFL is that `mark = 1` means "collect this node".

#### `void ta_mark(void *ptr)`
Sets the mark flag on the TA node that owns `ptr`.  Marked nodes are freed by
the next `ta_sweep` call.

#### `void ta_sweep(ta_list_t *ll)`
Frees all nodes in `ll` whose mark flag is set, and unlinks them.  Unmarked
nodes are left untouched.

---

### Diagnostics

#### `void ta_node_set_trace(void *n, char *str)`
Stores a short debug label (up to `MAX_DBG_STR_LEN - 1` characters) in the TA
node that owns `n`.  The label appears in `ta_dump_list` output.

---

## 4. Linked List — `list.h`

Intrusive doubly-linked circular list, Linux-kernel style.  Embed a `list_t`
member in any struct to make it linkable without a separate allocation.

```c
typedef struct {
    list_t  node;    // must be first, or use container_of
    int     value;
} item_t;
```

---

### Macros

#### `INIT_LIST_HEAD(ptr)`
Initialises `ptr` as an empty list head (both `next` and `prev` point to
`ptr` itself).

#### `LIST(l)`
Casts `l` to `list_t *`.  Convenience for passing struct pointers to list
functions.

#### `list_for_each(pos, head)`
Iterates all nodes from `head->next` to the node before `head`.  `pos` is a
`list_t *` cursor.  **Do not delete `pos` inside this loop** — use
`list_for_each_safe` instead.

#### `list_for_each_safe(pos, n, head)`
Same as `list_for_each` but saves `pos->next` into `n` before the loop body,
making it safe to delete `pos` from the list during iteration.

#### `list_entry(ptr, type, member)`
Returns a pointer to the enclosing struct of type `type`, given that `ptr`
points to the `member` field.  Equivalent to `container_of`.

#### `container_of(ptr, type, member)`
(Defined in `gc.h`, available when `gc.h` is included.)  Same as
`list_entry` but uses `offsetof` for portability.

---

### Functions

#### `void list_add(list_t *new_entry, list_t *head)`
Inserts `new_entry` immediately after `head` (push-front / stack behaviour).

#### `void list_add_tail(list_t *new_entry, list_t *head)`
Inserts `new_entry` immediately before `head` (push-back / queue behaviour).

#### `void list_del(list_t *entry)`
Removes `entry` from its list.  Does not free memory.

#### `int list_empty(list_t *list)`
Returns non-zero if `list` is empty (points to itself).

#### `void list_reparent(list_t *list, list_t *new_entry)`
Moves all nodes from `list` to `new_entry`.  If `list` is empty, does nothing.

---

## 5. Bitmap — `map.h`

Raw bitmap operations on `unsigned char *` buffers.  The buffer must be at
least `ceil(total_bits / 8)` bytes.  Bit 0 is the least-significant bit of
byte 0.

Symbols `SETBIT` (1) and `CLEARBIT` (0) select the operation direction.

---

### Byte-level primitives

#### `unsigned char byte_set_bits(int start, int numbits, unsigned char byte, int clear_or_set)`
Sets or clears `numbits` bits in `byte` starting at bit position `start`.

- **Returns** the modified byte value.

#### `int byte_count_bits(int start, int numbits, unsigned char byte, int clear_or_set)`
Counts contiguous bits equal to `clear_or_set` in `byte`, starting at `start`,
up to `numbits`.

- **Returns** the count of matching contiguous bits.

#### `int byte_find_gap(int start, int numbits, unsigned char byte, int *count)`
Searches for a run of `numbits` zero bits in `byte` starting at `start`.

- **Returns:**
  - `>= 0` — bit offset of the gap (fully fits in this byte)
  - `-1` — no gap found; `*count == 0`
  - `-2` — end of byte reached before the gap was complete; `*count` holds the
    number of zeros found at the trailing edge

#### `int byte_get_bit(unsigned char byte, int bit)`
Returns the value (0 or 1) of bit `bit` in `byte`.

---

### Bitmap operations

#### `int bm_set_bit(unsigned char *bm, uint64_t total_bits, uint64_t bit_address, int clear_or_set)`
Sets or clears a single bit.

- **Returns** `0` on success, `-1` if `bit_address >= total_bits`.

#### `int bm_get_bit(unsigned char *bm, uint64_t total_bits, uint64_t bit_address)`
Gets the value of a single bit.

- **Returns** `0` or `1`, or `-1` if `bit_address >= total_bits`.

#### `int bm_set_extent(unsigned char *bm, uint64_t total_bits, uint64_t bit_address, uint64_t num_bits_to_set, int clear_or_set)`
Sets or clears `num_bits_to_set` contiguous bits starting at `bit_address`.

- **Returns** `0` on success, `-1` if the range is out of bounds.

#### `int bm_count(unsigned char *bm, uint64_t total_bits, uint64_t bit_address, uint64_t num_bits_to_count, int clear_or_set, uint64_t *count_result)`
Counts contiguous bits equal to `clear_or_set` starting at `bit_address`, up
to `num_bits_to_count` bits.  Stores the result in `*count_result`.

- **Returns** `0` on success, `-1` if the range is out of bounds.

#### `int bm_find(unsigned char *bm, uint64_t total_bits, uint64_t bit_address, uint64_t count, uint64_t gap_size, uint64_t *found_address)`
Finds the first run of `gap_size` contiguous zero bits, searching `count` bits
starting at `bit_address`.  Stores the bit offset of the gap in
`*found_address`.

- **Returns** `0` on success, `-1` if no gap was found.

#### `int bm_extent_can_grow(unsigned char *bm, uint64_t total_bits, uint64_t bit_address, uint64_t count)`
Tests whether the `count`-bit region starting at `bit_address` can be extended
in place (i.e. bits immediately following the current 1s are all zero).

- **Returns:**

| Code | Meaning |
|------|---------|
| `0` | Region can grow — trailing bits are all 0 |
| `-1` | Invalid arguments |
| `-2` | Region starts with zeros (not a pure extent) |
| `-3` | Region is entirely 1s with no room to grow |
| `-4` | Internal error (should not occur) |
| `-5` | Fragmented — leading 1s, then 0s, then more 1s |
| `-100` to `-102` | Internal `bm_count` errors |

---

### Whole-bitmap utilities

#### `void bm_zero(unsigned char *bm, uint64_t total_bits)`
Clears all bits (marks the entire bitmap as free).

#### `void bm_fill(unsigned char *bm, uint64_t total_bits)`
Sets all bits (marks the entire bitmap as used).

#### `uint64_t bm_popcount(unsigned char *bm, uint64_t total_bits)`
Returns the total number of set (1) bits in the bitmap.  Uses
`__builtin_popcount` on GCC for efficiency.

---

## 6. Random Number Generator — `krand64.h`

Fast non-cryptographic 64-bit PRNG backed by a global seed.  Not thread-safe.

#### `void set_kseed64(uint64_t seed)`
Sets the PRNG seed.  Call once at startup before using `krand64`.

#### `uint64_t krand64(uint64_t max)`
Returns the next pseudo-random 64-bit value.

- If `max > 0`, returns a value in `[0, max)`.
- If `max == 0`, returns the raw 64-bit output with no modulo applied.

---

## 7. Variable Types — `var.h`

A tagged union (`var_t`) supporting seven types.  All constructors allocate
from a `ta_list_t` and return a TA-tracked pointer.

```c
typedef enum {
    VAR_NULL, VAR_INT, VAR_FLOAT, VAR_BOOL,
    VAR_STR, VAR_ARRAY, VAR_DICT
} var_type_t;
```

---

### Constructors

#### `var_t *var_null(ta_list_t *gc)`
Creates a `VAR_NULL` variable.

#### `var_t *var_int(ta_list_t *gc, int64_t v)`
Creates a `VAR_INT` variable with value `v`.

#### `var_t *var_float(ta_list_t *gc, double v)`
Creates a `VAR_FLOAT` variable with value `v`.

#### `var_t *var_bool(ta_list_t *gc, int v)`
Creates a `VAR_BOOL` variable.  Any non-zero `v` is stored as `1`.

#### `var_t *var_str(ta_list_t *gc, const char *s)`
Creates a `VAR_STR` variable.  The string is duplicated via `ta_strdup`.
`s` may be `NULL`; the stored pointer will be `NULL` (treated as empty on
coercion).

#### `var_t *var_array(ta_list_t *gc)`
Creates an empty `VAR_ARRAY` variable.  Initial capacity is 8 elements;
the backing array doubles on overflow.

---

### Array operations

#### `int var_push(ta_list_t *gc, var_t *arr, var_t *item)`
Appends `item` to `arr`.  Grows the backing array if needed.  `item` may be
`NULL` (stored as a `NULL` slot).

- **Returns** `0` on success, `-1` if `arr` is `NULL` or not `VAR_ARRAY`, or
  on allocation failure.

#### `var_t *var_get(var_t *arr, size_t idx)`
Returns the element at index `idx`, or `NULL` if out of bounds or `arr` is not
`VAR_ARRAY`.

#### `size_t var_len(var_t *arr)`
Returns the number of elements in `arr`, or `0` if `arr` is `NULL` or not
`VAR_ARRAY`.

---

### Type check

#### `int var_is_null(var_t *v)`
Returns non-zero if `v` is `NULL` or its type is `VAR_NULL`.

---

### Coercing accessors

All accessors return a default value (`0`, `0.0`, `"null"`) for types that
cannot be meaningfully converted.

#### `int64_t var_to_int(var_t *v)`
Coerces `v` to `int64_t`.  Strings are parsed with `strtoll`; floats are
truncated; booleans become 0 or 1.

#### `double var_to_float(var_t *v)`
Coerces `v` to `double`.  Strings are parsed with `strtod`.

#### `int var_to_bool(var_t *v)`
Coerces `v` to a boolean.  Zero integers, `0.0`, empty strings, and
`VAR_NULL` are falsy; everything else is truthy.

#### `char *var_to_str(ta_list_t *gc, var_t *v)`
Coerces `v` to a TA-tracked string representation.  Arrays and dicts return
`"[array]"` and `"{dict}"` respectively.

---

### Display

#### `void var_print(var_t *v)`
Prints `v` to **stdout** in a human-readable form.  Strings are quoted.
Arrays and dicts are printed recursively up to a depth of 32 to prevent
infinite loops on circular references.

---

## 8. Dictionary — `dict.h`

A hash map with string keys and `var_t *` values.  Implemented as a 64-bucket
chained hash table using `xxh32` for bucket selection.  All memory is tracked
by the `ta_list_t` supplied at creation time.

---

### Lifecycle

#### `dict_t *dict_new(ta_list_t *gc)`
Creates a new, empty dictionary with the default 64-bucket table.
Equivalent to `dict_new_sized(gc, 64)`.

- **Returns** the new `dict_t`, or `NULL` on allocation failure.

#### `dict_t *dict_new_sized(ta_list_t *gc, uint32_t bucket_count)`
Creates a new, empty dictionary with `bucket_count` buckets.
`bucket_count` must be a power of 2 and >= 4.  Use `dict_new()` for
the default 64-bucket table.  For known entry counts, choose a
`bucket_count >= ceil(expected_entries / 0.7)` to keep load factor
below 0.7 for good performance.

- **Returns** the new `dict_t`, or `NULL` on allocation failure or if
  `bucket_count` is not a power of 2, or if `bucket_count < 4`.

#### `var_t *var_dict_new(ta_list_t *gc)`
Creates a `VAR_DICT` variable wrapping a new `dict_t`.  Convenient when a
dictionary needs to be stored as a `var_t` value (e.g. inside another dict
or array).

- **Returns** the `VAR_DICT` var, or `NULL` on failure.

---

### CRUD

#### `int dict_set(dict_t *d, const char *key, var_t *val)`
Inserts or updates the entry for `key`.  If `key` already exists the value
is updated in place without changing the entry count.  The key string is
duplicated internally.

- **Returns** `0` on success, `-1` if `d` or `key` is `NULL` or on
  allocation failure.

#### `var_t *dict_get(dict_t *d, const char *key)`
Returns the value stored for `key`, or `NULL` if the key does not exist.
Note: a key can legitimately map to a `NULL` value; use `dict_has` to
distinguish "not found" from "found with NULL value".

#### `int dict_del(dict_t *d, const char *key)`
Removes the entry for `key` and frees its key string.  The associated
`var_t` is not freed here (it remains tracked by the tracked allocator list).

- **Returns** `0` on success, `-1` if the key was not found.

#### `int dict_has(dict_t *d, const char *key)`
Returns `1` if `key` exists in the dictionary, `0` otherwise.  Correctly
handles keys that map to `NULL` values (unlike testing `dict_get != NULL`).

#### `size_t dict_count(dict_t *d)`
Returns the number of entries in `d`, or `0` if `d` is `NULL`.

---

### Iteration

#### `typedef void (*dict_iter_fn)(const char *key, var_t *val, void *userdata)`
Callback type used by `dict_each`.

#### `void dict_each(dict_t *d, dict_iter_fn fn, void *userdata)`
Calls `fn(key, val, userdata)` for every entry in `d`.  Iteration order is
unspecified (depends on hash bucket layout).  The callback may delete the
current entry; the iteration uses `list_for_each_safe` internally.

---

### Display

#### `void dict_print(dict_t *d)`
Prints the dictionary to **stdout** in `{"key": value, ...}` form.  Nested
structures are printed recursively (depth-limited to 32 via `var_print`).
Prints `"null"` if `d` is `NULL`.

---

## 9. Panic and Stack Dump — `panic.h`

User-space only for backtrace.  In kernel-space builds the backtrace is
replaced with a note.  Requires `-rdynamic` at link time for symbol name
resolution.

#### `void stackdump(FILE *f)`
Prints a backtrace of up to `PANIC_BT_DEPTH` (64) frames to `f`.  Falls back
to `stderr` if `f` is `NULL`.  **Does not exit.**

#### `void panic(int rc, const char *msg)`
Prints `msg` and a full backtrace to **stderr**, then calls `exit(rc)`.  Use
for unrecoverable errors when no source location is needed.

#### `PANIC(rc, msg)`
Macro variant of `panic`.  Prepends `file:function:line` to the output,
matching the style of `TRACE_ERR`.  Expands to a call to `panic_at`.

#### `void panic_at(const char *file, const char *func, int line, int rc, const char *msg)`
Internal function called by the `PANIC` macro.  May also be called directly
when the source location is known dynamically.

---

## 10. Config Parser — `cfg_parser.h`

Parses configuration files into a `dict_t` of `var_t` values, backed by a
caller-supplied `ta_list_t`.

**Supported syntax:**

| Construct | Example |
|-----------|---------|
| Bash comment | `# comment` or trailing `# comment` |
| Integer | `COUNT=10` |
| Float | `PI=3.14` |
| Boolean | `FLAG=true` / `FLAG=false` |
| Quoted string | `NAME="hello world"` |
| Array literal | `ARR=[ 0, 1, "two" ]` |
| Variable reference | `COPY = ORIGINAL` |
| Array index | `V = ARR[2]` |
| Concatenation | `S = "hello" + NAME + "!"` |
| Append | `S += " more"` |
| Include | `include other.cfg` |
| Display | `display "value=" + VAR` |

String escape sequences `\n`, `\t`, `\\`, `\"` are supported inside quoted
strings.  `include` paths are resolved relative to the current file's
directory.  Nesting is limited to 16 levels.  `display` prints to **stdout**
during loading.

---

#### `kfl_cfg_t *kfl_cfg_new(ta_list_t *gc)`
Creates a new, empty config context backed by `gc`.

- **Returns** the new context, or `NULL` on failure.

#### `int kfl_cfg_load(kfl_cfg_t *cfg, const char *filename)`
Parses `filename` and merges all variables into `cfg`.  Can be called
multiple times to load several files into the same context.  `include`
directives are processed recursively.

- **Returns** `0` on success, `-1` if the file cannot be opened or contains
  parse errors.  On error, parsing continues to the end of the file so all
  errors are reported.

#### `var_t *kfl_cfg_get(kfl_cfg_t *cfg, const char *key)`
Looks up `key` in the config.

- **Returns** the associated `var_t *`, or `NULL` if the key was not set.

#### `void kfl_cfg_print(kfl_cfg_t *cfg)`
Prints all variables to **stdout** in `KEY = value` form with a header line
showing the total count.

---

## 11. Utilities — `utils.h`

#### `char *trim(char *s)`
Removes leading and trailing whitespace from `s` **in place**.  Modifies the
string directly; does not allocate.

- **Returns** `s`.

---

#### `int str_starts_with(const char *s, const char *prefix)`
Tests whether string `s` begins with `prefix`.  No dynamic allocation.

- **Returns** `1` if `s` starts with `prefix`, `0` otherwise.
- **Returns** `0` if either argument is `NULL`.
- **Returns** `0` if `prefix` is longer than `s`.

---

#### `int str_ends_with(const char *s, const char *suffix)`
Tests whether string `s` ends with `suffix`.  No dynamic allocation.

- **Returns** `1` if `s` ends with `suffix`, `0` otherwise.
- **Returns** `0` if either argument is `NULL`.
- **Returns** `0` if `suffix` is longer than `s`.

---

## 12. Hex Dump — `dumphex.h`

Debug utilities for printing memory contents.

#### `int dumphex(void *ptr, size_t size)`
Prints `size` bytes starting at `ptr` as an annotated hex + ASCII grid, 16
bytes per row.  Each row shows the byte offset, hex values in two groups of 8,
and the printable ASCII representation.

- **Returns** `0`.

#### `void dump_uint32(void *ptr, size_t size)`
Prints `size / 4` 32-bit words from `ptr` as a table with index, hex value,
and 4-character ASCII (printed in memory order).

#### `void dump_uint64(void *ptr, size_t size)`
Prints `size / 8` 64-bit words from `ptr` as a table with index, hex value,
and 8-character ASCII (printed in memory order).

---

## 13. CRC-32C — `crc32c.h`

CRC-32C (Castagnoli) checksum with hardware acceleration where available.
Used to protect all on-disk structs in the KANEK stack.

**Hardware acceleration:**
- x86/x86-64 with SSE4.2: `_mm_crc32_u*` intrinsics
- ARM/AArch64 with CRC extension: `__crc32c*` intrinsics
- Falls back to a software lookup table when no hardware support is present.

---

#### `uint32_t kfl_crc32c(uint32_t crc, const void *buf, size_t len)`

Computes the CRC-32C of `len` bytes starting at `buf`.

Uses the standard CRC-32C (iSCSI) convention: the running CRC is XOR'd
with `0xFFFFFFFF` on entry and on exit, making the function fully composable.

- `crc`: initial value.  Pass `0` for a fresh computation.  Pass the result
  of a prior call to chain buffers:
  ```c
  uint32_t c = kfl_crc32c(0,   buf1, len1);
  c           = kfl_crc32c(c,   buf2, len2);
  ```
  The chained result equals `kfl_crc32c(0, combined_buf, len1 + len2)`.

- **Returns** the CRC-32C of the input.

---

#### `int kfl_crc32c_verify(const void *buf, size_t len)`

Verifies a struct protected by CRC-32C.  The KANEK on-disk convention stores
the CRC-32C of the first `len - 4` bytes as a little-endian `uint32_t` in the
last 4 bytes of the struct.

- `buf`: pointer to the struct.
- `len`: total size of the struct, **including** the 4-byte CRC field.

The stored CRC is read byte-by-byte (safe on unaligned buffers).

- **Returns** `1` if the computed CRC matches the stored value.
- **Returns** `0` if the data is corrupted, or if `buf` is `NULL` or `len < 4`.
