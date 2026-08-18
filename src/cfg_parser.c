#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <libgen.h>     /* dirname() */
#include "cfg_parser.h"
#include "utils.h"
#include "trace.h"

#define CFG_MAX_LINE       1024
#define CFG_MAX_TOK        512
#define CFG_MAX_DEPTH      16      /* max include nesting depth */
#define CFG_CONCAT_INIT_CAP 1024   /* initial '+' concat buffer size */

/* Forward declaration — parse_line() calls this for 'include'. */
static int cfg_load_depth( kfl_cfg_t *cfg, const char *filename, int depth);


/* ═══════════════════════════════════════════════════════════════════════════
 * Tokeniser
 * ════════════════════════════════════════════════════════════════════════ */

typedef enum {
    TOK_EOF,
    TOK_IDENT,      /* [A-Za-z_][A-Za-z0-9_]* */
    TOK_STRING,     /* "..."  (quotes stripped, \n \t \\ \" handled) */
    TOK_INT,        /* 123 / -123 */
    TOK_FLOAT,      /* 3.14 */
    TOK_EQ,         /* = */
    TOK_PLUSEQ,     /* += */
    TOK_PLUS,       /* + */
    TOK_LBRACKET,   /* [ */
    TOK_RBRACKET,   /* ] */
    TOK_COMMA,      /* , */
    TOK_HASH,       /* # → rest of line is a comment */
} tok_type_t;

typedef struct {
    tok_type_t  type;
    char        sval[CFG_MAX_TOK];  /* IDENT / STRING value */
    int64_t     ival;               /* INT value */
    double      fval;               /* FLOAT value */
} tok_t;

typedef struct {
    const char *p;      /* current scan position in the line */
    int         error;  /* set to 1 when a malformed token is seen */
} lex_t;

static void lex_skip_ws( lex_t *l){
    while ( *l->p == ' ' || *l->p == '\t' || *l->p == '\r'){
        l->p++;
    }
}

static tok_t next_tok( lex_t *l){
    tok_t t;
    memset( &t, 0, sizeof( t));

    lex_skip_ws( l);

    if ( !*l->p || *l->p == '\n'){
        t.type = TOK_EOF;
        return( t);
    }

    /* inline comment */
    if ( *l->p == '#'){
        t.type = TOK_HASH;
        l->p++;
        return( t);
    }

    /* quoted string */
    if ( *l->p == '"'){
        int i = 0;
        l->p++;
        while ( *l->p && *l->p != '"' && i < CFG_MAX_TOK - 1){
            if ( *l->p == '\\' && *(l->p + 1)){
                l->p++;
                switch ( *l->p){
                    case 'n':  t.sval[i++] = '\n'; break;
                    case 't':  t.sval[i++] = '\t'; break;
                    default:   t.sval[i++] = *l->p; break;
                }
            } else {
                t.sval[i++] = *l->p;
            }
            l->p++;
        }
        t.sval[i] = '\0';
        if ( *l->p == '"'){
            l->p++;
        } else {
            TRACE_ERR( "unterminated string literal");
            l->error = 1;
        }
        t.type = TOK_STRING;
        return( t);
    }

    /* operators: += before + before = */
    if ( *l->p == '+' && *(l->p + 1) == '='){
        t.type = TOK_PLUSEQ;
        l->p += 2;
        return( t);
    }
    if ( *l->p == '+'){
        t.type = TOK_PLUS;
        l->p++;
        return( t);
    }
    if ( *l->p == '='){
        t.type = TOK_EQ;
        l->p++;
        return( t);
    }
    if ( *l->p == '['){
        t.type = TOK_LBRACKET;
        l->p++;
        return( t);
    }
    if ( *l->p == ']'){
        t.type = TOK_RBRACKET;
        l->p++;
        return( t);
    }
    if ( *l->p == ','){
        t.type = TOK_COMMA;
        l->p++;
        return( t);
    }

    /* number: leading digit or '-' followed by digit */
    if ( isdigit( (unsigned char)*l->p) ||
       (*l->p == '-' && isdigit( (unsigned char)*(l->p + 1)))){
        char nbuf[64];
        int  i = 0, is_float = 0, dot_count = 0;
        if ( *l->p == '-'){
            nbuf[i++] = *l->p++;
        }
        while ( *l->p && (isdigit( (unsigned char)*l->p) || *l->p == '.') &&
               i < 63) {
            if ( *l->p == '.'){
                is_float = 1;
                dot_count++;
            }
            nbuf[i++] = *l->p++;
        }
        nbuf[i] = '\0';
        if ( dot_count > 1){
            TRACE_ERR( "malformed numeric literal '%s'", nbuf);
            l->error = 1;
        }
        if ( is_float){
            t.type = TOK_FLOAT;
            t.fval = strtod( nbuf, NULL);
        } else {
            t.type = TOK_INT;
            t.ival = strtoll( nbuf, NULL, 10);
        }
        return( t);
    }

    /* identifier */
    if ( isalpha( (unsigned char)*l->p) || *l->p == '_'){
        int i = 0;
        while ( *l->p && (isalnum( (unsigned char)*l->p) || *l->p == '_') &&
              i < CFG_MAX_TOK - 1){
            t.sval[i++] = *l->p++;
        }
        t.sval[i] = '\0';
        t.type = TOK_IDENT;
        return( t);
    }

    /* unknown character — skip silently */
    l->p++;
    t.type = TOK_EOF;
    return( t);
}

