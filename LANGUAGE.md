# CC Language Reference

CC implements a subset of C. This document describes the supported grammar and features.

---

## EBNF Grammar

```ebnf
program        = { declaration } ;

declaration    = enum_decl | struct_decl | typedef_decl | global_decl | function_def ;

enum_decl      = "enum" [ identifier ] "{" enum_list "}" ";" ;
enum_list      = identifier [ "=" [ "-" ] number ] { "," identifier [ "=" [ "-" ] number ] } ;

struct_decl    = ( "struct" | "union" ) identifier "{" { member_decl } "}" ";" ;
member_decl    = type_spec { "*" } identifier ";" ;

typedef_decl   = "typedef" type_spec { "*" } identifier ";"
               | "typedef" type_spec "(" "*" identifier ")" "(" [ param_list ] ")" ";" ;

global_decl    = [ "static" ] type_spec { "*" } identifier [ "[" number "]" ] [ "=" initializer ] ";" ;
initializer    = [ "-" ] number | string | identifier ;

function_def   = [ "static" | "inline" ] type_spec { "*" } identifier "(" [ param_list | "void" ] ")" block ;
param_list     = param { "," param } ;
param          = type_spec { "*" } identifier
               | type_spec "(" "*" identifier ")" "(" [ param_list ] ")" ;

type_spec      = "void" | "bool" | "char" | "int" | "int32_t" | "int64_t"
               | ( "struct" | "union" ) identifier
               | identifier ;  (* typedef name *)

block          = "{" { block_item } "}" ;
block_item     = declaration | statement ;

statement      = "if" "(" expression ")" statement [ "else" statement ]
               | "while" "(" expression ")" statement
               | "for" "(" [ for_init ] ";" [ expression ] ";" [ expression ] ")" statement
               | "do" statement "while" "(" expression ")" ";"
               | "switch" "(" expression ")" "{" { switch_case } "}"
               | "return" [ expression ] ";"
               | "break" ";"
               | "continue" ";"
               | block
               | expression ";" ;

for_init       = type_spec { "*" } identifier "=" expression
               | expression ;

switch_case    = ( "case" number | "default" ) ":" { statement } ;

expression     = comma_expr ;
comma_expr     = assign_expr { "," comma_expr } ;
assign_expr    = cond_expr [ ( "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | "<<=" | ">>=" ) assign_expr ] ;
cond_expr      = lor_expr [ "?" expression ":" cond_expr ] ;
lor_expr       = land_expr { "||" land_expr } ;
land_expr      = or_expr { "&&" or_expr } ;
or_expr        = xor_expr { "|" xor_expr } ;
xor_expr       = and_expr { "^" and_expr } ;
and_expr       = eq_expr { "&" eq_expr } ;
eq_expr        = rel_expr { ( "==" | "!=" ) rel_expr } ;
rel_expr       = shift_expr { ( "<" | ">" | "<=" | ">=" ) shift_expr } ;
shift_expr     = add_expr { ( "<<" | ">>" ) add_expr } ;
add_expr       = mul_expr { ( "+" | "-" ) mul_expr } ;
mul_expr       = unary_expr { ( "*" | "/" | "%" ) unary_expr } ;
unary_expr     = ( "sizeof" | "-" | "!" | "~" | "*" | "&" | "++" | "--" ) unary_expr
               | "(" type_spec { "*" } ")" unary_expr
               | postfix_expr ;
postfix_expr   = primary { "[" expression "]" | "(" [ arg_list ] ")" | "." identifier | "->" identifier | "++" | "--" } ;
primary        = number | string | identifier | "(" expression ")" ;
arg_list       = expression { "," expression } ;

number         = digit { digit } | "0x" hex { hex } | "'" char "'" ;
string         = '"' { char } '"' { '"' { char } '"' } ;  (* adjacent strings concatenate *)
identifier     = letter { letter | digit | "_" } ;
```

---

## Supported Features

