---
paths:
  - "toys.c"
  - "test/toys_*.c"
---
# toys.c conventions

## Output goes through write(), not printf

`printf` is buffered in cx. `write()` bypasses the buffer. Mixing
them in a single command produces out-of-order output that looks
correct in isolation and wrong the moment the command is piped.

Use `write(1, ...)` for all command stdout. Format numbers manually
(`cx_itoa`, `cx_itopad`). Reserve `printf` for test harnesses, not
for command implementations.

## Errors go to stderr via `cx_err`

    void cx_err(char *msg) { write(2, msg, strlen(msg)); }

Every command diagnostic uses `cx_err`. Never `printf` for errors —
buffering will swallow the message when the command is piped into
another process.

## Every command needs a test

A new command in `toys.c` ships with at least one entry in the
appropriate `test/toys_<stage>.c` file. The test runner
(`test/tests.c`) picks up test files automatically; there is no
manual registration step.

## Platform constants belong in `#if`-guarded blocks

`O_CREAT`, `O_TRUNC`, `S_IF*` masks differ between macOS and Linux.
Keep the platform blocks at the top of `toys.c`; do not spread
platform checks through command bodies.

## Verify after touching toys.c

    cc -Wall -Wpedantic -o build/cx cx.c && build/cx cx.c test/tests.c

Both native and self-hosted paths must pass. A toys.c change that
regresses the self-host path usually means the toy code is
exercising a cx language feature that's broken — not that toys.c
itself is wrong.
