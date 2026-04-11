# C4 Development Guide

## What is c4

c4 is a minimal self-compiling C compiler by Robert Swierczek. It compiles a subset of C
to bytecode and interprets it in a stack-based VM. The entire compiler fits in ~800 lines of C.
It can compile its own source: `./build/c4 c4.c test/struct_simple.c`.

---

## Architecture

### Type System (PTR=256 encoding)

| Type      | Encoding | Check                      |
|-----------|----------|----------------------------|
| CHAR      | 0        | `ty == CHAR`               |
| INT32     | 1        | `ty == INT32`              |
| INT64     | 2        | `ty == INT64`              |
| Structs   | 3..255   | `ty > INT64 && ty < PTR`   |
| CHAR*     | 256      | `ty >= PTR`                |
| INT32*    | 257      |                            |
| Struct A* | 259      |                            |
| CHAR**    | 512      |                            |

- Pointers: `ty >= PTR`. Strip one level: `ty - PTR`.
- Struct values: `ty > INT64 && ty < PTR`. Keep address in accumulator; no load.
- Struct lookup: `struct_syms[ty - INT64 - 1]` maps sequential ID to symbol table entry.

### Key Globals

- `e` — code emission pointer; `*++e = opcode` emits instructions
- `id` — current identifier (pointer into `sym_pool`, array of `Idsz=13` int64_t fields)
- `sym` — symbol table base
- `struct_syms` — array mapping struct IDs (0-252) to symbol entries
- `num_structs` — number of defined structs (max 253)
- `loc` — local variable frame offset set at function entry
- `i` — multipurpose counter / current local slot index
- `scope_sp` — scope stack pointer for block-scoped variable save/restore
- `brk_sp`, `cnt_sp` — break/continue patch stack pointers

### Symbol Table Fields (Idsz = 13)

`Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Utyedef, Extent, Sline, Idsz`

- `Class` — `Fun`, `Sys`, `Glo`, `Loc`, or 0 (unbound)
- `Extent` — array size (0 for scalars). Arrays skip load in Id handler (address IS the value).
- `Sline` — for structs: linked list head of members `m[0]=hash, m[1]=type, m[2]=offset, m[3]=next`
- `Val` — for structs: total padded size in bytes; for locals/globals: address/offset

### Key Functions

- `next()` — lexer/tokenizer; sets `tk` and `id`
- `memacc()` — shared member access handler for `.` and `->` operators
- `expr(lev)` — expression parser with precedence climbing
- `stmt()` — statement parser
- `main()` — top-level declaration parser + VM interpreter

### Implemented Language Features

- Types: `char`, `int` (=int64_t), `int32_t`, `int64_t`, pointers, structs, arrays
- Control flow: `if`/`else`, `while`, `for`, `do`/`while`, `switch`/`case`/`default`
- Statements: `break`, `continue`, `return`
- Expressions: full precedence climbing, `sizeof`, casts, `++`/`--`, `->`, `.`, `[]`
- Scoped declarations: `{ int x = 0; }` and `for (int i = 0; ...)`
- Block-level variable declarations with initializers

### VM Opcodes

```
LEA, IMM, JMP, JSR, BZ, BNZ, ENT, ADJ, LEV
LI, LC, LI32, SI, SC, SI32, PSH
OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD
OPEN, READ, CLOS, PRTF, MALC, FREE, MSET, MCMP, EXIT, WRIT, SYST
POPN, PCLS, FRED, MCPY, MMOV, SCPY, SCMP, SLEN, SCAT, SNCM, DUP
```

---

## Build & Test

```sh
# Build
cc -o build/c4 c4.c -O3 -m64 -std=c11 -Wall

# Run individual tests
./build/c4 test/struct_simple.c
./build/c4 test/grok.c
./build/c4 test/loops.c

# Self-compilation test (c4 compiles itself, then runs a test)
timeout 10 ./build/c4 c4.c test/struct_simple.c

# Cross-check with system compiler
cc test/grok.c -o ./tmp/grok && ./tmp/grok

### Compiler Notes

- Use `cc` instead of `gcc` or `clang` — on macOS, `cc` is clang
- This ensures consistency across platforms and matches CI
- All examples in this guide use `cc`

# Full test suite
./build/c4 test/test.c
```

### Temporary files

Use `./tmp/` (gitignored) for all temporary test files and debugging output — **not** `/tmp/`.
This avoids VSCode permission prompts and keeps scratch work in the project directory.

```sh
cc test/grok.c -o ./tmp/grok && ./tmp/grok
cat > ./tmp/mytest.c << 'EOF'
...
EOF
./build/c4 ./tmp/mytest.c
```

### Test Files

All test files include `<stdio.h>` so they compile with both c4 and cc/gcc/clang.

