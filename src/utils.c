#include <ctype.h>
#include <string.h>
#include "utils.h"



/* Cut spaces */
char *trim( char *s){
  /* Initialize start, end pointers */
  char *s1 = s;
  char *s2;

  s2 = s + strlen( s) - 1;

  /* Trim and delimit right side */
  while ( (s2 >= s1) && (isspace( (int) *s2))){
      s2--;
  }
  *(s2+1) = '\0';

  /* Trim left side */
  while ( (isspace( (int) *s1)) && (s1 < s2)){
      s1++;
  }

  /* Copy finished string */
  strcpy (s, s1);
  return( s);
}

int str_starts_with( const char *s, const char *prefix){
    size_t slen, plen;

    if ( s == NULL || prefix == NULL) {
        return( 0);
    }
    slen = strlen( s);
    plen = strlen( prefix);
    if ( plen > slen) {
        return( 0);
    }
    return( strncmp( s, prefix, plen) == 0);
}

int str_ends_with( const char *s, const char *suffix){
    size_t slen, sflen;

    if ( s == NULL || suffix == NULL) {
        return( 0);
    }
    slen  = strlen( s);
    sflen = strlen( suffix);
    if ( sflen > slen) {
        return( 0);
    }
    return( strncmp( s + slen - sflen, suffix, sflen) == 0);
}
