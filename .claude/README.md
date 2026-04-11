# .claude/ — the harness for this project

Committed control center for how Claude Code behaves in the cx repo.
Layout follows the "Anatomy of the .claude/ folder" pattern (Chawla,
Mar 2026) plus the file-backed state discipline from "Natural-Language
Agent Harnesses" (Pan et al., arXiv 2603.25723, Mar 2026).

    .claude/
    ├── settings.json              # permissions (committed)
    ├── settings.local.json        # personal overrides (gitignored)
    ├── README.md                  # this file
    ├── commands/
    │   └── task.md                # /task → spawn task-runner
    ├── rules/
    │   ├── project-context.md     # always loaded — repo overview
    │   ├── cx-constraints.md      # paths: cx.c — self-host rules
    │   └── toys-conventions.md    # paths: toys.c, test/toys_*.c
    ├── skills/
    │   ├── build-and-test/
    │   │   └── SKILL.md           # the 45/45 gate
    │   ├── self-host-check/
    │   │   └── SKILL.md           # native vs self-host diff
    │   └── log-progress/
    │       └── SKILL.md           # append to MEMORY.md
    └── agents/
        └── task-runner.md         # TASK.md → RESPONSE.md subagent

## What goes where

**`rules/`** — modular instruction files loaded into every session
alongside the root AGENTS.md. Rules with a YAML `paths:` frontmatter
only activate when Claude is editing matching files. Rules without
`paths:` load unconditionally.

**`skills/`** — reusable workflows Claude invokes on its own when a
task matches the skill's description. Each skill is a folder with a
`SKILL.md` plus any supporting files the skill wants to ship alongside
itself (example templates, detailed guides, etc.).

**`commands/`** — manual slash commands. `/task` spawns the
task-runner subagent. Custom commands and skills now share the same
invocation surface; commands are the "user triggers it explicitly"
flavor, skills are the "Claude notices it should" flavor.

**`agents/`** — subagent personas. Each runs in its own isolated
context window with its own tool access. `task-runner` is the
file-backed TASK.md/RESPONSE.md runner — see below.

## The TASK.md / RESPONSE.md pattern

The main harness pattern in this repo is the file-backed task runner:

    TASK.md  (input, immutable)  →  task-runner agent  →  RESPONSE.md (output)

1. User writes `./TASK.md` at the project root with a job spec:
   goal, constraints, acceptance criteria, references.
2. User invokes `/task` (or Claude notices the file and offers to
   run it).
3. The `task-runner` subagent spawns in an isolated context. It
   reads TASK.md, plans, executes, verifies, and writes
   `./RESPONSE.md` with `## Summary` / `## Changes` / `## Evidence`
   / `## Open questions` / `## Status`.
4. The main agent reads RESPONSE.md and relays a short summary
   back to the user.

Why this exists: long-horizon work (multi-file refactors, self-host
debugging) pollutes the main session's context with exploration
noise. File-backed I/O lets the subagent absorb that and return a
single compressed artifact that survives restarts.

Both `TASK.md` and `RESPONSE.md` are gitignored — they're per-session
artifacts, not source. Don't commit them.

## The one verification gate

Anything that touches `cx.c`, `toys.c`, or `test/*.c` is only done
when this passes with 45/45 on both paths:

    cc -Wall -Wpedantic -o build/cx cx.c && build/cx cx.c test/tests.c

The `build-and-test` skill is the canonical way to run it. The test
runner (`test/tests.c`) runs every test twice — natively and through
self-hosted cx — so both paths are covered in one command.

## Relationship to the root docs

`.claude/` is a dispatch layer. The substance lives in:

- `AGENTS.md` — compiler internals, self-host constraints, calling
  convention
- `PLAN.md` — staged roadmap and milestone checklist
- `STYLE.md` — coding style
- `MEMORY.md` — timestamped progress log

If a rule feels redundant with one of those files, point at the
root doc instead of duplicating content. The goal is one source of
truth per concern, with `.claude/` wiring behavior on top.
