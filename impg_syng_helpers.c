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
