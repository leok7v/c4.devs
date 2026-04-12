# Memory: Function Pointer Implementation (Partial)

**Date**: March 4, 2025
**Feature**: Function pointer declarations `int (*fp)(int, int);`

## What We Did

### 1. Added Function Pointer Declarator Support
**Files**: cx.c - function body declaration loop (line ~2900), block-level declarations (line ~1445), mid-block declarations (line ~1590)

Added parsing for `(*name)(params)` syntax:
```c
// Check for function pointer declarator: (*name)(params)
if (tk == '(') {
    next();
    expect(Mul, "* expected in fnptr decl");
    require(Id, "bad fnptr decl name");
    id[Type] = FNPTR;
    ty = FNPTR;
    next();
    expect(')', ") expected in fnptr decl");
    // Params are optional: (*fp) or (*fp)(int, int)
    if (tk == '(') {
        next();
        while (tk != ')') { next(); }
        expect(')', ") expected");
    }
    is_fnptr = 1;
}
if (!is_fnptr) { require(Id, "bad local declaration"); }
```

### 2. Fixed next() Calls
For function pointers, the identifier is already consumed during fnptr parsing, so we skip the normal `next()` call:
```c
if (!is_fnptr) { next(); }
```

### 3. Created Test File
**File**: test/fnptr.c
- Tests function pointer declarations with and without params
- Assignment and calling not tested (needs more work)

## What Works
- Function pointer declarations: `int (*fp);`
- Function pointer declarations with params: `int (*fp)(int, int);`
- Self-compilation still works

## What Needs More Work
- Assignment: `fp = add;` - "undefined variable" error
- Calling: `fp(3, 4);` - segfault
- The JSRI opcode exists and is used for FNPTR types, but the type setup or assignment isn't working correctly

## Code Impact
- ~20 lines added to 3 declaration handlers
- Function pointer typedef already existed (partial)
- JSRI opcode already existed (partial)

## Verification Commands
```sh
# Declaration test
./cx test/fnptr.c  # exit(0) cycle = 5

# Self-compilation
./cx cx.c test/comma.c  # exit(0) - works
```

## Notes
- Infrastructure for function pointer calling exists (FNPTR type, JSRI opcode)
- Issue is likely in how function addresses are stored/loaded during assignment
- Would need to debug the symbol table lookup and assignment code
