# Plan: Consolidate Grok improvements, extend tests, clean docs, add memories

## Context
Grok reviewed our c4.c and identified 3 valid improvements (struct padding, sizeof struct, struct cast) among many incorrect claims. We need to adopt those 3 improvements, fix tests/grok.c for c4 compatibility, add `#include` headers to all tests so they compile with cc/gcc too, clean up stale docs, and create CLAUDE.md for project memory.

## Changes

### 1. Add struct padding/alignment to c4.c (~5 lines)
**File**: `c4.c` lines 609-618 (struct member parsing)

Before setting `m[2] = i` (member offset), align `i` to the member type's natural alignment:
```c
// Align before setting offset
if (ty == INT32) { i = ((i + 3) / 4) * 4; }
else if (ty != CHAR) { i = ((i + 7) / 8) * 8; } // INT64, ptrs, structs: 8-byte
m[2] = i;
// then add size as before...
```
No impact on self-compilation (c4.c uses no structs internally).

### 2. Add sizeof(struct X) to c4.c (~5 lines)
**File**: `c4.c` lines 172-186 (Sizeof handler in expr())

Add `Struct` case after `Char`:
```c
else if (tk == Struct) {
  next();
  if (tk == Id && id[Class] == Struct) { ty = id[Type]; next(); }
}
```
Change size emission to handle struct types:
```c
*++e = IMM;
if (ty == CHAR) *++e = sizeof(char);
else if (ty == INT32) *++e = 4;
else if (ty > INT64 && ty < PTR) *++e = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
else *++e = sizeof(int64_t);
```

### 3. Add (struct X *) cast to c4.c (~3 lines)
**File**: `c4.c` lines 214-231 (cast handler in expr())

Add `|| tk == Struct` to the cast condition, plus handler:
```c
else if (tk == Struct) { next(); if (tk == Id && id[Class] == Struct) { t = id[Type]; next(); } }
```

### 4. Add #include headers to ALL test files
Each test file gets headers so `cc tests/foo.c -o /tmp/foo && /tmp/foo` works alongside `./build/c4 tests/foo.c`. c4 ignores `#` lines, so this is safe.

| File(s) | Headers needed |
|---------|---------------|
| struct/simple.c, struct/ptr.c, struct/nested.c, args.c, test_int_ptr.c, test_arrow_simple.c, simple_int32.c, struct_ptr_arith.c | `<stdio.h>` |
| arrays.c, grok.c | `<stdio.h>`, `<stdint.h>` |
| int32_64.c | already has `<stdio.h>`, add `<stdint.h>` |
| io.c | `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`, `<fcntl.h>` |
| system.c | `<stdio.h>`, `<stdlib.h>` |
| meta.c | `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`, `<fcntl.h>` |
| test.c | `<stdio.h>`, `<stdlib.h>`, `<string.h>` |
| ptr_test.c | already has `<stdio.h>` |

### 5. Rewrite tests/grok.c for c4 compatibility
Grok's test uses c4-incompatible syntax:
- `int32_t x = 10;` → split to `int32_t x; x = 10;`
- `struct Point *pp = pts;` → split to `struct Point *pp; pp = pts;`
- `sizeof(struct Point) == 8` → wrong for c4 (int=8B, so Point=16B). Use `sizeof(struct Point) == 2 * sizeof(int)` or just print values.
- Merge the useful parts (padding, nested padding, struct ptr arith) into existing tests or keep as standalone test with c4-compatible syntax.

### 6. Clean up AGENTS.md
Rewrite to reflect current state:
- PTR=256 type encoding with sequential struct IDs (3-255)
- struct_syms lookup table, memacc() function
- Array support (local/global, all element types)
- Struct padding/alignment
- sizeof(struct), (struct *) casts
- All tests passing, self-compilation works
- Remove stale bug reports and outdated type system descriptions

### 7. Clean up RFC.md
Update completed sections:
- int32_t/int64_t: mark self-compilation FIXED, struct regression FIXED
- struct: mark as fully working, note new features (padding, sizeof, cast)
- Remove stale "broken" notes

### 8. Create CLAUDE.md for project memory
**File**: `/Users/leo/github.com/leok7v/c4/CLAUDE.md`

Key facts to persist:
- Project: c4 self-compiling minimal C compiler (Robert Swierczek)
- Type encoding: CHAR=0, INT32=1, INT64=2, structs=3-255, PTR=256
- struct_syms[id - INT64 - 1] maps struct ID → symbol entry
- memacc() handles . and -> member access
- Arrays use id[Extent] field, type becomes elem_type + PTR
- c4's `int` = int64_t (8 bytes), different from cc/gcc (4 bytes)
- All code must self-compile: no forward refs, no mid-block decls, no complex macros
- Test with: direct (./build/c4 tests/X.c) and self-comp (./build/c4 c4.c tests/X.c)
- Build: clang -o build/c4 c4.c -O3 -m64 -std=c11 -Wall
- CI: .github/workflows/ci.yml, uses -w flag for Linux format warnings

## Verification
1. `clang -o build/c4 c4.c -O3 -m64 -std=c11 -Wall` — clean build
2. `./build/c4 tests/grok.c` — padding, sizeof struct, struct ptr arith pass
3. `./build/c4 tests/arrays.c` — arrays still pass
4. `./build/c4 tests/struct/simple.c` etc. — all struct tests pass
5. `./build/c4 tests/struct_ptr_arith.c` — struct pointer arithmetic passes
6. `./build/c4 tests/int32_64.c` — int32/64 still passes
7. `timeout 10 ./build/c4 c4.c tests/struct/simple.c` — self-compilation works
8. `cc tests/grok.c -o /tmp/grok && /tmp/grok` — test compiles/runs with cc too
9. `cc tests/arrays.c -o /tmp/arrays && /tmp/arrays` — arrays test works with cc
