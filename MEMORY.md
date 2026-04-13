# MEMORY.md — progress log

Timestamped notes on work completed against PLAN.md. Newest entries on top.
Each entry links back to the commit(s) that implemented it.

---

## 2026-04-12 — Stages 7-9 completed: vi, shell, regex, full sed, ps, ls -lah

Commits: `6f02dca`..`230d5c4` (8 commits)

Major deliverables in a single session:

1. **vi editor** (~650 lines) — line-array based with normal/insert/ex modes,
   alternate screen, /search, SIGWINCH resize, horizontal scroll.

2. **Stage 8: cx as embedded interpreter** — `cx` shell builtin finds the cx
   binary and runs .c files; cx.c skips `#!` shebang lines so `chmod +x file.c`
   works; shell prompt shows `basename_of_cwd$ `.

3. **Regex engine** (~180 lines) — adapted from tiny-regex-c (public domain).
   Supports `.^$*+?[abc][^abc][a-z]\s\S\w\W\d\D`. Wired into both grep and sed.

4. **Full sed** — addresses (line number, `$`, `/regex/`), ranges (`N,M`),
   commands (`s///[g]`, `d`, `p`, `q`), `-n` suppress, `-e` multi-expression.
   Replaces the old s///-only sed.

5. **ps** — reads `/proc` on Linux, falls back to `system("ps")` on macOS.

6. **ls -lah** — permissions string (`drwxr-xr-x`), nlink, human-readable sizes.

7. **CI fix** — replaced raw `write()`/`read()` with `cx_write()` in vi and
   readline code to satisfy gcc `-Wunused-result`. CI green.

All 45/45 tests pass on both native and self-hosted paths.

---

## 2026-04-11 — cx self-hosts cleanly, 45/45 tests pass on both paths

Commit: `76d5663` — cx.c: fix self-hosting bugs so every test passes under cx-on-cx

`test/tests.c` now runs every test twice — once natively, once through
self-hosted cx (`build/cx cx.c <test>`) — and all 45 pass on both paths.
Getting there required closing five bugs that had been lurking in the
self-hosting path:

1. **`code_pool` too small.** 256 KB wasn't enough for current cx.c bytecode.
   Bumped to 512 KB. Only surfaced under ASAN or when the overflowing write
   happened to hit an unmapped page.

2. **Stacked `case Inc: case Dec:` in `expression()`.** cx's C4-style switch
   re-checks the scrutinee at every case label, so stacked cases silently
   drop through to the next comparison. `++i` matched the first check, fell
   into the second check (for `Dec`), failed, and skipped the body. Split
   into two cases calling a new `preinc()` helper.

3. **Shadowing a non-zero global left the local pointing at the global's
   data address.** The function prologue set `id[Val] = i` only when it was
   zero; `id[HVal] = id[Val]` had already saved the global's value into the
   shadowed slot, so the condition was false and the local kept the global's
   address as its "stack slot". Now assigns unconditionally (the array path
   already sets it to the same `i`).

4. **`break` inside a braced compound statement in a switch case escaped to
   the enclosing loop.** `switch_stmt` used its own local `break_stack`, but
   any `break` nested inside `{ ... }` in a case body went through
   `statement()`'s Break handler, which writes to `brk_patches` (the
   enclosing while/for's break stack). This is exactly the pattern in
   `run()`'s `case REVN: { ... break; }` — the `break` after `REVN` was
   jumping out of the VM loop, silently exiting the nested run() after one
   instruction. Every self-hosted multi-arg user function call returned
   garbage as a result. Fixed by unifying `switch_stmt` on `brk_patches`
   with `saved_brk_sp`, matching `while_stmt` / `for_stmt`.

5. **Self-hosted filesystem intrinsics were stubbed.** `I_STAT` / `I_OPDIR` /
   `I_RDDIR` / `I_CLDIR` / `I_LTIME` were guarded by `#ifndef __cx__` because
   cx can't parse `struct stat` / `DIR*` / `struct tm`. The `#else` branch
   returned -1 / 0. Replaced it with recursive intrinsic calls: the inner
   case body just calls `stat(...)` / `opendir(...)` / etc. by name — cx
   resolves these through the `intrinsic()` registrations, emits the
   matching Sys opcode, and the outer native VM handles it for real.

Side effect for AGENTS.md / future work: the classic C4 caveat "don't stack
case labels" now applies to cx self-hosting too. Anywhere cx.c stacks cases,
the body runs only for the *last* label in the stack. `case Inc: case Dec:`
was the only stacked pair; scan for new ones before landing future changes.

---
