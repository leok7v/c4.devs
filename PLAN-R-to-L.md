# PLAN-R-to-L.md — Switch parameter evaluation to right-to-left

## Status: DONE (42/45 tests passing, zero regressions vs HEAD)

Implemented. Summary of the final approach is in the **"Implementation
notes"** section at the bottom — the original plan below called for an
at-compile-time block-reversal, which turned out to be unsound (it silently
miscomputes PC-relative `JSR`/`JMP`/`BZ`/`BNZ` offsets inside argument
expressions like `add3(mul2(2,3), 4, 5)`). The working approach instead
adds a single `REVN` opcode that reverses the top N stack entries **at
runtime**, just before the call. The body of the plan is preserved as-is
for context; the "Implementation notes" section supersedes it.

Baseline results (captured 2026-04-09):
- HEAD (`efb1760`): 13/45 pass, 32 fail
- Working copy before fix: 0/45 pass
- After fix: 42/45 pass, 3 fail (all 3 are pre-existing varargs-dependent
  tests: `test/test_vararg.c`, `test/test_vsnprintf.c`, `test/test_sb.c`)

---

## Why this document exists

The working copy of `cx.c` is half-finished. A previous agent (gemini-cli) was
asked to switch argument evaluation order from L-to-R to R-to-L to prepare for
future varargs work. It took a **different** approach than requested: it kept
L-to-R source-order emission and instead bolted on a runtime "nargs" slot
pushed before every call, plus a 9-instruction LEA dance at every named-param
read to compute offsets against that slot. The formulas are off-by-one for
params and off-by-two for locals, so most tests that use either are broken.

This plan describes the **correct** fix: revert gemini's machinery, reverse
argument emission in the call site, and renumber params so classic
`loc - d[Val]` addressing yields R-to-L static offsets. Varargs is
**explicitly out of scope** for this pass — flipping the order is a
prerequisite and a standalone deliverable.

This file is self-contained so a follow-up agent (or a new session) can pick
up and finish if the current session runs out of budget. Every step lists
exact file paths, line ranges, before/after snippets, and verification.

---

## Current state (entering the fix)

- Branch: `main`
- `cx.c` is dirty (uncommitted working-copy changes from gemini).
- HEAD commit `efb1760` — "Add sb.h string builder and fix preprocessor line
  numbering". `cx.c` at HEAD has cosmetic damage in `expression()` (duplicate
  dead `else if (d[Class] == Fun)` branch, bad bracing), but it **builds and
  runs**. The working copy does not.
- Build target: `build/cx` (project convention per PLAN.md / AGENTS.md).
- Compile line: `cc -o build/cx cx.c` (or `cc -o build/cx cx.c -O3 -m64
  -std=c11 -Wall` for strict).
- Test runner: `./build/cx test/tests.c`

Quick probes that show the breakage:

```sh
cc -o build/cx cx.c

# Bug 1 (param off-by-one, user calls):
cat > /tmp/tre.c <<'EOF'
int three(int a, int b, int c) { return a*100 + b*10 + c; }
int main() { printf("%d\n", three(1,2,3)); return 0; }
EOF
./build/cx /tmp/tre.c
# Current output: 233        Expected: 123

# Bug 2 (local off-by-two):
cat > /tmp/loc.c <<'EOF'
int main() { int i; i = 42; printf("i=%d\n", i); return 0; }
EOF
./build/cx /tmp/loc.c
# Current output: i=444596224 (stack garbage)    Expected: i=42

# Bug 3 (run() hardcodes wrong nargs for main; masked by Bug 1):
./build/cx test/args.c    # prints "argc = 1" by coincidence
```

---

## Target architecture (what we want after the fix)

### Stack frame on function entry (R-to-L cdecl)

Caller emits, for `f(a0, a1, ..., a_{N-1})`:

```
    (evaluate a_{N-1}) PSH       <-- pushed FIRST
    ...
    (evaluate a1)      PSH
    (evaluate a0)      PSH       <-- pushed LAST
    JSR target
```

After callee's `ENT k`:

```
    bp[0]    = saved_bp
    bp[1]    = return address
    bp[2]    = a0         <-- first declared param, STATIC offset
    bp[3]    = a1
    ...
    bp[1+N]  = a_{N-1}    <-- last declared param
    bp[-1]   = local_0
    bp[-2]   = local_1
    ...
```

