# Fix plan: dict.c / var.c review findings

## Who this is for

This plan is written to be executed by an AI coding session with **no
memory of the review conversation that produced it**. Every task is
self-contained: exact file, exact current code, exact replacement code,
exact verification command. Do not improvise beyond what's written. If
a step tells you to STOP AND ASK, do that instead of guessing — a wrong
guess here can turn a documented limitation into a live memory-safety
bug.

Read `CODING_STYLE.md` before editing any `.c`/`.h` file — it is
binding (brace placement, `return( x);`, explicit `NULL` comparisons,
4-space indent, 78-col limit, etc.) and everything below must conform
to it.

## Background

This plan fixes issues found in a manual review of `src/dict.c` /
`src/dict.h` (a chained hash table keyed by string, storing `var_t *`
values) and their interaction with `src/var.c` / `src/ta.c`. The full
original review is not reproduced here — only the decisions and exact
fixes needed are. Where a decision was genuinely a tradeoff (not a
clear-cut bug), the choice already made by the project owner is stated
in each task, along with the reasoning, so you don't need to
re-litigate it.

**Priority for this whole plan, stated explicitly because it drove
several of the decisions below: avoid introducing crashes or
memory-safety issues, even at the cost of leaving a documented,
bounded inefficiency in place.** Where a fix could plug an
inefficiency but the only way to do so is by trusting some invariant
that can't be verified across the whole codebase, this plan chooses
the safe-by-construction option, and offers the more aggressive option
only as something a caller opts into explicitly, one call site at a
time.

## Ground rules

1. **Do the tasks in the order listed.** The order is not arbitrary —
   later tasks depend on earlier ones (dependencies are called out
   explicitly in each task).
2. **After every single task**, run both:
   ```
   cd src && make clean && make runtests
   cd src && make asan-tests
   ```
   (Task 1 below is what creates the `asan-tests` target — for Task 1
   itself, just verify the target works as described in that task,
   there's no earlier task to run it against.) All tests must pass
   under both commands, with no new compiler warnings and no
   AddressSanitizer reports, before you move on to the next task. Do
   not batch multiple tasks together and test once at the end — if
   something breaks, you need to know which task broke it.
3. **Do not touch anything not named in a task.** If you notice other
   issues while working, do not fix them — write them down in a final
   summary instead. Scope creep here is a liability, not a bonus.
4. **Do not create git commits unless the user explicitly asks for
   them in this session.**
5. If a task says **STOP AND ASK**, stop and ask the user before
   writing any code for that task. Do not pick an answer yourself.

## Explicitly out of scope (do not do these)

- **Hash-seed randomization.** `bucket_for_n()` in `dict.c` always
  hashes with seed `0`, which in theory allows hash-flooding if a dict
  ever holds attacker-controlled keys. Decision: **skip**, because (a)
  dict keys in this codebase currently come from internal/config
  sources, not untrusted external input, and (b) the only available
  fix (seeding from `krand64()`, see `krand64.h`) would add a
  cross-module dependency on the caller having already called
  `set_kseed64()` at startup — an implicit ordering requirement that's
  easy to get wrong, and `krand64()`'s global seed is documented as
  unsynchronized in kernel-space builds. Task 4 below adds a one-line
  comment noting this limitation; that comment is the entire scope of
  this item. Do not add any seeding logic.
- **Open-addressing / full hash-table redesign.** Chained buckets with
  a `list_t` per entry work; a more compact layout is a bigger future
  project, not part of this plan.
- **`ta.h`'s `MAX_DBG_STR_LEN` / `ta_node_t` header layout.** Any
  change there affects every module in the library. Not touched here,
  except for the one, minimal, additive fix in Task 3.
- **Thread-safety / locking.** Not addressed by this plan.
- **Migrating any existing call site to `dict_set_owned()`.** This
  plan (Task 6) only adds the function. Deciding whether any specific
  call site is safe to switch to it is separate future work, and is
  not started here.
- **`cfg_parser.c`, `var.c`'s array logic, or any file not explicitly
  named in a task below.**

---

## Task 1 — Add a permanent `make asan-tests` target

**File:** `src/Makefile`

**Why first:** every later task's verification step (per Ground Rule
2) runs `make asan-tests`. Add it before you need it.

**Step 1a — top-of-file comment.** Current (lines 1-3):
```makefile
# To enable AddressSanitizer, add to both CFLAGS and LDFLAGS:
#   CFLAGS  += -fsanitize=address
#   LDFLAGS += -fsanitize=address -static-libasan
```
Replace with:
```makefile
# To enable AddressSanitizer manually, add to both CFLAGS and LDFLAGS:
#   CFLAGS  += -fsanitize=address
#   LDFLAGS += -fsanitize=address -static-libasan
# Or just run `make asan-tests`, which does this for you, runs the
# full test suite under it, then restores a normal (non-sanitized)
# build.
```