- `test/grok.c` — padding, sizeof(struct), cast, struct ptr arith
- `test/arrays.c` — all array types including struct arrays
- `test/loops.c` — for/while/do-while with break/continue, scoped for declarations
- `test/struct_ptr_arith.c` — pointer arithmetic, pre/post increment
- `test/struct_simple.c`, `test/struct_nested.c` — basic struct operations
- `test/int32_64.c` — int32_t/int64_t operations
- `test/io.c` — file I/O (open/read/write)
- `test/scope.c` — block-scoped declarations, init-in-declaration, (void) params
- `test/test.c` — test runner (discovers and runs all tests)

### CI

`.github/workflows/ci.yml` — builds on ubuntu-latest with `clang -w` (suppresses Linux
int64_t format warnings), runs struct tests, io, self-compilation.

---

## Self-Compilation Constraints

`cx.c` must be valid cx input. cx supports `for`/`do`/`break`/`continue`/
`switch`, mid-block declarations, typedefs, unions, function pointers,
variadics, and a cpp-style preprocessor, so most modern C is fair game.
As of 2026-04-11 cx self-hosts the full 45-test suite end-to-end
(`build/cx cx.c test/tests.c`) — see MEMORY.md for the fixes that got us
there. Still, a few sharp edges remain:

- **No forward references to globals** — declare before use.
- **Don't stack case labels** — cx's C4-style switch re-checks the
  scrutinee at every `case`, so `case A: case B:` runs the body only for
  `B`. Split into two cases (optionally sharing a helper function).
- **Don't rely on `break;` inside a braced `{ ... }` in a switch case
  reaching the switch** — that path now works, but new control-flow
  additions should keep this pattern in mind; when in doubt, test under
  self-host with `build/cx cx.c test/tests.c`.
- **Local shadowing of non-zero globals works**, but only because the
  function prologue now assigns `id[Val] = i` unconditionally — watch
  this if you rework local-variable allocation.
- **No `unsigned`, `float`, `double`** — not implemented.
- **`int` = `int64_t` (8 bytes)** — use `(int)` casts for printf `%d`.
- **Always verify after changes to cx.c**:
  `cc -Wall -Wpedantic -o build/cx cx.c && build/cx cx.c test/tests.c`
  (runs every test natively *and* through self-hosted cx).

---

## Key Implementation Details

- `memacc()` handles both `.` and `->` member access (shared code path)
- Arrays use `id[Extent]` to store size; the Id handler skips the load (address is the value)
- Struct members stored as linked list: `m[0]=hash, m[1]=type, m[2]=offset, m[3]=next`
- Struct padding: members aligned to natural boundary; struct size padded to 8 bytes
- Postfix loop `while (tk == Brak || tk == '.' || tk == Arrow)` handles chaining like `pts[i].x`
- Postfix `Brak` must use `*++e = PSH` (append), never `*e = PSH` (replace)
- `break`/`continue` use global patch arrays (`brk_patches[]`, `cnt_patches[]`); each
  emits `JMP 0` and records the operand address; patches applied after loop exit
- `for` increment code is buffered (`inc_buf[128]`), rolled back, body emitted, then
  re-emitted — `continue` targets the re-emitted increment start
- Scoped `for (int j = ...)` uses `scope_stack` to save/restore the variable binding;
  restoration must use a local pointer (`d`), not the global `id`, to avoid clobbering
  the currently-parsed identifier

## Common Pitfalls

- Lexer maps `[` to `Brak` token (169), not ASCII `[` (91). Use `tk == Brak`, not `tk == '['`.
- `id` pointer changes after `next()` — save to local (`d`) before calling next() if needed.
- Element size must handle structs everywhere: Add, Sub, Inc, Dec, Brak (postfix and precedence).
- `PSH` pushes `a` without modifying it — subsequent `LI` reloads from address still in `a`.
- Scope restoration loops must use a local pointer variable, not the global `id`, so the
  currently-parsed identifier's entry is not overwritten.

---

## Calling Convention (R-to-L cdecl)

`cx.c` uses classic right-to-left cdecl parameter passing: the last
declared parameter is pushed deepest on the stack, the first declared
parameter ends up closest to `bp`. This means a named parameter's stack
offset is **static** — it depends only on the parameter's declaration
index, never on the caller's arity. For `f(a0, a1, ..., aN-1)`:

```
    bp[0]    = saved bp
    bp[1]    = return address
    bp[2]    = a0         <-- first declared, static offset
    bp[3]    = a1
    ...
    bp[1+N]  = a(N-1)     <-- last declared
    bp[-1]   = local_0
    ...
```

Because cx's single-pass parser reads arguments left-to-right, the
compiler emits arg pushes in source order and then issues a single
`REVN N` opcode that reverses the top N stack entries **at runtime**,
immediately before `JSR`. Sys intrinsics bypass `REVN` — their VM
handlers read args with L-to-R indexing (`sp[n-1-i]`), matching the
emit order. `run()` hand-builds `main`'s frame with `argv` pushed
first, `argc` second, so `bp[2] = argc`, `bp[3] = argv`.

