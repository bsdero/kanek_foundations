# To enable AddressSanitizer manually, add to both CFLAGS and LDFLAGS:
#   CFLAGS  += -fsanitize=address
#   LDFLAGS += -fsanitize=address -static-libasan
# Or just run `make asan-tests`, which does this for you, runs the
# full test suite under it, then restores a normal (non-sanitized)
# build.

CC      = cc
AR      = ar
CFLAGS  = -Wall -DUSER_SPACE -g -O0

# Platform detection: macOS's clang/ld lack a couple of GNU/Linux-only
# knobs used below (a static ASan runtime, and the `setarch` tool), so
# asan-tests/tsan adapt per-OS.
UNAME_S := $(shell uname -s)

LIBKFL  = libkfl.a
# -rdynamic exports all symbols to the dynamic symbol table so that
# backtrace_symbols() in panic.c can resolve function names at runtime.
LDFLAGS = -L. -lkfl -rdynamic -lpthread

LIB_OBJS = krand64.o hash.o dumphex.o ta.o map.o utils.o var.o \
           dict.o trace.o panic.o cfg_parser.o crc32c.o

TESTS = testrand testhash testdh test_dumphex testgc testmap \
        testvar testpanic testconfig testutils testcrc32c testta testdict

# Check whether the compiler supports -msse4.2 (returns 0 on success).
SSE42_OK := $(shell $(CC) -msse4.2 -x c -c /dev/null \
                    -o /dev/null 2>/dev/null; echo $$?)

all: $(LIBKFL) tests

$(LIBKFL): $(LIB_OBJS)
	$(AR) rcs $(LIBKFL) $(LIB_OBJS)

tests: $(LIBKFL) $(TESTS)

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

# Build and run the full test suite under AddressSanitizer, then
# restore a normal (non-sanitized) build. Run this as part of
# verifying any change to ta.c, dict.c, or var.c — a plain build will
# not reliably catch use-after-free / double-free bugs.
#
# -static-libasan links ASan's runtime statically; macOS's clang only
# ships a dynamic ASan runtime (dylib), so that flag is Linux-only.
ifeq ($(UNAME_S),Darwin)
ASAN_LDFLAGS = -L. -lkfl -rdynamic -lpthread -fsanitize=address
else
ASAN_LDFLAGS = -L. -lkfl -rdynamic -lpthread -fsanitize=address -static-libasan
endif

asan-tests:
	$(MAKE) clean
	$(MAKE) CFLAGS="-Wall -DUSER_SPACE -g -O0 -fsanitize=address" \
	        LDFLAGS="$(ASAN_LDFLAGS)" \
	        runtests
	$(MAKE) clean
	$(MAKE) tests

# test binaries
testrand: testrand.o $(LIBKFL)
	$(CC) -o $@ testrand.o $(LDFLAGS)

testhash: testhash.o $(LIBKFL)
	$(CC) -o $@ testhash.o $(LDFLAGS)

testdh: testdh.o $(LIBKFL)
	$(CC) -o $@ testdh.o $(LDFLAGS)

test_dumphex: test_dumphex.o $(LIBKFL)
	$(CC) -o $@ test_dumphex.o $(LDFLAGS)

testgc: testgc.o $(LIBKFL)
	$(CC) -o $@ testgc.o $(LDFLAGS)

testmap: testmap.o $(LIBKFL)
	$(CC) -o $@ testmap.o $(LDFLAGS)

testvar: testvar.o $(LIBKFL)
	$(CC) -o $@ testvar.o $(LDFLAGS)

testpanic: testpanic.o $(LIBKFL)
	$(CC) -o $@ testpanic.o $(LDFLAGS)

testconfig: testconfig.o $(LIBKFL)
	$(CC) -o $@ testconfig.o $(LDFLAGS) -lm

testutils: testutils.o $(LIBKFL)
	$(CC) -o $@ testutils.o $(LDFLAGS)

testta: testta.o $(LIBKFL)
	$(CC) -o $@ testta.o $(LDFLAGS)

testdict: testdict.o $(LIBKFL)
	$(CC) -o $@ testdict.o $(LDFLAGS)

testcrc32c: testcrc32c.o $(LIBKFL)
	$(CC) -o $@ testcrc32c.o $(LDFLAGS)

# crc32c.o: add -msse4.2 only when the compiler supports it.
ifeq ($(SSE42_OK),0)
crc32c.o: crc32c.c
	$(CC) $(CFLAGS) -msse4.2 -c -o $@ $<
endif

# `setarch -R` disables ASLR, which is util-linux/Linux-only tooling
# with no macOS equivalent; TSan's own address layout doesn't need it
# there, so just run the binary directly on Darwin.
tsan: testrand.c krand64.c
	$(CC) $(CFLAGS) -fsanitize=thread -o testrand_tsan \
	    testrand.c krand64.c -lpthread
ifeq ($(UNAME_S),Darwin)
	./testrand_tsan
else
	setarch $$(uname -m) -R ./testrand_tsan
endif

clean:
	rm -f *.o $(TESTS) $(LIBKFL) testrand_tsan
