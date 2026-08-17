# Plan: improve krand64 test coverage

Scope: `testrand.c` / `krand64.c` / `krand64.h` only. Does not cover
replacing the mixing algorithm itself (tracked separately as a design
question from the earlier review) — this plan only makes the *existing*
generator's behavior verifiable and regression-safe.

## Status (2026-08-17)

Phases 1, 2, and 3 are done. Phase 4 is still open.

## Current gaps

- ~~`testrand.c` only prints bucket counts and a thread-safety smoke
  run — there is no `assert`/non-zero exit on bad output, so a
  regression that skews the distribution or breaks reentrancy would
  not fail `make runtests`.~~ **Fixed in Phase 1.**
- ~~No coverage of API contract edges: `max == 0` (raw passthrough),
  `max == 1`, seed determinism/reproducibility, or `krand64_r` matching
  `krand64`'s algorithm when seeded identically.~~ **Fixed in Phase 2.**
- ~~The "thread safety test" runs one thread on the global path and
  one thread on an independent reentrant state — it never puts
  multiple threads in contention on the shared mutex/`_kseed64`, so it
  can't catch a race in the locking itself. No `-fsanitize=thread`
  build exists to catch this class of bug directly.~~ **Fixed in
  Phase 3.**
- ~~`SAMPLES = 100000000` always runs as part of `make runtests`, so
  the test suite pays a heavy, fixed cost with no fast/slow split.~~
  **Fixed in Phase 1.**
- No period/bit-bias checks — only the per-bucket frequency for
  `DICE_SIDES = 100` is inspected, and only by eye. **Still open —
  Phase 4.**

## Phase 1 — Make the existing test pass/fail (highest priority, small) — DONE

- [x] Replace the printed-only distribution check with a chi-square
  goodness-of-fit test over the `DICE_SIDES` buckets against a fixed
  critical value (e.g. df=99, alpha=0.01); `assert()`/`exit(1)` on
  failure so `make runtests` actually catches a skewed generator.
  Implemented as `chi_square_stat()` + `test_distribution()` in
  `testrand.c`, critical value `CHI2_CRITICAL = 134.642`.
- [x] Drop the default `SAMPLES` used by `make runtests` to something
  that runs in a couple seconds (e.g. 2,000,000) — enough for a stable
  chi-square result at this bucket count. Keep an opt-in heavy mode
  (env var, e.g. `KRAND_FULL=1`, or an optional CLI arg) that restores
  100,000,000 samples for manual/occasional runs; no new source file or
  Makefile target needed. Implemented: `SAMPLES_FAST` (2,000,000)
  default, `SAMPLES_FULL` (100,000,000) via `KRAND_FULL=1` env var.

Verified: `make testrand` builds clean, `./testrand` and
`KRAND_FULL=1 ./testrand` both exit 0, `make runtests` passes.

## Phase 2 — API contract / edge cases (small–medium) — DONE

- [x] `max == 0`: assert the raw output is non-degenerate (not
  constant) across many calls. `test_max_zero()`.
- [x] `max == 1`: assert every draw is exactly `0`. `test_max_one()`.
- [x] `max` near `UINT64_MAX`: assert no crash and every result
  `< max`. `test_max_near_uint64_max()`.
- [x] Determinism: `set_kseed64(X)`, draw N values, `set_kseed64(X)`
  again, draw N values again — assert the two sequences are identical
  (single-threaded, no interleaving). `test_determinism()`.
- [x] `krand64_r` state advancement: two independently-seeded
  `uint64_t` states initialized to the same value produce identical
  sequences via `krand64_r`, proving the reentrant path is a pure
  function of state (no hidden shared mutation).
  `test_reentrant_purity()`.

## Phase 3 — Real concurrency coverage (medium) — DONE

- [x] Replace the current one-thread-per-path smoke test with an
  actual contention test: spawn N threads (`CONTENTION_THREADS = 8`)
  all calling the global `krand64()` concurrently and at volume, each
  writing to its own pre-sized slice of a results array (no
  shared-write races in the test itself). Asserts: aggregated bucket
  counts sum to the expected total sample count, and the aggregated
  distribution still passes the Phase 1 chi-square check. Implemented
  as `thread_contention()` + `test_contention()` in `testrand.c`,
  wired into `main()` using the same `samples` value (fast/full split
  via `KRAND_FULL`) as `test_distribution()`.
- [x] A `tsan` target already exists in the Makefile (builds
  `testrand_tsan` with `-fsanitize=thread` from `testrand.c` +
  `krand64.c` and runs it via `setarch ... -R`), not part of default
  `make runtests`. No new target was needed — `make tsan` now
  exercises `test_contention()` along with the rest of the suite.

Verified: `make testrand` builds clean; `./testrand` and
`KRAND_FULL=1 ./testrand` both exit 0 with the contention test's
chi-square statistic well under the critical value (~1.27 fast,
~0.07 full, vs. 134.642); `make runtests` passes; `make tsan` exits 0
with no ThreadSanitizer race warnings.

## Phase 4 — Regression baselines beyond bucket frequency (medium) — OPEN

- Period smoke check: since the generator is driven by a monotonic
  counter, assert no repeated raw (`max == 0`) 64-bit output occurs in
  the first ~1,000,000 draws — a cheap canary for a construction that
  degenerates to a short cycle.
- Bit-bias check: over a large raw-output sample, assert each of the 64
  output bits is set within a reasonable tolerance of 50% (adds a
  dimension chi-square-over-buckets doesn't cover).
- Pair with a doc-only change to `krand64.h`: state explicitly that
  this is not a cryptographically secure generator, so future callers
  don't reach for it where that property is required.

## Out of scope

- Swapping the mixing function for a vetted algorithm (splitmix64,
  PCG, xoshiro) — a design decision, not a testing one; flagged
  separately.
- Formal statistical test suites (dieharder/PractRand/TestU01) — high
  value but heavyweight external dependencies; worth reconsidering only
  if the generator itself is redesigned.

## Priority / effort summary

| Phase | Description                          | Effort | Priority | Status |
|-------|---------------------------------------|--------|----------|--------|
| 1     | Assert-based chi-square + fast default | S    | Must     | Done   |
| 2     | Edge cases + determinism               | S–M  | Must     | Done   |
| 3     | Real contention test + TSan target     | M    | Should   | Done   |
| 4     | Period/bit-bias regression baselines   | M    | Should   | Open   |
