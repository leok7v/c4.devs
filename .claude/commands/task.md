---
description: Spawn the task-runner subagent on ./TASK.md, relay ./RESPONSE.md back
argument-hint: (optional — reads ./TASK.md directly)
---

# /task

Run the file-backed **task-runner** subagent against `./TASK.md`.

## What this does

1. Confirms `./TASK.md` exists at the project root. If not, prints a
   short usage hint and stops — don't invent a task.
2. Spawns the `task-runner` agent (see `.claude/agents/task-runner.md`)
   with instructions to read `./TASK.md`, do the work in its own
   isolated context, and write `./RESPONSE.md`.
3. When the agent finishes, read `./RESPONSE.md` and relay a 2–3
   sentence summary to the user: the `## Status` line, a one-line
   headline from `## Summary`, and any `## Open questions` that need
   their attention. Don't dump the full file — point at it.

## Why

Long-horizon work (multi-file refactors, bug hunts, stage
implementations) pollutes the main conversation context with messy
exploration. The task-runner absorbs that noise and hands back a
single compressed artifact. The `TASK.md` / `RESPONSE.md` pair is
durable state — if the session restarts, RESPONSE.md is still there.

## Typical flow

    # 1. User drops a spec file
    $ cat > TASK.md <<'EOF'
    Bump cx code_pool from 64K to 128K slots, verify 45/45.
    EOF

    # 2. User invokes
    /task

    # 3. task-runner reads TASK.md, makes the edit, runs the gate,
    #    writes RESPONSE.md.

    # 4. Main agent summarizes: "Status: done. code_pool bumped to
    #    128K slots in cx.c:61. 45/45 passed on both paths. No open
    #    questions. See RESPONSE.md."

## Notes

- `TASK.md` and `RESPONSE.md` are gitignored per-session artifacts.
  Don't commit them.
- If `RESPONSE.md` comes back with `Status: blocked` or `partial`,
  the main agent should read the full file and decide next steps
  with the user — do not auto-retry.
