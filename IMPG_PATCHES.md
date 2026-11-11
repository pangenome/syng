# impg patches to syng

This fork of [richarddurbin/syng](https://github.com/richarddurbin/syng) carries
impg-specific patches on the `impg-integration` branch. Keep this file current
as patches are added, removed, or upstreamed.

Upstream `main` should be kept in sync with `richarddurbin/syng:main` so we can
rebase our patches on top as upstream moves. Do not force-push `main` — always
rebase `impg-integration` against it.

## Current patches (on `impg-integration`, newest last)

### `23b6547` — add impg FFI helper wrappers for static-inline functions

Exposes static-inline functions from syng headers so Rust FFI can call them.
Lives entirely in `impg_syng_helpers.c` — no changes to upstream files. Safe to
leave as-is; purely additive. Could be upstreamed as `impg_ffi_helpers.c` if
desired.

### `1dbfd58` — feat: add impg_syng_suppress_debug helper for FFI

Adds a runtime toggle to silence the C `printf` debug output from
`syngBWTnext` et al. Lives in `impg_syng_helpers.c`. Additive, no upstream file
changes.

### local — keep syng GBWT load diagnostics off stdout

Redirects `syngBWTread`'s "read GBWT..." diagnostic and `timeUpdate` summary
from stdout to stderr. impg streams BED/GFA/FASTA query results on stdout, so
native diagnostics must not be mixed into machine-readable output.

### `ce46949` — fix: hash REMOVED sentinel collides with hashInt(1)

**Genuine upstream bug.** Patches `hash.c` line 98.

The original code:
```c
REMOVED = hashInt((I64MAX-1)^I64MAX) ;
```
simplifies to `REMOVED = hashInt(1) = I64MAX - 1`, which is a perfectly
legitimate hash of the integer key `1`. With this sentinel, every
`hashAdd(h, hashInt(1), ...)` call treated the slot as REMOVED and assigned a
fresh index each time, so `startCount(sb, 1, true)` always returned 0 instead
of incrementing. Two `syngBWTpathStartNew(gbwt, 1)` calls would both get
`jLast=0` and collide.

This breaks any syng index built from scratch whenever the very first k-mer
added to the kmer hash ends up as a GBWT start node (which is almost always —
the first sequence's first syncmer gets `kmer_index = 1`). Silent at build
time; crashes at query time as:
```
FATAL ERROR: syngBWTpathStartOld startNode 1 count 0 >= startCount 0
```

Our patch uses a middle-of-range sentinel:
```c
REMOVED.i = 0x4000000000000000LL ;
```
This corresponds to `hashInt(0x3FFFFFFFFFFFFFFF)`, an impractically large key
that no real syng workload will ever use. All existing tests pass, and the
yeast235 pangenome (235 genomes, byte-identical chrIII across samples) now
builds and queries correctly.

**Should be upstreamed.** Reproducer: build a syng index with any two
byte-identical sequences, then query the first one — crashes at the C layer.
The impg test suite has both FFI-level and CLI-level regression tests
(`tests/test_syng_startcount.rs` and
`tests/test_syng_integration.rs::test_syng_identical_sequences_build_and_query`).

### local — make process-global mutable state safe under concurrent queries

`SyngIndex` and `SyngMatcher` are `unsafe impl Send + Sync`, and impg queries
them from Rayon worker threads. Several C translation units kept mutable
*static* state that those concurrent paths write:

| File | State | Fix |
| :--- | :--- | :--- |
| `utils.c` | `totalAllocated`, `maxAllocated`, `rOld`/`rFirst`, `tOld`/`tFirst` | `_Thread_local` |
| `hash.c` | `nCreated`, `nDestroyed`, `nAdded`, `nBounced`, `nFound`, `nNotFound` | `_Thread_local` |
| `array.c` | `totalAllocatedMemory`, `totalNumberCreated`, `totalNumberActive`, `reportArray` | `_Thread_local` |
| `rskip.c` | `static U64 rng` in `randomDepth` | `_Thread_local` |
| `syngbwt3.c` | `rsWrite` scratch pointers `iSym`/`runLen`/`size` | `_Thread_local` |
| `syngbwt3.c`, `syng.c`, `impg_syng_helpers.c` | `pathCount` | `__atomic_*` builtins |

Two groups of symptoms:

1. **Counters** (`utils.c`, `hash.c`, `array.c`, `rskip.c`) are diagnostics only,
   so a lost update costs a wrong number in a report. They are still undefined
   behavior, and LLVM is free to miscompile the surrounding loop. The RNG in
   `rskip.c` was already documented as per-thread, so `_Thread_local` is the
   intended semantics.
2. **Correctness** (`rsWrite`, `pathCount`):
   - `rsWrite` frees and reallocates its `static I64 *iSym, *runLen` scratch
     buffers. Two `SyngIndex` saves running at once — the normal case for the
     `syng-local:<window>` partitioned pipeline — hit a double free or a
     use-after-free on the second writer.
   - `pathCount` is incremented while indexing and read by `syngBWTnext` /
     `syngBWTmatch` as a debug gate. `impg_syng_suppress_debug` wrote it during
     queries. All accesses now use `__atomic_load_n` / `__atomic_fetch_add` /
     `__atomic_store_n`.

Replacing the C statics with `_Thread_local` changes `hashStats()` /
`arrayReport()` to report only the calling thread's totals. Nothing in impg
reads those reports, so this only affects ad-hoc C-level debugging.

**Should be upstreamed.** Reproducer: build two syng indices concurrently in one
process (or run `impg partition` with `--gfa-engine syng-local:<window>`), or run
any multi-threaded syng query under ThreadSanitizer.

## Workflow for syncing upstream changes

```bash
cd vendor/syng
git fetch origin               # origin = pangenome/syng
git fetch upstream             # add upstream = richarddurbin/syng if needed
git checkout main
git merge --ff-only upstream/main
git push origin main
git checkout impg-integration
git rebase main                # may need to resolve conflicts in hash.c
git push --force-with-lease origin impg-integration
```

Then in impg, bump the submodule pointer and commit.
