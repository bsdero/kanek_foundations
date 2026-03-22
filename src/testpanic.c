#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "panic.h"

/* ── helpers to create a meaningful call stack ─────────────────────────── */

static void level3(void){
    printf("[level3] calling stackdump:\n");
    stackdump(stdout);
}

static void level2(void){ level3(); }
static void level1(void){ level2(); }


int main(void){
    int pass = 0, fail = 0;
    int rc;

    /* Subprocess modes — checked first so child processes exit immediately
     * without running the full test suite. */
    if(getenv("TESTPANIC_DO_PANIC")){
        panic(42, "deliberate test panic");
        /* unreachable */
    }
    if(getenv("TESTPANIC_DO_MACRO")){
        PANIC(7, "deliberate macro panic");
        /* unreachable */
    }


    /* Test 1: stackdump() produces output without crashing */
    printf("=== Test 1: stackdump() from a 3-deep call chain ===\n");
    level1();
    printf("stackdump returned normally\n\n");
    pass++;

    /* Test 2: stackdump(NULL) falls back to stderr without crashing */
    printf("=== Test 2: stackdump(NULL) uses stderr ===\n");
    fflush(stdout);
    stackdump(NULL);
    printf("stackdump(NULL) returned normally\n\n");
    pass++;

    /* Test 3: panic() exits with the correct return code */
    printf("=== Test 3: panic() exits with rc=42 ===\n");
    rc = system("TESTPANIC_DO_PANIC=1 ./testpanic 2>/dev/null");
    if(WIFEXITED(rc) && WEXITSTATUS(rc) == 42){
        printf("  PASS: exit code = 42\n\n");
        pass++;
    } else {
        printf("  FAIL: expected rc=42, got %d\n\n", WEXITSTATUS(rc));
        fail++;
    }

    /* Test 4: PANIC() macro output contains file:func:line */
    printf("=== Test 4: PANIC() macro includes source location ===\n");
    rc = system("TESTPANIC_DO_MACRO=1 ./testpanic 2>&1 | grep -q 'testpanic.c'");
    if(WIFEXITED(rc) && WEXITSTATUS(rc) == 0){
        printf("  PASS: 'testpanic.c' found in PANIC() output\n\n");
        pass++;
    } else {
        printf("  FAIL: source location not found in PANIC() output\n\n");
        fail++;
    }

    /* Test 5: PANIC() exits with the correct return code */
    printf("=== Test 5: PANIC() exits with rc=7 ===\n");
    rc = system("TESTPANIC_DO_MACRO=1 ./testpanic 2>/dev/null");
    if(WIFEXITED(rc) && WEXITSTATUS(rc) == 7){
        printf("  PASS: exit code = 7\n\n");
        pass++;
    } else {
        printf("  FAIL: expected rc=7, got %d\n\n", WEXITSTATUS(rc));
        fail++;
    }

    printf("--- RESULTS ---\n");
    printf("PASSED: %d  FAILED: %d\n", pass, fail);
    return fail ? 1 : 0;
}