**Step 1b — new target.** Find the `runtests:` recipe block:
```makefile
runtests: tests
	@echo "--- testrand ---";   ./testrand
	@echo "--- testhash ---";   echo "hello\nworld\nfoo" | ./testhash
	@echo "--- testdh ---";     ./testdh
	@echo "--- testgc ---";     ./testgc
	@echo "--- testta ---";     ./testta
	@echo "--- testmap ---";    ./testmap
	@echo "--- testutils ---";  ./testutils
	@echo "--- testcrc32c ---"; ./testcrc32c
	@echo "--- testdict ---";   ./testdict
```
Leave it completely unchanged. Immediately after it (and before the
`# test binaries` comment that follows), insert this new target:
```makefile
# Build and run the full test suite under AddressSanitizer, then
# restore a normal (non-sanitized) build. Run this as part of
# verifying any change to ta.c, dict.c, or var.c — a plain build will
# not reliably catch use-after-free / double-free bugs.
asan-tests:
	$(MAKE) clean
	$(MAKE) CFLAGS="-Wall -DUSER_SPACE -g -O0 -fsanitize=address" \
	        LDFLAGS="-L. -lkfl -rdynamic -lpthread -fsanitize=address -static-libasan" \
	        runtests
	$(MAKE) clean
	$(MAKE) tests
```
The trailing `$(MAKE) clean` + `$(MAKE) tests` rebuilds a normal
(non-ASan) `libkfl.a` and all test binaries, so the tree is left in
its usual state afterward — no separate manual rebuild step needed.

