# Project context

This repo is **cx** — a minimalist self-compiling C compiler (~3800
lines) plus its toy Unix command suite (`toys.c`). The canonical docs
at the project root are authoritative:

- `PLAN.md` — staged roadmap and milestone checklist
- `AGENTS.md` — compiler internals, calling convention, self-host
  constraints, common pitfalls
- `STYLE.md` — coding style for cx.c and toys.c
- `MEMORY.md` — timestamped progress log (newest on top); append an
  entry whenever a milestone lands

Read those before proposing architectural changes. They are the
project's single source of truth; everything under `.claude/` is
just a thin dispatch layer on top.

## The one command that matters

Any change to `cx.c`, `toys.c`, or files under `test/` is only done
when this passes with `SUMMARY: 45/45 tests passed.`:

    cc -Wall -Wpedantic -o build/cx cx.c && build/cx cx.c test/tests.c

The test runner (`test/tests.c`) now runs every test twice — once
natively (`build/cx <file>`) and once through self-hosted cx
(`build/cx cx.c <file>`). Both paths must return clean.

If a change compiles but the self-host path regresses, the fix is
almost always in cx.c itself, not in the failing test — check
`.claude/rules/cx-constraints.md` first.

## Layout at a glance

    cx.c         # the compiler + VM
    toys.c       # the busybox-style command binary
    test/        # cx language tests + toys_*.c command tests
    test/tests.c # the dual-path test runner
    build/       # compiled artifacts (gitignored)
    .claude/     # harness config (this folder, committed)