static tok_t peek_tok( lex_t *l){
    const char *saved = l->p;
    tok_t t = next_tok( l);
    l->p = saved;
    return( t);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Evaluator
 * ════════════════════════════════════════════════════════════════════════ */

/* Evaluate a single value atom.  'tok' is the already-consumed lead token. */
static var_t *eval_atom( lex_t *l, tok_t tok, kfl_cfg_t *cfg){
    ta_list_t *gc = cfg->gc;

    switch ( tok.type) {

        case TOK_STRING:
            return( var_str( gc, tok.sval));

        case TOK_INT:
            return( var_int( gc, tok.ival));

        case TOK_FLOAT:
            return( var_float( gc, tok.fval));

        case TOK_IDENT: {
            /* boolean / null literals */
            if ( strcmp( tok.sval, "true")  == 0) {
                return( var_bool( gc, 1));
            }
            if ( strcmp( tok.sval, "false") == 0) {
                return( var_bool( gc, 0));
            }
            if ( strcmp( tok.sval, "null")  == 0) {
                return( var_null( gc));
            }

            /* variable reference — look up in the dict */
            var_t *v = dict_get( cfg->vars, tok.sval);

            /* array indexing: IDENT[N] */
            if ( peek_tok( l).type == TOK_LBRACKET) {
                next_tok( l);                    /* consume [ */
                tok_t idx = next_tok( l);
                tok_t rb  = next_tok( l);         /* expect ] */

                if ( rb.type != TOK_RBRACKET) {
                    TRACE_ERR( "expected ']' after array index");
                    l->error = 1;
                    return( var_null( gc));
                }
                if ( idx.type != TOK_INT) {
                    TRACE_ERR( "array index must be an integer");
                    l->error = 1;
                    return( var_null( gc));
                }
                if ( v == NULL) {
                    TRACE_ERR( "undefined variable '%s'", tok.sval);
                    return( var_null( gc));
                }
                var_t *elem = var_get( v, (size_t)idx.ival);
                return( (elem != NULL) ? elem : var_null( gc));
            }

            if ( v == NULL) {
                TRACE_ERR( "undefined variable '%s'", tok.sval);
                return( var_null( gc));
            }
            return( v);
        }

        case TOK_LBRACKET: {
            /* array literal:  [ item, item, ... ] */
            var_t *arr = var_array( gc);
            if ( arr == NULL) {
                TRACE_ERR( "var_array failed");
                return( var_null( gc));
            }

            while ( 1) {
                tok_t pk = peek_tok( l);
                if ( pk.type == TOK_RBRACKET || pk.type == TOK_EOF ||
                   pk.type == TOK_HASH) {
                    break;
                }
                if ( pk.type == TOK_COMMA) {
                    next_tok( l);    /* skip commas */
                    continue;
                }
                if ( pk.type != TOK_STRING && pk.type != TOK_INT &&
                   pk.type != TOK_FLOAT && pk.type != TOK_IDENT &&
                   pk.type != TOK_LBRACKET) {
                    TRACE_ERR( "unexpected token in array literal");
                    l->error = 1;
                    next_tok( l);    /* consume and skip the bad token */
                    continue;
                }
                tok_t item_tok = next_tok( l);
                var_t *item = eval_atom( l, item_tok, cfg);
                if ( item != NULL) {
                    var_push( gc, arr, item);
                }
            }
            if ( peek_tok( l).type == TOK_RBRACKET) {
                next_tok( l); /* consume ] */
            }
            return( arr);
        }

        default:
            return( var_null( gc));
    }
}

/* Evaluate:  value ( '+' value )*
 * '+' always performs string concatenation (per spec). */
static var_t *eval_rhs( lex_t *l, kfl_cfg_t *cfg){
    ta_list_t *gc = cfg->gc;

    tok_t tok = next_tok( l);
    if ( tok.type == TOK_EOF || tok.type == TOK_HASH) {
        return( var_null( gc));
    }

    var_t *result = eval_atom( l, tok, cfg);

    if ( peek_tok( l).type != TOK_PLUS) {
        return( (result != NULL) ? result : var_null( gc));
    }

    /* concatenation chain: accumulate into one growable buffer instead
     * of reallocating + copying the whole result on every '+' */
    size_t cap = CFG_CONCAT_INIT_CAP;
    char  *buf = ta_malloc( gc, cap);
    if ( buf == NULL) {
        TRACE_ERR( "ta_malloc failed for %zu bytes", cap);
        return( (result != NULL) ? result : var_null( gc));
    }

    char  *rs0  = var_to_str( gc, result);
    size_t len  = (rs0 != NULL) ? strlen( rs0) : 0;
    if ( len + 1 > cap) {
        cap = len + 1;
        buf = ta_realloc( gc, buf, cap);
    }
    if ( buf != NULL) {
        if ( rs0 != NULL) {
            memcpy( buf, rs0, len + 1);
        } else {
            buf[0] = '\0';
        }
    }
    if ( rs0 != NULL) {
        ta_free( rs0);
    }

    while ( buf != NULL && peek_tok( l).type == TOK_PLUS) {
        next_tok( l);                        /* consume + */
        tok_t rtok = next_tok( l);
        if ( rtok.type == TOK_EOF || rtok.type == TOK_HASH) {
            break;
        }

        var_t *rhs  = eval_atom( l, rtok, cfg);
        char  *rs   = var_to_str( gc, rhs);
        size_t rlen = (rs != NULL) ? strlen( rs) : 0;

        if ( len + rlen + 1 > cap) {
            while ( cap < len + rlen + 1) {
                cap *= 2;
            }
            char *newbuf = ta_realloc( gc, buf, cap);
            if ( newbuf == NULL) {
                TRACE_ERR( "ta_realloc failed for %zu bytes", cap);
                if ( rs != NULL) {
                    ta_free( rs);
                }
                break;
            }
            buf = newbuf;
        }
        if ( rs != NULL) {
            memcpy( buf + len, rs, rlen + 1);
            ta_free( rs);
        }
        len += rlen;
    }

    if ( buf == NULL) {
        return( (result != NULL) ? result : var_null( gc));
    }

    result = var_str( gc, buf);
    ta_free( buf);

    return( (result != NULL) ? result : var_null( gc));
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Line parser
 * ════════════════════════════════════════════════════════════════════════ */

static int parse_line( kfl_cfg_t *cfg, const char *raw_line,
                      const char *dir, int depth){
    ta_list_t *gc = cfg->gc;
    char line[CFG_MAX_LINE];
    lex_t l;

    /* work on a local copy so we can call trim() on it */
    strncpy( line, raw_line, sizeof( line) - 1);
    line[sizeof( line) - 1] = '\0';
    trim( line);

    l.p = line;
    l.error = 0;

    tok_t first = next_tok( &l);

    /* blank lines and full-line comments */
    if ( first.type == TOK_EOF || first.type == TOK_HASH) {
        return( 0);
    }

    if ( first.type != TOK_IDENT) {
        TRACE_ERR( "expected identifier at start of line: '%s'", line);
        return( -1);
    }

    /* ── include directive ────────────────────────────────────────────── */
    if ( strcmp( first.sval, "include") == 0) {
        lex_skip_ws( &l);

        /* take the rest of the line as the filename (strip quotes) */
        char fname[CFG_MAX_TOK];
        strncpy( fname, l.p, sizeof( fname) - 1);
        fname[sizeof( fname) - 1] = '\0';
        trim( fname);

        size_t flen = strlen( fname);
        if ( flen >= 2 && fname[0] == '"' && fname[flen - 1] == '"') {
            memmove( fname, fname + 1, flen - 2);
            fname[flen - 2] = '\0';
        }

        if ( !fname[0]) {
            TRACE_ERR( "include: missing filename");
            return( -1);
        }

        /* resolve relative path against current file's directory */
        char path[CFG_MAX_LINE];
        if ( fname[0] == '/') {
            strncpy( path, fname, sizeof( path) - 1);
        } else {
            snprintf( path, sizeof( path), "%s/%s", dir, fname);
        }
        path[sizeof( path) - 1] = '\0';

        return( cfg_load_depth( cfg, path, depth + 1));
    }

    /* ── display directive ────────────────────────────────────────────── */
    if ( strcmp( first.sval, "display") == 0) {
        var_t *v = eval_rhs( &l, cfg);
        /* print the human-readable value, not the var_print repr:
         * strings print without quotes, arrays/dicts fall back to
         * var_print */
        if ( v != NULL && v->type == VAR_STR) {
            printf( "%s\n", (v->s != NULL) ? v->s : "");
        } else {
            var_print( v);
            printf( "\n");
        }
        return( l.error ? -1 : 0);
    }

    /* ── KEY = RHS  or  KEY += RHS ────────────────────────────────────── */
    char key[CFG_MAX_TOK];
    strncpy( key, first.sval, sizeof( key) - 1);
    key[sizeof( key) - 1] = '\0';

    tok_t op = next_tok( &l);

    if ( op.type == TOK_EQ) {
        var_t *val = eval_rhs( &l, cfg);
        int    rc  = dict_set( cfg->vars, key, val);
        return( ( l.error || rc < 0) ? -1 : 0);
    }

    if ( op.type == TOK_PLUSEQ) {
        /* get existing value (as string), append the new RHS */
        var_t *existing = dict_get( cfg->vars, key);
        char *es = (existing != NULL) ? var_to_str( gc, existing)
                                       : ta_strdup( gc, "");

        var_t *rhs = eval_rhs( &l, cfg);
        char *rs = var_to_str( gc, rhs);

        size_t elen = (es != NULL) ? strlen( es) : 0;
        size_t rlen = (rs != NULL) ? strlen( rs) : 0;
        char *buf = ta_malloc( gc, elen + rlen + 1);
        if ( buf == NULL) {
            TRACE_ERR( "ta_malloc failed for %zu bytes", elen + rlen + 1);
            return( -1);
        }
        if ( es != NULL) {
            memcpy( buf,        es, elen);
        }
        if ( rs != NULL) {
            memcpy( buf + elen, rs, rlen);
        }
        buf[elen + rlen] = '\0';

        int rc = dict_set( cfg->vars, key, var_str( gc, buf));
        return( ( l.error || rc < 0) ? -1 : 0);
    }

    TRACE_ERR( "expected '=' or '+=' after key '%s'", key);
    return( -1);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ════════════════════════════════════════════════════════════════════════ */

kfl_cfg_t *kfl_cfg_new( ta_list_t *gc){
    kfl_cfg_t *cfg = ta_malloc( gc, sizeof( kfl_cfg_t));
    if ( cfg == NULL) {
        TRACE_ERR( "ta_malloc failed for kfl_cfg_t");
        return( NULL);
    }
    cfg->gc   = gc;
    cfg->vars = dict_new( gc);
    if ( cfg->vars == NULL) {
        TRACE_ERR( "dict_new failed");
        ta_free( cfg);
        return( NULL);
    }
    return( cfg);
}

static int cfg_load_depth( kfl_cfg_t *cfg, const char *filename, int depth){
    FILE *fp;
    char  line[CFG_MAX_LINE];
    char  dir_tmp[CFG_MAX_LINE];
    char  dir[CFG_MAX_LINE];
    int   lineno = 0, rc = 0;

    if ( depth > CFG_MAX_DEPTH) {
        TRACE_ERR( "include depth limit (%d) exceeded", CFG_MAX_DEPTH);
        return( -1);
    }

    fp = fopen( filename, "r");
    if ( fp == NULL) {
        TRACE_SYSERR( "cannot open '%s'", filename);
        return( -1);
    }

    /* dirname() may modify its argument; keep a copy and copy the result */
    strncpy( dir_tmp, filename, sizeof( dir_tmp) - 1);
    dir_tmp[sizeof( dir_tmp) - 1] = '\0';
    strncpy( dir, dirname( dir_tmp), sizeof( dir) - 1);
    dir[sizeof( dir) - 1] = '\0';

    while ( fgets( line, sizeof( line), fp)) {
        lineno++;

        if ( strchr( line, '\n') == NULL && !feof( fp)) {
            TRACE_ERR( "%s:%d: line exceeds max length (%d bytes)",
                     filename, lineno, CFG_MAX_LINE - 1);
            rc = -1;

            /* drain the remainder of this line so the next fgets()
             * call starts at the next real line */
            int c;
            while ( (c = fgetc( fp)) != EOF && c != '\n') {
                ;
            }
            continue;
        }

        line[strcspn( line, "\n")] = '\0';   /* strip newline */

        if ( parse_line( cfg, line, dir, depth) < 0) {
            TRACE_ERR( "%s:%d: parse error", filename, lineno);
            rc = -1;    /* keep going — report all errors */
        }
    }

    fclose( fp);
    return( rc);
}

int kfl_cfg_load( kfl_cfg_t *cfg, const char *filename){
    if ( cfg == NULL || filename == NULL) {
        TRACE_ERR( "cfg or filename is NULL");
        return( -1);
    }
    return( cfg_load_depth( cfg, filename, 0));
}

var_t *kfl_cfg_get( kfl_cfg_t *cfg, const char *key){
    if ( cfg == NULL || key == NULL) {
        TRACE_ERR( "cfg or key is NULL");
        return( NULL);
    }
    return( dict_get( cfg->vars, key));
}

static void print_entry( const char *key, var_t *val, void *ud){
    (void)ud;
    printf( "  %-24s = ", key);
    var_print( val);
    printf( "\n");
}

void kfl_cfg_print( kfl_cfg_t *cfg){
    if ( cfg == NULL) {
        TRACE_ERR( "cfg is NULL");
        return;
    }
    printf( "config (%zu vars):\n", dict_count( cfg->vars));
    dict_each( cfg->vars, print_entry, NULL);
}
