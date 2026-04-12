# Memory: Compound Assignment Implementation

**Date**: March 4, 2025
**Feature**: Compound assignment operators (+=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=)

## What We Did

### 1. Verified Array Initializers Already Worked
- Tested `int arr[] = {1, 2, 3};` — already supported in cx.c
- Returns correct values (1+2+3=6)

### 2. Implemented Compound Assignment Operators
**New tokens added** (cx.c line ~98):
- `AddAssign, SubAssign, MulAssign, DivAssign, ModAssign`
- `AndAssign, OrAssign, XorAssign, ShlAssign, ShrAssign`

**Lexer updates** (cx.c lines ~260-350):
- `+=` after `++` check
- `-=` after `-->` check  
- `<<=` after `<<` check
- `>>=` after `>>` check
- `*=` after `*` check
- `/=` after `/` check (before comment handling)
- `%=` after `%` check
- `&=` after `&&` check
- `|=` after `||` check
- `^=` after `^` check

**Expression parser** (cx.c lines ~1040+):
Each compound assignment follows this pattern:
```c
case AddAssign:
    next();
    if (*e == LC) { *e = PSH; *++e = LC;
    } else if (*e == LI32) { *e = PSH; *++e = LI32;
    } else if (*e == LI) { *e = PSH; *++e = LI;
    } else { fatal("bad lvalue in compound assignment"); }
    *++e = PSH;
    expression(AddAssign);
    ty = t;
    *++e = ADD;
    store();
    break;
```
Key insight: Change load to push+load, then push value for operation, then store result.

### 3. Created Test File
**File**: test/compound.c
- Tests all 10 compound assignment operators
- Tests chained assignments (x += 1; x *= 2; x -= 1;)
- Returns 0 if all pass, non-zero error code if any fail

### 4. Updated LANGUAGE.md
- Added compound assignment to "Supported Features" → "Operators"
- Updated EBNF: `assign_expr = cond_expr [ ( "=" | "+=" | ... | ">>=" ) assign_expr ]`
- Removed compound assignment from "Unsupported C99 Features"
- Removed array/struct initializers from "Unsupported C99 Features" (already worked)

### 5. Verified Self-Compilation
- Compiled cx.c with gcc: `gcc -o cx cx.c -D_GNU_SOURCE`
- Self-compiled: `./cx cx.c test/compound.c`
- Both pass all tests (exit 0)

## Code Impact
- ~10 new tokens
- ~10 lexer rule modifications
- ~10 expression parser cases (~150 lines total)
- Test file: 60 lines
- Documentation updates

## Verification Commands
```sh
# Direct test
./cx test/compound.c  # exit(0) cycle = 211

# Self-compiled test
./cx cx.c test/compound.c  # exit(0) cycle = 781722
```
