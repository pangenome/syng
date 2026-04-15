/*
 * impg_syng_helpers.c — thin wrappers around syng static-inline functions
 * so that Rust FFI can call them.
 */

#include <pthread.h>
#include "seqhash.h"
#include "utils.h"
#include "syng.h"

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

/* Thread-safe seqhash creation.
   Upstream `seqhashCreate` calls srandom/random, which share a single
   global libc PRNG state — concurrent calls from multiple threads race
   and produce non-deterministic factor/shift values. glibc's reentrant
   `random_r` fixes this but doesn't exist on macOS / BSD libc.
   A portable fix is to serialize the upstream call with a mutex:
   preserves exact upstream behavior and works everywhere. The mutex
   contention is negligible since this function is called at most a few
   times per index build. */
static pthread_mutex_t impg_seqhash_mutex = PTHREAD_MUTEX_INITIALIZER;

Seqhash *impg_seqhashCreateSafe(int k, int w, int seed)
{
    pthread_mutex_lock(&impg_seqhash_mutex);
    Seqhash *sh = seqhashCreate(k, w, seed);
    pthread_mutex_unlock(&impg_seqhash_mutex);
    return sh;
}

/* Suppress syngBWTnext debug output by setting pathCount to a non-zero value.
   The debug print triggers when pathCount == PATH_DEBUG (both default to 0). */
extern int pathCount;
void impg_syng_suppress_debug(void)
{
    pathCount = -1;
}
