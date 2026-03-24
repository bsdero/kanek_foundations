/*
 * testutils.c
 * Tests for string utility functions: str_starts_with, str_ends_with.
 *
 * Part of the Kanek Foundation Library (KFL).
 * KANEK Storage Project.
 */
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

#define PASS "PASS"
#define FAIL "FAIL"
#define CHECK(expr) \
    ((expr) ? (printf("  PASS: %s\n", #expr), passed++) \
            : (printf("  FAIL: %s\n", #expr), failed++))

int main(void)
{
    int passed = 0, failed = 0;

    printf("=== str_starts_with ===\n");
    /* NULL inputs */
    CHECK(str_starts_with(NULL, "abc") == 0);
    CHECK(str_starts_with("abc", NULL) == 0);
    CHECK(str_starts_with(NULL, NULL)  == 0);
    /* empty strings */
    CHECK(str_starts_with("", "")      == 1);
    CHECK(str_starts_with("abc", "")   == 1);
    CHECK(str_starts_with("", "abc")   == 0);
    /* match */
    CHECK(str_starts_with("hello world", "hello") == 1);
    CHECK(str_starts_with("hello", "hello")        == 1);
    /* no match */
    CHECK(str_starts_with("hello world", "world")  == 0);
    CHECK(str_starts_with("hello", "xyz")           == 0);
    /* prefix longer than string */
    CHECK(str_starts_with("hi", "hello")            == 0);

    printf("\n=== str_ends_with ===\n");
    /* NULL inputs */
    CHECK(str_ends_with(NULL, "abc") == 0);
    CHECK(str_ends_with("abc", NULL) == 0);
    CHECK(str_ends_with(NULL, NULL)  == 0);
    /* empty strings */
    CHECK(str_ends_with("", "")      == 1);
    CHECK(str_ends_with("abc", "")   == 1);
    CHECK(str_ends_with("", "abc")   == 0);
    /* match */
    CHECK(str_ends_with("hello world", "world") == 1);
    CHECK(str_ends_with("world", "world")        == 1);
    /* no match */
    CHECK(str_ends_with("hello world", "hello")  == 0);
    CHECK(str_ends_with("hello", "xyz")           == 0);
    /* suffix longer than string */
    CHECK(str_ends_with("hi", "world")            == 0);

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return (failed == 0) ? 0 : 1;
}