The param-reversal is implemented as a post-parse pass over the symbol
table: during parsing, `id[Val] = i++` assigns 0..N-1 in source order;
after `)` is consumed, a sweep flips them to `(N-1) - Val`, so classic
`loc - d[Val]` LEA emission yields the R-to-L offsets above.

## Varargs (`...`, `va_start`, `va_end`, `va_copy`, `vsnprintf`)

Static named-param offsets make varargs nearly trivial:

- **`...`** — new `Ellipsis` token (produced by `next()` on three `.`s).
  The function-parameter loop consumes it as the end-of-params marker;
  no flag on the symbol. Caller pushes everything normally.
- **`va_list`** — user code `typedef`s it as `int64_t *`. Every stack
  slot is 8 bytes, so `va_arg` is just `*ap++` (no per-type stride).
- **`va_start(&last)`** — Sys intrinsic `I_VSTRT`, VM handler
  `a = *sp + 8;`. Returns the address of the first variadic slot, which
  is one 8-byte slot past the address of the last named param.
- **`va_end(ap)`** — Sys intrinsic `I_VEND`, no-op.
- **`va_copy(src)`** — Sys intrinsic `I_VCPY`, returns its arg unchanged
  (pointers copy trivially).
- **`vsnprintf(buf, n, fmt, ap)`** — Sys intrinsic `I_VSNPF`, dispatches
  to the `cx_vsnprintf` helper in `cx.c`. That helper walks the format
  string and hands each `%…` conversion to host `snprintf` with a
  rebuilt single-spec format, forcing an `ll` length modifier for
  integer conversions so cx's int64 slots format correctly.

Standard user pattern (see `test/sb.h`):

```c
#ifdef __cx__
typedef int64_t * va_list;
#define va_start(ap, last) ap = va_start(&last)
#define va_copy(dest, src) dest = va_copy(src)
#define va_end(ap) va_end(ap)
#endif
```

The recursive-looking macro body is safe because cx's preprocessor
does strict single-pass substitution with no rescan: the inner
`va_start(&last)` in the body is written to the output as-is, where
the main parser then resolves it to the Sys intrinsic.

---

## Future Plans

### Function Pointers

**Priority: HIGH**

Allow storing and calling functions through pointer variables.

**Design:**

- Type encoding: use a dedicated sentinel (e.g., `FNPTR = PTR + PTR`) or reuse `INT64 + PTR`
  with a flag. Simplest: treat function pointer as opaque `int64_t` holding a code address;
  mark with a new class `FnPtr` or reuse `Glo`/`Loc` with a special type.
- Storing a function address: `fp = myfunc;` emits `IMM <fun_address>` (like a global address).
- Indirect call opcode: add `JSRI` — pops the call address from the stack (or reads from `a`),
  pushes return address, jumps. VM: `sp = sp - 1; *sp = (int64_t)(pc + 1); pc = (int64_t *)*sp_arg;`
- Call syntax: `fp(args)` or `(*fp)(args)` — detect in `expr()` when an `Id` with function-pointer
  type is followed by `(`. Push args normally, then emit `PSH; JSRI` (push `fp` value, indirect call).
- Declaration syntax: `int (*fp)(int, int);` — parser extension for declarator with `(*)` inside.
  Minimal approach: `int *fp;` treated as function pointer when assigned a `Fun`-class identifier.

**New opcode required**: `JSRI` (indirect JSR via accumulator or top-of-stack).

**Estimated cost**: ~80 lines.

---

### Unsigned Integer Types (`uint32_t`, `uint64_t`)

**Priority: LOW**

- New opcodes for unsigned compare: `BLTU`, `BGTU`, `BLEU`, `BGEU`
- Unsigned division/modulo opcodes
- Logical (unsigned) right shift
- Type encoding to distinguish signed vs unsigned
- `%u` printf format support

**Estimated cost**: ~100 lines.

---

### Floating Point (`float`, `double`)

**Priority: LOW**

- New VM opcodes: `FADD`, `FSUB`, `FMUL`, `FDIV`, `FEQ`, `FNE`, `FLT`, `FGT`, `FLE`, `FGE`,
  `ITOF`, `FTOI`
- Literal parsing: `1.23`, `1e-5` in `next()`
- Type promotion rules (int op float → float)
- `%f` printf support
- Fundamental change to "everything is an integer" architecture.

**Estimated cost**: ~200–300 lines.

---

### `typedef` / `union`

**Priority: MEDIUM**

- `typedef`: alias for an existing type. Tokens already reserved. Cost: ~30 lines.
- `union`: overlapping member storage (all members at offset 0, size = largest member).
  Token already reserved. Cost: ~50 lines.

---

### Preprocessor Macros (`#define` with arguments)

**Priority: VERY LOW**

A full macro expander is nearly as complex as the compiler itself. Recommendation: keep
restricting to simple numeric constants, or use an external preprocessor (`cpp file.c | ./build/c4`).