**Verify:** run `make asan-tests` directly (it's not part of `all` or
`runtests`, so it won't run on its own). Confirm it prints the same
`--- testX ---` / `PASS` output as a normal `make runtests` run, with
no AddressSanitizer error reports anywhere in the output, and that it
finishes without errors. Then run `make runtests` immediately after —
it should rebuild little or nothing (confirming the target's own
trailing rebuild already restored a normal build).

---

## Task 2 — Establish baseline `dict.c` test coverage

**File:** `src/testdict.c`

**Why this comes before any behavior changes:** the current test file
only exercises `dict_new_sized()` and a bulk `dict_set()`/`dict_get()`
load of 200 brand-new keys. `dict_del()`, `dict_has()`, `dict_each()`,
`dict_print()`, `dict_new()`, and `var_dict_new()` have **zero**
coverage, and even the tested functions are missing edge cases:
overwriting an existing key, storing an explicit `NULL` value, `NULL`
arguments, an empty dict, and deletion from a multi-entry bucket chain
(head/middle/tail). Tasks 3-7 all touch `dict_del()`, the
update-in-place branch of `dict_set()`, or bucket-chain traversal —
without this baseline in place first, there is no regression net for
exactly the code those tasks change.

None of the tests added here depend on anything from Tasks 3-7 — they
test the dict.c/dict.h behavior as it exists right now, and that
behavior does not change for any of these functions across the rest of
this plan (see each later task's description). They should keep
passing, unmodified, all the way through Task 7.

**One forward-compatibility note, so it doesn't confuse you later:**
the "chain (bucket collision) correctness" block below (`dchain`)
deliberately inserts 20 keys into a 4-bucket table and relies on the
pigeonhole principle to guarantee some real multi-entry chains exist,
to test deletion from them. Once Task 7 (automatic rehashing) is
implemented, that same test will trigger auto-growth partway through
the 20 inserts, so by the end the table will have more than 4 buckets
and shorter chains than when this test was written. **This is fine —
do not "fix" it.** The test's assertions (every remaining key stays
correctly retrievable after each delete) remain fully valid regardless
of table size; only the comment's "forces long chains" framing becomes
less strictly true once Task 7 lands. Task 7 has its own dedicated
rehashing test for that behavior specifically.

**Step 2a — add file-scope helpers.** In `src/testdict.c`, right after
the existing `CHECK()` macro definition and before `int main( void){`,
add:
```c
static int     each_visit_count = 0;
static int64_t each_sum         = 0;

static void count_and_sum_cb( const char *key, var_t *val, void *userdata){
    (void)key;
    (void)userdata;
    each_visit_count++;
    each_sum += var_to_int( val);
}

static void delete_current_cb( const char *key, var_t *val, void *userdata){
    dict_t *d = (dict_t *)userdata;

    (void)val;
    dict_del( d, key);
}
```

**Step 2b — add the test blocks.** In `main()`, find the existing
block:
```c
    ta_list_destroy( &ta);
```
Insert everything below directly *before* that line (i.e. after the
existing "200 entries in 256-bucket table" block, which stays
unchanged). Insert it as one contiguous chunk, in this exact order:

```c
    printf( "\n=== dict_new_sized: bucket_count below minimum ===\n");

    dict_t *d2 = dict_new_sized( &ta, 2);
    CHECK( d2 == NULL,
          "dict_new_sized(2) returns NULL (power of 2, below minimum 4)");

    printf( "\n=== dict_new: default bucket count ===\n");

    dict_t *ddef = dict_new( &ta);
    CHECK( ddef != NULL, "dict_new() returns non-NULL");
    if ( ddef != NULL) {
        CHECK( ddef->nbuckets == DICT_NBUCKETS,
              "dict_new() uses DICT_NBUCKETS (64) buckets");
        CHECK( dict_count( ddef) == 0, "freshly created dict has count == 0");
    }

    printf( "\n=== NULL-argument safety ===\n");

    CHECK( dict_set( NULL, "k", NULL) == -1, "dict_set(NULL, k, v) == -1");
    CHECK( dict_get( NULL, "k") == NULL, "dict_get(NULL, k) == NULL");
    CHECK( dict_del( NULL, "k") == -1, "dict_del(NULL, k) == -1");
    CHECK( dict_has( NULL, "k") == 0, "dict_has(NULL, k) == 0");
    CHECK( dict_count( NULL) == 0, "dict_count(NULL) == 0");
    dict_each( NULL, count_and_sum_cb, NULL);
    CHECK( 1, "dict_each(NULL, ...) does not crash");
    dict_print( NULL);
    CHECK( 1, "dict_print(NULL) does not crash");

    if ( ddef != NULL) {
        CHECK( dict_set( ddef, NULL, NULL) == -1,
              "dict_set(d, NULL, v) == -1");
        CHECK( dict_get( ddef, NULL) == NULL, "dict_get(d, NULL) == NULL");
        CHECK( dict_del( ddef, NULL) == -1, "dict_del(d, NULL) == -1");
        CHECK( dict_has( ddef, NULL) == 0, "dict_has(d, NULL) == 0");
    }

    printf( "\n=== empty dict behavior ===\n");

    dict_t *dempty = dict_new_sized( &ta, 4);
    CHECK( dempty != NULL, "dict_new_sized(4) for empty-dict test != NULL");
    if ( dempty != NULL) {
        CHECK( dict_count( dempty) == 0, "empty dict: count == 0");
        CHECK( dict_get( dempty, "missing") == NULL,
              "empty dict: dict_get(missing) == NULL");
        CHECK( dict_has( dempty, "missing") == 0,
              "empty dict: dict_has(missing) == 0");
        CHECK( dict_del( dempty, "missing") == -1,
              "empty dict: dict_del(missing) == -1");
    }

    printf( "\n=== basic set/get/has/del roundtrip ===\n");

    dict_t *dbasic = dict_new_sized( &ta, 4);
    CHECK( dbasic != NULL, "dict_new_sized(4) for basic test != NULL");
    if ( dbasic != NULL) {
        var_t *v1 = var_int( &ta, 42);
        var_t *v2 = var_int( &ta, 99);
        var_t *got;

        CHECK( dict_set( dbasic, "a", v1) == 0, "dict_set(a, 42) == 0");
        CHECK( dict_count( dbasic) == 1, "count == 1 after first insert");
        CHECK( dict_has( dbasic, "a") == 1, "dict_has(a) == 1");
        CHECK( dict_get( dbasic, "a") == v1,
              "dict_get(a) returns the exact stored pointer");

        /* overwrite: count must not increase, old value must survive
         * (dict_set is documented as non-owning) */
        CHECK( dict_set( dbasic, "a", v2) == 0,
              "dict_set(a, 99) (overwrite) == 0");
        CHECK( dict_count( dbasic) == 1,
              "count still 1 after overwrite (not incremented)");
        CHECK( dict_get( dbasic, "a") == v2,
              "dict_get(a) returns the new pointer after overwrite");
        CHECK( var_to_int( v1) == 42,
              "old value (v1) still valid/unchanged after being "
              "overwritten in the dict");

        /* explicit NULL value: a legitimate, distinct case from
         * key-not-found (see dict_has's purpose) */
        CHECK( dict_set( dbasic, "b", NULL) == 0,
              "dict_set(b, NULL) == 0");
        CHECK( dict_has( dbasic, "b") == 1,
              "dict_has(b) == 1 (key exists, value is NULL)");
        got = dict_get( dbasic, "b");
        CHECK( got == NULL, "dict_get(b) == NULL (the stored value)");
        CHECK( dict_has( dbasic, "nonexistent") == 0,
              "dict_has(nonexistent) == 0 (distinct from stored-NULL "
              "case above)");

        /* delete */
        CHECK( dict_del( dbasic, "a") == 0, "dict_del(a) == 0");
        CHECK( dict_count( dbasic) == 1,
              "count == 1 after deleting one of two entries");
        CHECK( dict_has( dbasic, "a") == 0, "dict_has(a) == 0 after delete");
        CHECK( dict_get( dbasic, "a") == NULL,
              "dict_get(a) == NULL after delete");
        CHECK( dict_del( dbasic, "a") == -1,
              "deleting an already-deleted key returns -1");
    }

    printf( "\n=== chain (bucket collision) correctness ===\n");

    dict_t *dchain = dict_new_sized( &ta, 4);
    CHECK( dchain != NULL, "dict_new_sized(4) for chain test != NULL");
    if ( dchain != NULL) {
        char key[32];
        int  i, n = 20;
        int  all_ok = 1;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "chain_%d", i);
            dict_set( dchain, key, var_int( &ta, (int64_t)i));
        }
        CHECK( dict_count( dchain) == (size_t)n,
              "count == 20 after inserting 20 keys into a 4-bucket table");

        /* Delete every key one at a time, checking after each delete
         * that the deleted key is gone and every *remaining* key is
         * still correctly retrievable. See the forward-compatibility
         * note above this task's steps regarding Task 7. */
        for ( i = 0; i < n && all_ok; i++) {
            int j;

            snprintf( key, sizeof( key), "chain_%d", i);
            if ( dict_del( dchain, key) != 0 || dict_has( dchain, key)) {
                all_ok = 0;
                break;
            }
            for ( j = i + 1; j < n; j++) {
                char   other[32];
                var_t *v;

                snprintf( other, sizeof( other), "chain_%d", j);
                v = dict_get( dchain, other);
                if ( v == NULL || var_to_int( v) != (int64_t)j) {
                    all_ok = 0;
                    break;
                }
            }
        }
        CHECK( all_ok, "deleting all 20 chained keys one at a time "
                       "leaves every remaining key intact");
        CHECK( dict_count( dchain) == 0,
              "count == 0 after deleting all keys");
    }

    printf( "\n=== dict_each: iteration ===\n");

    dict_t *deach = dict_new_sized( &ta, 4);
    CHECK( deach != NULL, "dict_new_sized(4) for dict_each test != NULL");
    if ( deach != NULL) {
        char    key[32];
        int     i, n = 10;
        int64_t expected_sum = 0;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "e_%d", i);
            dict_set( deach, key, var_int( &ta, (int64_t)i));
            expected_sum += i;
        }

        each_visit_count = 0;
        each_sum = 0;
        dict_each( deach, count_and_sum_cb, NULL);
        CHECK( each_visit_count == n, "dict_each visits exactly n entries");
        CHECK( each_sum == expected_sum,
              "dict_each callback sees the correct key/val pairs "
              "(sum check)");
    }

    printf( "\n=== dict_each: callback may dict_del the current key ===\n");

    dict_t *deachdel = dict_new_sized( &ta, 4);
    CHECK( deachdel != NULL,
          "dict_new_sized(4) for dict_each-delete test != NULL");
    if ( deachdel != NULL) {
        char key[32];
        int  i, n = 10;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "d_%d", i);
            dict_set( deachdel, key, var_int( &ta, (int64_t)i));
        }

        dict_each( deachdel, delete_current_cb, deachdel);
        CHECK( dict_count( deachdel) == 0,
              "dict_each callback deleting every current key empties "
              "the dict without crashing or skipping entries");
    }

    printf( "\n=== dict_print: smoke test ===\n");

    dict_t *dprint = dict_new_sized( &ta, 4);
    if ( dprint != NULL) {
        dict_set( dprint, "x", var_int( &ta, 1));
        dict_set( dprint, "y", var_str( &ta, "hello"));
        printf( "  ");
        dict_print( dprint);
        printf( "\n");
    }
    CHECK( 1, "dict_print did not crash on a populated dict");

    printf( "\n=== var_dict_new: nested dict ===\n");

    var_t *vouter = var_dict_new( &ta);
    CHECK( vouter != NULL, "var_dict_new() returns non-NULL");
    if ( vouter != NULL) {
        CHECK( vouter->type == VAR_DICT,
              "var_dict_new() wrapper has type VAR_DICT");

        dict_t *inner = (dict_t *)vouter->dict;
        CHECK( inner != NULL, "wrapped dict_t is non-NULL");
        if ( inner != NULL) {
            var_t *got;

            dict_set( inner, "nested_key", var_int( &ta, 7));
            got = dict_get( inner, "nested_key");
            CHECK( got != NULL && var_to_int( got) == 7,
                  "set/get through a var_t-wrapped dict works");
        }
    }
```

**Verify:** `make clean && make runtests` and `make asan-tests` — all
new checks print `PASS`, no new compiler warnings, no ASan reports.

---

## Task 3 — `ta_free()` must tolerate `NULL`

**File:** `src/ta.c`

**Why this matters for later tasks:** Task 6 below adds
`dict_set_owned()`, which calls `ta_free()` on a value that can
legitimately be `NULL` (dict entries are allowed to store a `NULL`
value on purpose — see `dict_has()`'s doc comment in `dict.h`).
`ta_free()` currently has no `NULL` check, so this fix must land
before Task 6.

**Current code:**
```c
void ta_free( void *p){
    ta_node_t *node = container_of( p, ta_node_t, data);
    list_del( LIST( node));
    free( node);
}
```

**Replace with:**
```c
void ta_free( void *p){
    ta_node_t *node;

    if ( p == NULL) {
        return;
    }
    node = container_of( p, ta_node_t, data);
    list_del( LIST( node));
    free( node);
}
```

**Test to add:** in `src/testta.c`, in `main()`, add (following the
existing `CHECK()` macro pattern already used in that file, anywhere
after `ta_list_init( &ta);`):
```c
printf( "=== ta_free(NULL) safety ===\n");
ta_free( NULL);
CHECK( 1, "ta_free(NULL) does not crash");
```
(The `CHECK(1, ...)` just records a pass if execution reaches that
line — if `ta_free(NULL)` crashes, the test binary itself will not
finish running, which is the actual signal to watch for.)

**Verify:** `make clean && make runtests` and `make asan-tests` (see
Ground Rule 2) — all green, no new warnings, no ASan reports.

---

## Task 4 — De-duplicate the dict-printing recursion

**Files:** `src/dict.c`, `src/dict.h`, `src/var.c`

**Why:** `dict.c` has its own `dict_print()` that walks all buckets
and calls `var_print()` per value. `var.c` separately has a *static*
`dict_print_depth()` / `var_print_depth()` pair that does the same
walk but with depth-limiting (to bound recursion on self-referential
structures — e.g. a dict that (directly or indirectly) contains
itself). These are two independent implementations of the same logic
that can silently drift apart. This task merges them into one.

(Note: tracing it through, `dict.c`'s current standalone loop is not
actually an infinite-recursion bug today — once it calls into
`var_print()`, depth tracking resumes correctly. But maintaining two
copies of the same traversal is a real maintainability risk, so we
still fix it.)

**Step 4a — `src/dict.h`:** add this declaration (in the "display"
section, near the existing `dict_print` declaration):
```c
/* internal: shared with var.c's var_print_depth() recursion so
 * dict-printing has exactly one implementation. Not part of the
 * stable per-entry API — use dict_print() or var_print() instead. */
void dict_print_depth( dict_t *d, int depth);
```

**Step 4b — `src/var.c`:** find the existing forward declaration:
```c
static void dict_print_depth( dict_t *d, int depth);
```
Delete that line entirely (the declaration now comes from `dict.h`,
which `var.c` already includes).

Then find the existing definition:
```c
static void dict_print_depth( dict_t *d, int depth){
```
Remove the `static` keyword, so it reads:
```c
void dict_print_depth( dict_t *d, int depth){
```
Do not change anything else about this function's body.

**Step 4c — `src/dict.c`:** replace the entire current `dict_print()`
function body (including its comment block) —
```c
void dict_print( dict_t *d){
    /* depth-limited printing is handled via var_print → dict_print_depth
     * in var.c; this entry point just delegates to the depth-0 path by
     * printing directly (single-level display, no recursion guard needed
     * since var_print already guards sub-values). */
    int i;
    list_t *pos;
    int first = 1;

    if ( d == NULL) {
        printf( "null");
        return;
    }

    printf( "{");
    for ( i = 0; i < (int)d->nbuckets; i++) {
        list_for_each( pos, &d->buckets[i]) {
            dict_entry_t *e = container_of( pos, dict_entry_t, node);
            if ( !first) {
                printf( ", ");
            }
            printf( "\"%s\": ", e->key);
            var_print( e->val);
            first = 0;
        }
    }
    printf( "}");
}
```
with:
```c
void dict_print( dict_t *d){
    dict_print_depth( d, 0);
}
```

**Note on the hash-seed limitation (see "Explicitly out of scope"
above):** while you're in `dict.c`, add this one comment directly
above the `bucket_for_n()` function and do nothing else there:
```c
/* NOTE: seed is fixed at 0. If this dict is ever used with
 * attacker-influenced keys, this is vulnerable to hash-flooding
 * (crafted keys colliding into one bucket). Not fixed here — see
 * plan.md's "Explicitly out of scope" section for why. */
```

**Verify:** `make clean && make runtests` and `make asan-tests` — all
green, no new warnings, no ASan reports. Also build and run
`testvar`/`testdict` individually (`make testvar && ./testvar`, `make
testdict && ./testdict`) and spot-check by eye that dict/var printed
output is unchanged from before this task (output format did not
change, only where the code lives).

---

## Task 5 — Fold the key into `dict_entry_t` (one allocation instead of two)

**Files:** `src/dict.h`, `src/dict.c`

**Why:** Every new entry currently costs two separate `ta_malloc`
allocations (the `dict_entry_t` struct, and a separate `ta_strdup`'d
key string), each carrying a fixed ~72-byte tracked-allocator header.
Storing the key inline as a flexible array member cuts this to one
allocation per new entry.

**Step 5a — `src/dict.h`:** change:
```c
typedef struct {
    list_t  node;
    char   *key;
    var_t  *val;
} dict_entry_t;
```
to:
```c
typedef struct {
    list_t  node;
    var_t  *val;
    char    key[];   /* flexible array member — MUST stay last field */
} dict_entry_t;
```
(`val` and `key` swapped order deliberately — a flexible array member
must be the last field in the struct, that's a C language rule, not a
style choice.)

**Step 5b — `src/dict.c`, in `dict_set()`:** find:
```c
    /* new entry */
    e = ta_malloc( d->gc, sizeof( dict_entry_t));
    if ( e == NULL) {
        TRACE_ERR( "ta_malloc failed for dict_entry_t");
        return( -1);
    }
    e->key = ta_strdup( d->gc, (char *)key);
    if ( e->key == NULL) {
        TRACE_ERR( "ta_strdup failed for key");
        ta_free( e);
        return( -1);
    }
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;
    return( 0);
```
Replace with:
```c
    /* new entry — key is stored inline (flexible array member) so
     * this is a single allocation instead of two. */
    keylen = strlen( key);
    e = ta_malloc( d->gc, sizeof( dict_entry_t) + keylen + 1);
    if ( e == NULL) {
        TRACE_ERR( "ta_malloc failed for dict_entry_t");
        return( -1);
    }
    memcpy( e->key, key, keylen + 1);
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;
    return( 0);
```
Also add `keylen` to `dict_set()`'s existing top-of-function
declarations block, so it reads:
```c
int dict_set( dict_t *d, const char *key, var_t *val){
    size_t        b;
    list_t       *pos;
    dict_entry_t *e;
    size_t        keylen;
```
(`string.h` is already `#include`d at the top of `dict.c` — `strlen`
and `memcpy` need no new includes.)

**Step 5c — `src/dict.c`, in `dict_del()`:** find:
```c
        if ( strcmp( e->key, key) == 0) {
            list_del( pos);
            ta_free( e->key);
            ta_free( e);
            d->count--;
            return( 0);
        }
```
Replace with:
```c
        if ( strcmp( e->key, key) == 0) {
            list_del( pos);
            ta_free( e);
            d->count--;
            return( 0);
        }
```
(Deleting the `ta_free( e->key);` line is required, not optional —
`key` is no longer a separate allocation, it's inline inside `e`. If
you leave that line in, it will not compile the same way it did before
— `e->key` is now an array, and even if it did compile, calling
`ta_free()` on an address that isn't the start of its own tracked
allocation node is memory corruption.)

**Step 5d — grep before moving on:** run
```
grep -rn '\.key\b\|->key\b' src/*.c src/*.h
```
and check every hit outside `dict.c`/`dict.h`. All existing call sites
should only *read* `e->key` (e.g. `strcmp`, `printf( "%s", e->key)`)
— reading still works identically since an array name decays to a
pointer in those contexts. If you find any place that **assigns** to
`->key` (e.g. `e->key = something;`) or takes its address as if it
were a `char **` (`&e->key`), STOP AND ASK before proceeding — that
call site was relying on `key` being an independent pointer and this
change would break it silently.

**Verify:** `make clean && make runtests` and `make asan-tests` — all
green, no new warnings, no ASan reports.

---

## Task 6 — Add `dict_set_owned()`; `dict_set()` stays non-owning

**Files:** `src/dict.h`, `src/dict.c`

**Decision already made, and why:** `dict_set()` will **not** be
changed to free the value it replaces on overwrite (its current,
existing behavior — leave the old value tracked-but-orphaned until the
whole `ta_list_t` context is destroyed, or until the caller frees it
explicitly). That's deliberate: whether it's safe to free a replaced
value depends on a property that can't be verified once and locked in
— "is this `var_t *` ever reachable from anywhere else" — because
nothing stops a `var_t *` from being stored under two different dict
keys, pushed into an array *and* set into a dict, or read via
`dict_get()` and used again by a caller right before/after a
`dict_set()` on that same key. If any of that ever happens, freeing on
overwrite is a use-after-free, and the bug shows up later, wherever
the dangling pointer happens to get dereferenced next — not at the
`dict_set()` call site. That risk profile is not acceptable as a
library-wide default.

Instead, this task adds `dict_set_owned()`: a second, explicitly-named
entry point that *does* free the old value, for call sites where the
caller can verify locally that nothing else holds a reference to the
value being replaced (the overwhelmingly common case is: the value was
just constructed fresh, right before this call, solely to be stored
here). `dict_set()` itself does not change behavior, so **this task
cannot break anything that currently uses `dict_set()`** — it only
adds a new function nothing calls yet (per "Explicitly out of scope,"
migrating any call site to use it is not part of this plan).

**Step 6a — `src/dict.c`:** find the current `dict_set()` function
(the version that exists after Task 5's edits):
```c
int dict_set( dict_t *d, const char *key, var_t *val){
    size_t        b;
    list_t       *pos;
    dict_entry_t *e;
    size_t        keylen;

    if ( d == NULL || key == NULL) {
        TRACE_ERR( "d or key is NULL");
        return( -1);
    }
    b = bucket_for_n( key, d->nbuckets);

    /* update in place if key already exists */
    list_for_each( pos, &d->buckets[b]) {
        e = container_of( pos, dict_entry_t, node);
        if ( strcmp( e->key, key) == 0) {
            e->val = val;
            return( 0);
        }
    }

    /* new entry — key is stored inline (flexible array member) so
     * this is a single allocation instead of two. */
    keylen = strlen( key);
    e = ta_malloc( d->gc, sizeof( dict_entry_t) + keylen + 1);
    if ( e == NULL) {
        TRACE_ERR( "ta_malloc failed for dict_entry_t");
        return( -1);
    }
    memcpy( e->key, key, keylen + 1);
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;
    return( 0);
}
```
Replace the whole thing with:
```c
static int dict_set_impl( dict_t *d, const char *key, var_t *val,
                          int free_old){
    size_t        b;
    list_t       *pos;
    dict_entry_t *e;
    size_t        keylen;

    if ( d == NULL || key == NULL) {
        TRACE_ERR( "d or key is NULL");
        return( -1);
    }
    b = bucket_for_n( key, d->nbuckets);

    /* update in place if key already exists */
    list_for_each( pos, &d->buckets[b]) {
        e = container_of( pos, dict_entry_t, node);
        if ( strcmp( e->key, key) == 0) {
            if ( free_old && e->val != val) {
                ta_free( e->val);
            }
            e->val = val;
            return( 0);
        }
    }

    /* new entry — key is stored inline (flexible array member) so
     * this is a single allocation instead of two. */
    keylen = strlen( key);
    e = ta_malloc( d->gc, sizeof( dict_entry_t) + keylen + 1);
    if ( e == NULL) {
        TRACE_ERR( "ta_malloc failed for dict_entry_t");
        return( -1);
    }
    memcpy( e->key, key, keylen + 1);
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;
    return( 0);
}

int dict_set( dict_t *d, const char *key, var_t *val){
    return( dict_set_impl( d, key, val, 0));
}

int dict_set_owned( dict_t *d, const char *key, var_t *val){
    return( dict_set_impl( d, key, val, 1));
}
```
(The `e->val != val` guard in `dict_set_impl` exists so that calling
`dict_set_owned()` again with the exact same value pointer it already
holds — e.g. a caller "refreshing" a key with its current value —
doesn't free the value out from under itself.)

**Step 6b — `src/dict.h`:** find:
```c
/* CRUD */
int    dict_set  (dict_t *d, const char *key, var_t *val);
var_t *dict_get  (dict_t *d, const char *key);
int    dict_del  (dict_t *d, const char *key);
int    dict_has  (dict_t *d, const char *key);
size_t dict_count( dict_t *d);
```
Replace with:
```c
/* CRUD */

/*
 * dict_set() - set d[key] = val, without taking ownership of val.
 *
 * If key already exists, its value pointer is replaced with val — the
 * previous value is NOT freed; it stays alive (tracked by d->gc) until
 * the whole gc context is destroyed, or until you free it yourself.
 * This is the safe default: it never frees a pointer you might still
 * be holding a reference to elsewhere (another container, a local
 * variable, a dict_each() callback, etc).
 *
 * Returns 0 on success, -1 on error (d or key is NULL, or allocation
 * failure for a new entry).
 */
int    dict_set  (dict_t *d, const char *key, var_t *val);

/*
 * dict_set_owned() - like dict_set(), but if key already exists, its
 * previous value IS freed via ta_free() before being replaced.
 *
 * Only use this at a call site where you can be certain the value
 * being replaced is not referenced anywhere else (not stored under
 * another key, not pushed into an array, not held by a caller that
 * expects to keep using it). If that's not certain, use dict_set()
 * instead and let the old value live until the gc context is torn
 * down — a bounded, harmless cost — rather than risk a
 * use-after-free.
 *
 * Returns 0 on success, -1 on error (same conditions as dict_set()).
 */
int    dict_set_owned( dict_t *d, const char *key, var_t *val);

var_t *dict_get  (dict_t *d, const char *key);
int    dict_del  (dict_t *d, const char *key);
int    dict_has  (dict_t *d, const char *key);
size_t dict_count( dict_t *d);
```

**Tests to add** (in `src/testdict.c`, following the existing
`CHECK()` pattern):
1. **`dict_set()` stays non-owning:** create a dict, `dict_set()` a
   key to `var_int(&ta, 1)`, save that returned pointer in a local
   (e.g. `var_t *old = var_int(&ta, 1); dict_set(d, "k", old);`), then
   `dict_set()` the same key to a second, different `var_t *` (e.g.
   `var_int(&ta, 2)`). Then read `old` directly (not through the
   dict — e.g. `CHECK(var_to_int(old) == 1, "old value survives
   dict_set overwrite")`). This confirms the old value was not freed.
2. **`dict_set_owned()` frees and still works correctly:** create a
   dict, `dict_set_owned()` a key to a fresh value, then
   `dict_set_owned()` the same key to a second fresh value, then
   confirm `dict_get()` on that key returns the second value
   (`CHECK(var_to_int(dict_get(d, "k")) == <second value>, ...)`).
   This is a functional check; the real check for absence of
   use-after-free/double-free is the ASan run in "Verify" below, not
   this test by itself.
3. **`dict_set_owned()` with a `NULL` old value:** `dict_set_owned(d,
   "k", NULL)` then `dict_set_owned(d, "k", var_int(&ta, 5))`, confirm
   it doesn't crash and `dict_get()` returns `5`. This exercises the
   `ta_free(NULL)` path from Task 3 through `dict_set_owned()`.

**Verify:** `make clean && make runtests` and `make asan-tests` — all
green, no new warnings, no ASan reports.

---

## Task 7 — Safe rehashing: `dict_rehash()` + automatic growth

**Files:** `src/dict.h`, `src/dict.c`

**Why this is last:** it's the largest and riskiest change, and
benefits from `dict_entry_t`'s simplified layout (Task 5) and the
`dict_set_impl()` split (Task 6) already being in place — this task's
auto-growth trigger hooks into `dict_set_impl()`, so it applies to
both `dict_set()` and `dict_set_owned()` automatically.

**The one rule that matters more than anything else in this task:**
`d->buckets` is an array of `list_t` sentinel nodes, and each sentinel
is a **circular, self-referential** list head (`INIT_LIST_HEAD` makes
an empty bucket's `next`/`prev` point at itself). If you ever move or
resize this array with `ta_realloc()` (or `memcpy`), every sentinel's
self-pointer becomes stale, and any bucket that currently has entries
has entries pointing at the *old* sentinel address. This corrupts the
list silently and will crash later, not immediately, making it hard to
debug. **Never call `ta_realloc()` on `d->buckets`.** The only safe way
to grow the table is: allocate a brand new bucket array, move each
entry's list node into it one at a time via `list_del()` +
`list_add_tail()`, then discard the old array. That is exactly what
`dict_rehash()` below does — follow it exactly.

**Step 7a — `src/dict.h`:** add this declaration, near
`dict_new_sized()`:
```c
/*
 * dict_rehash() - resize d's bucket table to new_nbuckets and
 * re-distribute all existing entries into it.
 *
 * new_nbuckets must be a power of 2 and >= 4 (same constraint as
 * dict_new_sized()). On success, d->buckets and d->nbuckets are
 * updated in place and the old bucket array is freed. On failure
 * (invalid new_nbuckets, or allocation failure), d is left completely
 * unmodified and -1 is returned — it is always safe to keep using d
 * with its old table after a failed rehash.
 *
 * You normally don't need to call this directly — dict_set() and
 * dict_set_owned() call it automatically when the load factor crosses
 * ~0.75. It's exposed for callers who want to pre-size a dict at a
 * specific growth point.
 */
int dict_rehash( dict_t *d, uint32_t new_nbuckets);
```

**Step 7b — `src/dict.c`:** add this new function. Place it right
after `dict_new()` and before the "CRUD" section comment:
```c
int dict_rehash( dict_t *d, uint32_t new_nbuckets){
    uint32_t  i;
    list_t   *old_buckets;
    uint32_t  old_nbuckets;
    list_t   *new_buckets;
    list_t   *pos, *tmp;

    if ( d == NULL) {
        TRACE_ERR( "d is NULL");
        return( -1);
    }
    if ( new_nbuckets < 4 || !is_power_of_two( new_nbuckets)) {
        TRACE_ERR( "invalid new_nbuckets %u", new_nbuckets);
        return( -1);
    }

    new_buckets = ta_calloc( d->gc, new_nbuckets, sizeof( list_t));
    if ( new_buckets == NULL) {
        TRACE_ERR( "ta_calloc failed for %u buckets", new_nbuckets);
        return( -1);
    }
    for ( i = 0; i < new_nbuckets; i++) {
        INIT_LIST_HEAD( &new_buckets[i]);
    }

    old_buckets  = d->buckets;
    old_nbuckets = d->nbuckets;

    /* Move every entry's list node into the new table. Never realloc
     * or memcpy the bucket array itself — see the comment above this
     * function's declaration in dict.h. */
    for ( i = 0; i < old_nbuckets; i++) {
        list_for_each_safe( pos, tmp, &old_buckets[i]) {
            dict_entry_t *e = container_of( pos, dict_entry_t, node);
            size_t new_b = bucket_for_n( e->key, new_nbuckets);

            list_del( pos);
            list_add_tail( pos, &new_buckets[new_b]);
        }
    }

    d->buckets  = new_buckets;
    d->nbuckets = new_nbuckets;
    ta_free( old_buckets);

    return( 0);
}
```

**Step 7c — automatic growth trigger.** In `dict_set_impl()` (added in
Task 6), find the end of the function:
```c
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;
    return( 0);
}
```
Replace with:
```c
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;

    /* grow at ~0.75 load factor; if it fails, keep going with the
     * table we already have — the insert above already succeeded, so
     * a rehash failure here must not turn into a dict_set_impl()
     * failure. Applies to both dict_set() and dict_set_owned(), since
     * both call this shared function. */
    if ( d->count * 4 >= (size_t)d->nbuckets * 3 &&
         d->nbuckets <= (UINT32_MAX / 2)) {
        dict_rehash( d, d->nbuckets * 2);
    }

    return( 0);
}
```
(`stdint.h` is not currently included in `dict.c` — add `#include
<stdint.h>` near the top with the other includes, for `UINT32_MAX`.)

**Test to add:** in `src/testdict.c`, add a test that creates a dict
with `dict_new_sized( &ta, 4)` (the minimum size), then `dict_set()`s
at least 10 distinct keys (enough to force at least one automatic
rehash past the 0.75 load factor a couple of times), then:
- checks `dict_count()` equals the number of keys inserted,
- checks `dict_get()` on every one of those keys still returns the
  correct value (this is the real test — it confirms no entry was
  lost or corrupted during rehashing),
- checks `d->nbuckets` is now larger than `4` (confirms growth
  actually happened; this requires including `dict.h`, which exposes
  the struct fields, so this is fine to check directly).

Also add one direct test of `dict_rehash()`'s failure path: call
`dict_rehash( d, 5)` (not a power of 2) and confirm it returns `-1`
and that the dict is still fully intact afterward (re-check a couple
of existing keys with `dict_get()`).

**Verify:** `make clean && make runtests` and `make asan-tests` — all
green, no new warnings, no ASan reports. This task moves list nodes
around a lot, which is exactly the kind of change ASan is good at
catching if something's wrong, so don't skip the ASan run here.

---

## After all seven tasks

1. Confirm `cd src && make clean && make runtests` and `cd src && make
   asan-tests` both pass cleanly one final time.
2. Update `API.md` if it documents `dict.c`'s public functions
   individually — add entries for `dict_set_owned()` and
   `dict_rehash()` in the same style as the existing `dict_set()` /
   `dict_new_sized()` entries, including the ownership-contract
   wording from `dict.h` and the "safe to keep using `d` after a
   failed rehash" guarantee.
3. Write a short summary of what changed, task by task, and list
   anything you noticed along the way that this plan told you not to
   fix (per Ground Rule 3) — hand that list back to the user rather
   than acting on it.

## Deferred / future work (not part of this plan, do not start these)

- Hash-seed randomization for `bucket_for_n()` (documented, not
  fixed — see "Explicitly out of scope").
- Open-addressing hash table redesign for better cache locality and
  lower per-entry overhead.
- Shrinking `ta_node_t`'s debug-label overhead (`MAX_DBG_STR_LEN`) —
  affects the whole library, needs its own review.
- Any dict-shrinking (rehashing down) on mass deletion — `dict_rehash`
  as written can be called manually to shrink, but nothing triggers it
  automatically, by design (avoids thrashing on delete/insert churn
  near a threshold).
- Auditing existing (or future) call sites to see whether any of them
  could safely switch from `dict_set()` to `dict_set_owned()` to
  reclaim memory sooner. Not started or evaluated by this plan —
  `dict_set_owned()` is added but unused everywhere.
