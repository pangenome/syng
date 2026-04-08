/*
 * impg_syng_helpers.c — thin wrappers around syng static-inline functions
 * so that Rust FFI can call them.
 */

#include "seqhash.h"
#include "utils.h"

/* seqhashIteratorDestroy is static inline in seqhash.h — wrap it. */
void impg_seqhashIteratorDestroy(SeqhashIterator *si)
{
    seqhashIteratorDestroy(si);
}

/* seqhashDestroy is static inline in seqhash.h — wrap it. */
void impg_seqhashDestroy(Seqhash *sh)
{
    seqhashDestroy(sh);
}

/* Suppress syngBWTnext debug output by setting pathCount to a non-zero value.
   The debug print triggers when pathCount == PATH_DEBUG (both default to 0). */
extern int pathCount;
void impg_syng_suppress_debug(void)
{
    pathCount = -1;
}