### Types
- `void`, `bool`, `char`, `int`, `int32_t`, `int64_t`
- Pointers (any depth): `char *`, `int **`, etc.
- Structs and unions with member access (`.`, `->`)
- Arrays (fixed size): `int arr[10]`
- Array initializers: `int arr[] = {1, 2, 3};` (global, local, static local)
- Function pointers: `int (*fp)(int, int)` (declaration supported, calling needs work)
- Typedefs for all above

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Assignment: `=`
- Compound assignment: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- Increment/decrement: `++`, `--` (prefix and postfix)
- Comma: `(expr1, expr2)` - evaluates left (discards), returns right
- Ternary: `? :`
- Sizeof: `sizeof(type)`, `sizeof(expr)`
- Address/dereference: `&`, `*`
- Cast: `(type)expr`
- Member access: `.`, `->`
- Subscript: `[]`
- Function call: `()`

### Control Flow
- `if`/`else`
- `while`, `for`, `do`/`while`
- `switch`/`case`/`default`
- `break`, `continue`, `return`

### Declarations
- Global and local variables
- Global scalar initializers: `int x = 42;`
- Block-scoped variables: `{ int x = 0; }`
- For-loop scoped: `for (int i = 0; ...)`
- Mid-block declarations
- Forward function declarations
- Static local variables: `static int count;`
- `inline` (parsed, ignored)
- `const` (parsed, ignored)

### Other
- String literal concatenation: `"hello " "world"`
- Single-line comments: `//`
- `true`/`false` constants
- Enum with optional explicit values

### Preprocessor
- `#define NAME value` — object-like macros
- `#define NAME` — define without value (for ifdef checks)
- `#undef NAME` — undefine a macro
- `#ifdef NAME` / `#ifndef NAME` — conditional compilation
- `#if os(linux)` / `#if os(apple)` / `#if os(windows)` — OS detection
- `#elif` / `#else` / `#endif` — conditional block control
- `#include "file"` — local file inclusion
- `#include <file>` — system includes (skipped, intrinsics provide stdlib)
- `#pragma once` — include guard
- `#embed "file"` — embed binary file as comma-separated byte values (C23)
- `#embed "file", 0` — embed with null terminator
- `#line N "file"` — line number directive (for error reporting)

---

## Unsupported C99 Features

### Types
- `unsigned`, `signed` qualifiers
- `short`, `long`, `long long`
- `float`, `double`, `long double`
- `_Complex`, `_Imaginary`
- `_Bool` (use `bool`)
- Bit-fields
- Flexible array members
- Variable-length arrays (VLAs)
- `restrict`, `volatile`

### Operators

### Declarations
- Designated initializers: `.field = value`
- Multiple declarators with init: `int a = 1, b = 2;` (partial support)
- Extern declarations

### Preprocessor (unsupported features)
- Function-like macros: `#define MAX(a,b) ...`
- `#if` with arbitrary expressions (only `os()` and 0/1 supported)
- `#error`, `#warning`
- Token pasting (`##`), stringizing (`#`)
- Recursive/nested macro expansion

### Other
- Multi-file compilation / separate compilation
- Linkage (`extern`, file-scope `static` semantics)
- Variadic functions (`...`)
- Goto / labels
- Inline assembly
- Wide characters / Unicode
- `_Generic`

---

## Runtime Environment

- `int` is 64-bit (same as `int64_t`)
- Stack-based VM with 64-bit slots
- All pointers are 64-bit
- Struct padding: members aligned to natural boundary, size padded to 8 bytes
- `printf` limited to 6 arguments (format + 5 values)

### Intrinsics (stdlib replacements)
```
open, read, write, close          File I/O
printf                            Formatted output (max 6 args)
malloc, free, alloca              Memory allocation
memset, memcpy, memmove, memcmp   Memory operations
strcpy, strcat, strcmp, strncmp, strlen  String operations
system, popen, pclose, fread      Process/pipe operations
exit, assert                      Program control
```
