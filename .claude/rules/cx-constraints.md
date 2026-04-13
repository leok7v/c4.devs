---
paths:
  - "cx.c"
---
# cx.c self-hosting constraints

cx self-hosts the full 45-test suite end-to-end. These rules encode
pitfalls that have already bitten us. Breaking any of them shows up
as a test that passes natively but fails under `build/cx cx.c ...`.
The commit history and `MEMORY.md` entries spell out the incident
for each one.

## Don't stack case labels

cx's C4-style `switch` re-checks the scrutinee at every `case` label,
so `case A: case B:` runs the body only for `B` — `A` falls into
`B`'s check, fails, and skips the body. Split stacked cases into
separate cases, sharing a helper function if the body is non-trivial.

Example: `case Inc:` / `case Dec:` in `expression()` now call a
`preinc()` helper instead of falling through.

## Route switch-case `break` through `brk_patches`

A `break;` nested inside a braced `{ ... }` in a switch case goes
through `statement()`'s Break handler, which writes to
`brk_patches` — the same stack used by `while_stmt` / `for_stmt`.
`switch_stmt` saves and restores `brk_sp` around its body so those
breaks land at the end of the switch, not the enclosing loop.

Don't re-introduce a local `break_stack` in `switch_stmt`.

## Assign `id[Val]` unconditionally in the function prologue

For non-array locals that shadow a non-zero global, the shadowed
global's value has already been saved to `id[HVal]`, and the local's
slot must still be written. Use `id[Val] = i;` unconditionally — not
`if (id[Val] == 0) id[Val] = i;`. The array branch sets it to the
same `i` inside, so the unconditional assign is safe everywhere.

## `code_pool` must fit cx.c's own bytecode

It's currently 512 KB (65536 × int64_t). If cx.c grows significantly,
watch for silent memory corruption just past the end of `code_pool` —
ASAN catches it; unluckily, a plain segfault also reveals it. Bump
the pool if needed.

## Self-hosted intrinsics forward, they don't stub

Filesystem intrinsics that need host types (`struct stat`, `DIR*`,
`struct tm`) live in the native `#ifndef __cx__` branch. The `#else`
branch calls the same intrinsic by name — cx resolves it through
the `intrinsic()` registrations and the outer native VM handles it
for real. Don't re-stub these to `a = -1; break;`.

## Verify after touching cx.c

    cc -Wall -Wpedantic -o build/cx cx.c && build/cx cx.c tests/all.c

Expect `SUMMARY: 45/45 tests passed.` Anything less is a regression.
