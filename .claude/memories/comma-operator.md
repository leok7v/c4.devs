# Memory: Comma Operator Implementation

**Date**: March 4, 2025
**Feature**: Comma operator `(expr1, expr2, ...)`

## What We Did

### 1. Added Comma Token
**File**: cx.c line ~97

Added `Comma` token with lowest precedence (before `Assign`):
```c
Comma, Assign, Cond, Lor,
```

### 2. Updated Lexer
**File**: cx.c line ~384

Changed comma from pass-through to return `Comma` token:
```c
} else if (tk == ',') {
    tk = Comma;
    return;
}
```

### 3. Added Comma Case to Expression Parser
**File**: cx.c line ~1042

Comma operator simply evaluates right operand (left is discarded):
```c
case Comma:
    next();
    expression(Comma);
    break;
```

Key insight: Don't use ADJ to discard - just let right operand overwrite left in accumulator.

### 4. Updated Top-Level Expression Calls
Changed these from `expression(Assign)` to `expression(Comma)`:
- `if_stmt()` - if condition
- `while_stmt()` - while condition  
- `for_stmt()` - for condition (3rd expression)
- `for_stmt()` - for init (1st expression)
- `switch_stmt()` - switch expression
- `return_stmt()` - return expression
- Parenthesized expressions `(...)`

### 5. Fixed skip_comma()
**File**: cx.c line ~454

Updated to check for `Comma` token instead of ASCII `,`:
```c
void skip_comma() { if (tk == Comma) { next(); } }
```

This allows multiple declarators like `char *p, *lp,` to still work.

### 6. Created Test File
**File**: test/comma.c

Tests:
- Basic comma: `(1, 2, 3)` returns 3
- Comma with assignments: `(a = 5, b = 10)`
- Comma in if/while conditions
- Comma in return statement
- Nested commas: `((1, 2), (3, 4))`

### 7. Updated LANGUAGE.md
- Added comma operator to supported operators
- Updated EBNF: `comma_expr = assign_expr { "," comma_expr }`
- Removed from unsupported features

## Code Impact
- 1 new token
- 1 lexer change
- 1 expression parser case (3 lines)
- 7 expression call site updates
- 1 helper function fix
- Test file: 50 lines

## Verification Commands
```sh
# Direct test
./cx test/comma.c  # exit(0) cycle = 206

# Self-compiled test  
./cx cx.c test/comma.c  # exit(0) cycle = 617678

# Both pass all tests
```

## Notes
- Comma has lowest precedence of all operators
- Left-to-right evaluation, rightmost value returned
- Works in all expression contexts (if, while, for, return, parens)
- Multiple declarators still work via skip_comma()