Key property: offset of a declared param depends **only** on its declaration
index, not on how many args the caller passed. Future varargs support can
then just read forward past `bp[1+N]` without any per-call metadata.

### `d[Val]` numbering convention (classic c4, preserved)

- During param parsing we still assign `id[Val] = i++;` (0, 1, ..., N-1 in
  source order), then after `)` we **reverse them in place**: param at index
  `k` gets `id[Val] = (N-1) - k`. This is the only new step on the parse side.
- After reversal, the last declared param has `d[Val] = 0`, the first has
  `d[Val] = N-1`.
- `loc = ++i; i = i + 8;` (reserve struct-temp), then locals increment `i`
  as today.
- Classic LEA offset `loc - d[Val]` then yields:
  - For first param (d[Val] = N-1): `loc - (N-1) = (N+1) - (N-1) = 2` → bp[2]. ✓
  - For last param (d[Val] = 0):    `loc - 0 = N+1` → bp[N+1]. ✓
  - For first local (d[Val] = N+10): `(N+1) - (N+10) = -9` → bp[-9]. ✓

So no change to the emit-side formula: we keep `*++e = LEA; *++e = loc -
d[Val];` inline everywhere. All of gemini's `emit_lea(d)` callsites revert to
that inline pattern.

### Argument emission (the one genuinely new piece)

The `while (tk != ')')` loop in `expression()` parses source L-to-R — we
can't avoid that, the lexer is single-pass. We need the **emitted code** for
the args to appear in reverse order in the instruction stream. Approach:

1. Record `e + 1` before each arg is emitted into a small `arg_starts[]`
   array (fixed 32 slots; fatal if exceeded — matches existing c4 limits).
2. After the loop closes on `)`, compute block boundaries and reverse the
   blocks in-place using a scratch buffer.

Pseudo-code (STYLE-compliant, flat, no nesting explosion):

```c
int64_t *arg_starts[32];
int64_t arg_count = 0;
arg_starts[0] = e + 1;
while (tk != ')') {
    expression(Assign);
    *++e = PSH;
    if (arg_count >= 31) { fatal("too many args"); }
    arg_count = arg_count + 1;
    arg_starts[arg_count] = e + 1;
}
/* reverse arg_count blocks in [arg_starts[0] .. e] */
if (arg_count > 1) {
    int64_t total = e + 1 - arg_starts[0];
    int64_t *scratch = malloc(total * sizeof(int64_t));
    memcpy(scratch, arg_starts[0], total * sizeof(int64_t));
    int64_t *dst = arg_starts[0];
    int64_t k = arg_count - 1;
    while (k >= 0) {
        int64_t off = arg_starts[k] - arg_starts[0];
        int64_t len = arg_starts[k + 1] - arg_starts[k];
        memcpy(dst, scratch + off, len * sizeof(int64_t));
        dst = dst + len;
        k = k - 1;
    }
    free(scratch);
}
```

Notes:
- `arg_starts[arg_count]` records the end of the last block so lengths are
  `arg_starts[k+1] - arg_starts[k]`.
- The blocks are self-contained — `expression()` evaluates into `a`, then
  PSH pushes and leaves `a` unchanged. No LEA references inside one arg
  block point at other arg blocks (all addressing is bp-relative).
- A single arg (no reversal needed) short-circuits at `arg_count <= 1`.
- Max arg count is capped at 31 slots to keep the stack array tiny; this
  is lavish for c4 code and matches existing limits elsewhere in cx.c
  (e.g. 1024 fwd_patches).

---

## Step-by-step fix

