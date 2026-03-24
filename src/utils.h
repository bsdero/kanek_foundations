/*
 * utils.h
 * String utility functions.
 *
 * Part of the Kanek Foundation Library (KFL).
 * KANEK Storage Project.
 */
#ifndef _UTILS_H_
#define _UTILS_H_

char *trim(char *s);

/*
 * str_starts_with() - check if a string starts with a prefix.
 * Returns 1 if s starts with prefix, 0 otherwise.
 * Returns 0 if either argument is NULL.
 */
int str_starts_with(const char *s, const char *prefix);

/*
 * str_ends_with() - check if a string ends with a suffix.
 * Returns 1 if s ends with suffix, 0 otherwise.
 * Returns 0 if either argument is NULL.
 */
int str_ends_with(const char *s, const char *suffix);

#endif
