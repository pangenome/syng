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
