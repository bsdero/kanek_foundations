# To enable AddressSanitizer, add to both CFLAGS and LDFLAGS:
#   CFLAGS  += -fsanitize=address
#   LDFLAGS += -fsanitize=address -static-libasan

CC      = cc
AR      = ar
CFLAGS  = -Wall -DUSER_SPACE -g -O0

LIBKFL  = libkfl.a
# -rdynamic exports all symbols to the dynamic symbol table so that
# backtrace_symbols() in panic.c can resolve function names at runtime.
LDFLAGS = -L. -lkfl -rdynamic

LIB_OBJS = krand64.o hash.o dumphex.o gc.o map.o utils.o var.o dict.o trace.o panic.o kfl_config.o

TESTS = testrand testhash testdh test_dumphex testgc testmap testvar testpanic testconfig


all: $(LIBKFL) tests

$(LIBKFL): $(LIB_OBJS)
	$(AR) rcs $(LIBKFL) $(LIB_OBJS)

tests: $(LIBKFL) $(TESTS)

runtests: tests
	@echo "--- testrand ---";  ./testrand
	@echo "--- testhash ---";  echo "hello\nworld\nfoo" | ./testhash
	@echo "--- testdh ---";    ./testdh
	@echo "--- testgc ---";    ./testgc
	@echo "--- testmap ---";   ./testmap

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

clean:
	rm -f *.o $(TESTS) $(LIBKFL)