All line numbers refer to **current working-copy `cx.c`** (the broken state
we're fixing). If they drift due to earlier edits, anchor with the `grep`
patterns listed instead.

### Step 1 — Delete `cur_fn` global

**File:** `cx.c` — around line 73
**Find:** `int64_t *cur_fn;`
**Action:** Delete the line.

### Step 2 — Delete `emit_lea()` helper

**File:** `cx.c` — lines ~520-540
**Find:** the entire `void emit_lea(int64_t *d) { ... }` definition.
**Action:** Delete the whole function (between `void skip_comma...` block
and `void load()`). Leave one blank line between neighbors.

### Step 3 — Revert expression()'s struct-return-area prologue

**File:** `cx.c` — lines ~926-932
**Find:**
```c
if (d[Class] == Fun && d[Type] > INT64 && d[Type] < FNPTR) {
    if (!cur_fn) { fatal("struct return area address-of outside function"); }
    int64_t nparams = cur_fn[Extent];
    *++e = LEA; *++e = -1 - (struct_temp - nparams);
    *++e = PSH;
    ++t;
}
```
**Replace with:**
```c
if (d[Class] == Fun && d[Type] > INT64 && d[Type] < FNPTR) {
    *++e = LEA;
    *++e = loc - struct_temp;
    *++e = PSH;
    ++t;
}
```

### Step 4 — Revert expression()'s call site; reverse arg blocks

**File:** `cx.c` — lines ~933-976
**Find:** the block starting `while (tk != ')') {` and ending just before
`break;` at the close of the `case Id:` call path (look at the working copy
around `*++e = IMM; *++e = t; *++e = PSH;` — that's the gemini marker).

**Replace with (L-to-R parse, reversed emission, no nargs push):**

```c
int64_t *arg_starts[32];
int64_t arg_count = 0;
arg_starts[0] = e + 1;
while (tk != ')') {
    expression(Assign);
    *++e = PSH;
    if (arg_count >= 31) { fatal("too many args in call"); }
    arg_count = arg_count + 1;
    arg_starts[arg_count] = e + 1;
    skip_comma();
}
if (arg_count > 1) {
    int64_t total = e + 1 - arg_starts[0];
    int64_t *scratch = malloc(total * sizeof(int64_t));
    memcpy(scratch, arg_starts[0], total * sizeof(int64_t));
    int64_t *dst = arg_starts[0];
    int64_t k = arg_count - 1;
    while (k >= 0) {
        int64_t off = arg_starts[k] - arg_starts[0];
        int64_t len = arg_starts[k + 1] - arg_starts[k];
        memcpy(dst, scratch + off, len * sizeof(int64_t));
        dst = dst + len;
        k = k - 1;
    }
    free(scratch);
}
t = t + arg_count;
next();
if (d[Class] == Sys) {
    *++e = d[Val];
} else if (d[Class] == Fun) {
    *++e = JSR;
    if (d[Val]) {
        int64_t target = d[Val];
        int64_t offset = (char *)target - (char *)(e + 1);
        *++e = offset;
    } else {
        *++e = 0;
        fwd_patches[fwd_sp] = (int64_t)d;
        fwd_patches[fwd_sp + 1] = (int64_t)e;
        fwd_sp = fwd_sp + 2;
    }
} else if (d[Type] == FNPTR) {
    if (d[Class] == Loc) {
        *++e = LEA;
        *++e = loc - d[Val];
    } else if (d[Class] == Glo) {
        *++e = IMMD;
        *++e = d[Val] - (int64_t)data_base;
        *++e = OFFSET;
    }
    *++e = LI;
    *++e = JSRI;
} else {
    fatal("bad function call");
}
if (t) { *++e = ADJ; *++e = t; }
ty = d[Type];
if (ty == FNPTR) { ty = INT64; }
```

Key points:
- `t` starts at `0` or `1` (1 if struct-return prologue ran). We combine that
  with `arg_count` into the final `ADJ` count.
- The struct-return prologue's PSH is emitted **before** `arg_starts[0]`, so
  it is not part of the reversed range (correct — the struct return dest
  must stay deepest on the stack per the ABI already established).
- No `IMM t; PSH` nargs slot. No `ADJ t+1` bump. Classic `ADJ t`.

### Step 5 — Revert the outer-chain branches of `case Id:`

Still in `expression()`, the code after the call path (handling bare
identifier references — function-as-value, constant, local/global variable)
was made messy by gemini. Replace with the clean pre-gemini pattern.

**File:** `cx.c` — lines ~977-1001

**Find and replace the whole outer `} else if (d[Class] == Fun) { ... }
else if (d[Class] == Num) { ... } else { ... }` chain with:**

```c
} else if (d[Class] == Fun) {
    *++e = IMMC;
    *++e = (char *)d[Val] - (char *)code_base;
    *++e = COFFSET;
    ty = FNPTR;
} else if (d[Class] == Num) {
    *++e = IMM;
    *++e = d[Val];
    ty = INT64;
} else {
    if (d[Class] == Loc) {
        *++e = LEA;
        *++e = loc - d[Val];
    } else if (d[Class] == Glo) {
        *++e = IMMD;
        *++e = d[Val] - (int64_t)data_base;
        *++e = OFFSET;
    } else {
        fatal("undefined variable");
    }
    ty = d[Type];
    if (d[Extent] == -1) {
        *++e = LI;
    } else if (!d[Extent]) {
        load();
    }
}
break;
```

### Step 6 — Revert the 9 statement() callsites that use `emit_lea(d|id|pid)`

**File:** `cx.c` — 9 occurrences at approx. these lines (verify via grep
`emit_lea(` after each edit):

- 1406, 1416 (statement() local-decl assign, struct + scalar)
- 1613, 1623 (statement() nested decl assign, struct + scalar)
- 1856 (statement() array init loop)
- 1904, 1914 (statement() plain assign, struct + scalar)
- 2974 (struct-param copy in function prologue — note this one uses `pid`)
- 3089 (function-body array init)
- 3137, 3147 (function-body assign, struct + scalar)

**Each one replaces:**
```c
emit_lea(d);             /* or emit_lea(id); or emit_lea(pid); */
```
**with:**
```c
*++e = LEA;
*++e = loc - d[Val];     /* or id[Val] / pid[Val] to match */
```

One special case at around line 2973 (struct param copy in prologue) also
had the gemini line:
```c
*++e = LEA; *++e = -1 - (local_slot - nparams); *++e = PSH;
```
**Replace with:**
```c
*++e = LEA;
*++e = loc - local_slot;
*++e = PSH;
```
and delete the line `int64_t nparams = cur_fn[Extent];` above it.

### Step 7 — Revert function-definition prologue: remove `cur_fn = d;` and `d[Extent] = i;`; reverse param Val assignments

**File:** `cx.c` — around lines 2849-2916

**Remove line 2851:**
```c
cur_fn = d;
```

**Remove line 2916:**
```c
d[Extent] = i; // Store nparams
```

**Immediately after the param-parsing `while (tk != ')') { ... }` loop
closes, and before the `next()` that consumes `)` on line 2917**, insert
a param-Val reversal pass:

```c
/* Reverse Val for params so classic loc-d[Val] yields R-to-L offsets.
 * Walk sym table, find entries whose Class==Loc with Val in [0..i),
 * and flip: id[Val] = (i - 1) - id[Val].  `i` is the nparams counter. */
{
    int64_t nparams = i;
    int64_t *pid = sym;
    while (pid[Tk]) {
        if (pid[Class] == Loc && pid[Val] >= 0 && pid[Val] < nparams) {
            pid[Val] = (nparams - 1) - pid[Val];
        }
        pid = pid + Idsz;
    }
}
```

**Why this is correct:**
- During param parsing `id[Val] = i++` assigns 0, 1, ..., N-1 in source
  order. The reversal makes the first source param hold `N-1` and the last
  hold `0`.
- Locals haven't been declared yet at this point, so no collisions.
- `loc = ++i; i = i + 8;` runs **after** this block (unchanged — nothing
  else needs to move).

### Step 8 — Revert `run()` to not push an `nargs` slot

**File:** `cx.c` — lines ~3308-3314

**Find:**
```c
*--sp = (int64_t)t;
*--sp = argc;
*--sp = (int64_t)argv;
*--sp = 3; // nargs
*--sp = -1; // return address marker
```

**Replace with (drop the nargs line):**
```c
*--sp = (int64_t)t;
*--sp = argc;
*--sp = (int64_t)argv;
*--sp = -1; // return address marker
```

Also revisit the comment on `bp = sp;` — it says "main's ENT will push this,
so main's bp+1 will point here." Keep the comment, it's still accurate.

### Step 9 — Verify `le = code_base = e = code_pool;`

**File:** `cx.c` — around line 2594

The init `le = code_base = e = code_pool;` that gemini added is **good** and
should stay. `le` is the disassembler cursor (`cx.c:42`) and was previously
uninitialized. Leave it.

---

## Verification

### Build

```sh
mkdir -p build
cc -o build/cx cx.c
```

Expect zero warnings, zero errors.

### Smoke tests (manual, prove the bugs are gone)

```sh
cat > /tmp/tre.c <<'EOF'
int three(int a, int b, int c) { return a*100 + b*10 + c; }
int four(int a, int b, int c, int d) { return a*1000+b*100+c*10+d; }
int main() {
    printf("three=%d\n", three(1,2,3));
    printf("four=%d\n", four(1,2,3,4));
    return 0;
}
EOF
./build/cx /tmp/tre.c
# Expect: three=123 / four=1234

cat > /tmp/loc.c <<'EOF'
int main() {
    int i;
    i = 42;
    printf("i=%d\n", i);
    int j = 7;
    printf("j=%d\n", j);
    return 0;
}
EOF
./build/cx /tmp/loc.c
# Expect: i=42 / j=7

./build/cx test/args.c               # Expect: argc = 1 (unchanged)
./build/cx test/loops.c               # Expect: well-formed output, no giant numbers
./build/cx test/expressions.c         # Expect: 75/75 passed
./build/cx test/fnptr.c               # Expect: non-empty, no crash
```

### Full test suite

```sh
./build/cx test/tests.c
```

There are 46 test files in `test/`. Pre-existing failures unrelated to this
change may exist — compare before/after to isolate. Minimum bar: tests that
passed on `git stash` (HEAD unmodified) must still pass after the fix.

**Baseline capture procedure** (in case context runs out and a follow-up
session needs to compare):

```sh
git stash                              # set aside fix
cc -o build/cx-head cx.c               # build HEAD
./build/cx-head test/tests.c > /tmp/baseline.txt 2>&1 || true
git stash pop                          # restore fix
cc -o build/cx cx.c
./build/cx test/tests.c > /tmp/after.txt 2>&1 || true
diff /tmp/baseline.txt /tmp/after.txt  # new failures = regressions
```

### Self-compilation

Not required for this change (c4-style self-compile is a separate concern
and cx may not currently self-compile regardless), but worth a quick check:

```sh
./build/cx cx.c test/expressions.c 2>&1 | tail -5
```

If it fails, note the error and continue — do not let self-compile block
landing this fix.

---

## What this fix does NOT do (scope guard)

- **No varargs.** `...` in parameter lists, `va_start`/`va_arg`/`va_end`,
  any caller-side nargs slot — all out of scope. That lands in a follow-up
  once the R-to-L base is in and green.
- **No new opcodes.** The enum additions (IMMD, IMMC, OFFSET, COFFSET, plus
  the 30+ new intrinsic opcodes) landed in commit `02d7091` and are kept.
- **No changes to ENT/LEV/JSR mechanics.** Stack layout is classic c4.
- **No refactor of statement().** Only the 9 `emit_lea(...)` call sites
  get reverted; surrounding code is untouched.
- **No printf format churn**, no `STYLE.md` tidy-ups on unrelated code.

---

## Rollback

If the fix misbehaves and we need to bail out:

```sh
git checkout -- cx.c     # back to HEAD (efb1760) — broken-but-known
```

HEAD's `cx.c` still builds and runs (despite the messy `expression()`
structure), so it is a safe fallback point.

If HEAD itself turns out to be broken in some new way, use commit `02d7091`
(the PIC/PID refactor) or `428a53e` (pre-PIC) — both predate the damage.

---

## Checklist

- [x] Step 1: `cur_fn` global deleted
- [x] Step 2: `emit_lea()` deleted
- [x] Step 3: struct-return prologue reverted in `expression()`
- [x] Step 4: call site rewritten — runtime `REVN` instead of compile-time
      block reversal (see Implementation notes)
- [x] Step 5: outer `case Id:` chain reverted
- [x] Step 6: all 9 `emit_lea(...)` call sites reverted to inline LEA
- [x] Step 7: `cur_fn = d` and `d[Extent] = i` removed; param Val reversal
      inserted after param parse
- [x] Step 8: `run()` no longer pushes nargs slot; push order flipped to
      match R-to-L (argv first, argc second) for main
- [x] Step 9: `le = code_base = e = code_pool;` retained
- [x] Build is clean
- [x] Smoke tests pass (`three`, `four`, local `i`, local `j`,
      `add3(mul2(2,3), 4, 5)`)
- [x] Full test suite: 42/45 pass, zero regressions vs HEAD's 13/45

---

## Implementation notes (actual fix, supersedes "Step 4" above)

### Why block-reversal was wrong

The original plan called for shuffling already-emitted bytecode blocks in
place after the call's arg list closes. That approach is broken: `JSR`,
`JMP`, `BZ`, `BNZ` all encode PC-relative offsets measured at emission time
against the address of their operand slot. Moving the block to a new
location in the code stream silently invalidates those offsets — and the
moved block may also contain forward-patch records (`fwd_patches`) whose
stored patch addresses no longer match.

The failing probe was `int z = add3(mul2(2,3), 4, 5);`. The inner
`mul2(2, 3)` emits a JSR whose offset anchors to its own slot position.
Once the `mul2(...)` block got moved by the reversal, the JSR jumped into
garbage, the VM walked off a cliff, and main returned a random value with
no output.

Fixing this in-place would require walking every moved block and patching
every PC-relative opcode it contains plus updating every `fwd_patches`
entry that points into a moved range. Doable but complex, fragile, and
opcode-audit-heavy.

### What we did instead: `REVN` opcode

Emit args in source order (no relocation), then emit a single new opcode
`REVN N` that reverses the top N stack entries at runtime, immediately
before the `JSR` or `JSRI`. The callee then sees R-to-L cdecl layout as
designed.

- **New opcode**: `REVN` appended to the opcode enum at `cx.c:115-117`.
  Disasm string tables updated at `cx.c:166` and `cx.c:3353`. Disasm's
  operand-printing predicate widened from `opc <= ADJ` to
  `opc <= ADJ || opc == REVN` at two sites.
- **VM impl** (`cx.c:3416-3431`): reads `n = *pc++`, swaps `sp[0]..sp[n-1]`
  pairwise. ~15 lines.
- **Call-site emission** (`cx.c:910-943`): drop the block-reversal code.
  After the `while (tk != ')')` push loop, emit
  `if (arg_count > 1 && d[Class] != Sys) { *++e = REVN; *++e = arg_count; }`.
  Sys intrinsics still read args with L-to-R indexing in the VM, so they
  bypass `REVN`.

### Why the struct-return slot is still safe

The struct-return-area pointer is pushed **before** the `arg_count` loop
starts and is not counted in `arg_count`. `REVN` reverses only the top
`arg_count` slots, leaving the struct-return slot untouched at its deepest
position. ABI preserved.

### Stack layout after `REVN` for a call `f(a0, a1, ..., a_{N-1})`

```
    (L-to-R push emitted as normal)
    a0  PSH     -- sp[N-1] after all pushes
    a1  PSH     -- sp[N-2]
    ...
    a_{N-1} PSH -- sp[0]
    REVN N      -- swaps sp[0..N-1] → sp[0]=a0, sp[N-1]=a_{N-1}
    JSR f
    (callee ENT)
    bp[2] = a0  (static, independent of caller arity)
    bp[3] = a1
    ...
    bp[1+N] = a_{N-1}
```

This matches the "target architecture" diagram in the plan header.

### `run()` main invocation

`main(argc, argv)` is set up in `cx.c:3325` by manually pushing args onto
the stack (no REVN opcode runs because main is entered directly, not via
JSR). To match R-to-L convention, the push order is flipped: **argv
pushed first (deepest), argc pushed second (closest to bp)**. That puts
`argc` at `bp[2]` and `argv` at `bp[3]` inside main, which matches what
emit-side code expects for the first and second declared params.

Verified with `./build/cx test/args.c foo bar baz` → prints
`argc = 4` and all 4 `argv[i]` entries correctly.

### Performance note

`REVN` costs O(N) per call. For the typical N ≤ 4 that's 1–2 swaps. That
is far cheaper than gemini-cli's rejected approach (9 bytecode instructions
at **every** named-param access for the life of the program) and is paid
only at the call site.

### What remains (out of scope for this plan)

- `...` parameter syntax, `va_list` / `va_start` / `va_arg` / `va_end`.
- These will land as a follow-up and can now rely on the R-to-L layout.
- The 3 failing tests (`test_vararg.c`, `test_vsnprintf.c`, `test_sb.c`)
  all exercise varargs and stay failing until that follow-up.
