# To enable AddressSanitizer manually, add to both CFLAGS and LDFLAGS:
#   CFLAGS  += -fsanitize=address
#   LDFLAGS += -fsanitize=address -static-libasan
# Or just run `make asan-tests`, which does this for you, runs the
# full test suite under it, then restores a normal (non-sanitized)
# build.

CC      = cc
AR      = ar
CFLAGS  = -Wall -DUSER_SPACE -g -O0

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
asan-tests:
	$(MAKE) clean
	$(MAKE) CFLAGS="-Wall -DUSER_SPACE -g -O0 -fsanitize=address" \
	        LDFLAGS="-L. -lkfl -rdynamic -lpthread -fsanitize=address -static-libasan" \
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

tsan: testrand.c krand64.c
	$(CC) $(CFLAGS) -fsanitize=thread -o testrand_tsan \
	    testrand.c krand64.c -lpthread
	setarch $$(uname -m) -R ./testrand_tsan

clean:
	rm -f *.o $(TESTS) $(LIBKFL) testrand_tsan
