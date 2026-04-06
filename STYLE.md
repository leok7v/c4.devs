# STYLE.md (CRITICAL AGENT INSTRUCTIONS)

## 1. STRUCTURAL LOGIC & FLOW

- SINGLE EXIT: One entry, one exit per function. Use local state variables to 
  route flow smoothly to the end of the function block.
  ```c
  int status = 0;
  if (condition) {
      status = process_data();
  }
  return status;
  ```

- LOOP PURITY: Use local boolean state to terminate loops naturally.
  ```c
  bool running = true;
  while (running) {
      if (check_condition()) {
          running = false;
      } else {
          process_step();
      }
  }
  ```

- EXPRESSION PURITY: Keep evaluation logic separate from assignment.
  ```c
  int c = getc();
  while (c >= 0) {
      parse_char(c);
      c = getc();
  }
  ```

- D.R.Y: If nesting deepens or local state balloons, extract logic into short, 
  static helper functions.

## 2. GEOMETRY & FORMATTING

- DENSITY: Zero empty lines inside function bodies. 
- SEPARATION: Exactly one empty line between functions.
- WIDTH: Maximum line width strictly < 80 characters.
- INDENTATION: 4 spaces. Indent "case" statements inside "switch" blocks.
- BRACES: All control flow (if, while, for, etc.) must use { }.
- LINE BREAKS: Exactly one statement or initialization per line.
  ```c
  int count = 0;
  int limit = 10;
  setup_process();
  ```

- ELSIF: Keep "} else {" and "} else if (cond) {" on the same line as the 
  closing brace of the preceding block.
  ```c
  if (condition) {
      handle_primary();
  } else if (other_condition) {
      handle_secondary();
  } else {
      handle_fallback();
  }
  ```

## 3. SYNTAX & NAMING

- POINTERS & CASTS: Declare pointers with spaces on both sides of the asterisk. 
  Perform type casts with absolutely no internal spaces.
  ```c
  void * ptr = (void*)other_ptr;
  ```

- PARENTHESES: Keep them to the absolute minimum required by compiler warnings.

- VARIABLES: Use short names (e.g., "i") for short scopes. Use single words 
  for locals. Only abbreviate using standard de-facto terms (e.g., "str").
  ```c
  int i = 0;
  char * line = get_input();
  ```

- FUNCTIONS: Avoid verb-only names for local functions. Use nouns or 
  namespace prefixes.
  ```c
  void request_headers();
  void parser_init();
  ```

- COMMENTS: Do not comment the obvious. Explain the "why", not the "what".

## 4. STRUCTS & TYPES

- NO TYPEDEFS: Never use `typedef` for structs. Always use the explicit 
  `struct` keyword to keep data structures transparent.
  ```c
  struct message {
      char role[16];
  };
  // NOT: typedef struct { ... } Message;
  ```

## 5. CASE CONVENTION

- SNAKE CASE: Strictly use `snake_case` for all variables, functions, and 
  struct names. NEVER use `camelCase` or `PascalCase`.
  ```c
  struct stream_state my_state;
  // NOT: StreamState myState;
  ```
