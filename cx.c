// cx.c - self-hosting, self-compiling, self-excuting compiler in C
//
// originated from:
// https://github.com/rswier/c4
//
// c4.c - C in four functions
// char, int, and pointer types
// if, while, return, and expression statements
// just enough features to allow self-compilation and a bit more
// Written by Robert Swierczek

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#if defined(__linux__)
    #define _GNU_SOURCE
#elif defined(__APPLE__)
    #define _DARWIN_C_SOURCE
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

char *p, *lp, // current position in source code
     *data;   // data/bss pointer

int64_t *e, *le,  // current position in emitted code
    *id,      // currently parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    *struct_syms, // struct type table (ID -> symbol entry)
    tk,       // current token
    ival,     // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    cur_file, // current file pointer (into string table)
    debug,    // print executed instructions
    num_structs, // number of defined structs
    members_next, // members_pool allocation index
    i,        // multipurpose counter / local offset
    scope_sp; // scope stack pointer

static int64_t sym_pool[32768]   = {0};
static int64_t code_pool[32768]  = {0};
static int64_t stack_pool[32768] = {0};
char* src_pool;
int64_t struct_syms_arr[256] = {0};
int64_t members_pool[4096]   = {0};
int64_t scope_stack[1024]    = {0};
int64_t brk_patches[512]     = {0};
int64_t cnt_patches[512]     = {0};
int64_t fwd_patches[1024]    = {0};
int64_t brk_sp;
int64_t cnt_sp;
int64_t fwd_sp;
int64_t fn_ret_ty; // current function return type (for struct returns)
int64_t struct_temp; // offset of struct temp area in local frame (from bp)
const int64_t poolsz = 262144;  // 256KB for pools
const int64_t srcbufsz = 4194304; // 4MB for source/preprocessor

// preprocessor state
char *pp_names[512] = {0};   // macro names
char *pp_values[512] = {0};  // macro values (0 = defined without value)
int64_t pp_nlen[512] = {0};  // name lengths
int64_t pp_vlen[512] = {0};  // value lengths
int64_t pp_count = 0;        // number of macros
int64_t pp_cond[64] = {0};   // conditional stack (1=emit, 0=skip)
int64_t pp_cond_sp  = 0;     // conditional stack pointer
char *pp_once[256]  = {0};   // #pragma once file list
int64_t pp_once_count = 0;   // number of once files
int64_t pp_line = 0;         // current line in preprocessing
char *pp_file = 0;           // current file being preprocessed
// function-like macro params: up to 8 params per macro, 16 chars each
// flattened: pp_params[macro_idx * 128 + param_idx * 16 + char_idx]
char pp_params[65536]   = {0}; // 512 macros * 8 params * 16 chars
int64_t pp_pcount[512]  = {0}; // param count per macro (-1 = object-like)
// file table for error reporting
char file_table[8192]   = {0}; // storage for filenames
int64_t file_table_next = 0;   // next free position
// command-line defines storage (set before compile, used during preprocess)
char *cmdline_defs[256] = {0};
int64_t cmdline_def_count = 0;

// tokens and classes (operators last and in precedence order)
enum {
    Num = 128, Fun, Sys, Glo, Loc, Id, Tdef,
    Bool, Char, Const, Else, Enum, If, Inline, Int, Int32_t, Int64_t, Return,
    Sizeof, Static, Struct, Typedef, Union, Void, While, For, Do, Switch, Case,
    Default, Break, Continue,
    Comma, Assign, Cond, Lor,
    Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div,
    Mod, Inc, Dec, Brak, Arrow,
    AddAssign, SubAssign, MulAssign, DivAssign, ModAssign, AndAssign, OrAssign, XorAssign, ShlAssign, ShrAssign
};

// opcodes (VM instructions)
enum {
    LEA, IMM, JMP, JSR, BZ, BNZ, ENT, ADJ, LEV, LI, LC, LI32, SI, SC, SI32,
    PSH, OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV,
    MOD, DUP, SPGET, SWAP, JSRI, MCPY, EXIT, PEEK
};

// intrinsics (system calls)
enum {
    I_OPEN = 64, I_READ, I_CLOS, I_PRTF, I_MALC, I_FREE, I_MSET, I_MCMP,
    I_EXIT, I_WRIT, I_SYST, I_POPN, I_PCLS, I_FRED, I_MCPY, I_MMOV,
    I_SCPY, I_SCMP, I_SLEN, I_SCAT, I_SNCM, I_ASRT, I_ALCA,
    I_MRED, I_MCLS, I_MWRT,
    I_LSEEK, I_MMAP, I_MUNMAP, I_MSYNC, I_FTRUNC, I_REN,
    I_LAST
};

// types
// FNPTR = 255 reserved for function pointers (just before PTR)
enum { CHAR, INT32, INT64, FNPTR = 255, PTR = 256 };

// identifier offsets (since we can't create an ident struct)
enum {
    Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Utyedef, Extent,
    Sline, Idsz
};

void next() {
    char *pp;
    int64_t opc; // opcode for -s output, local to avoid clobbering global i
    while ((tk = *p)) {
        ++p;
        if (tk == '\n') {
            if (src) {
                printf("%d: %.*s", (int)line, (int)(p - lp), lp);
                lp = p;
                while (le < e) {
                    opc = *++le;
                    if (opc < I_OPEN) {
                        printf("%8.4s",
                            &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,"
                             "LEV ,LI  ,LC  ,LI32,SI  ,SC  ,SI32,PSH ,"
                             "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,"
                             "GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                             "DUP ,SPGT,SWAP,JSRI,MCPY,EXIT,PEEK,"
                             [opc * 5]);
                    } else {
                        printf("%8.4s",
                            &"OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,"
                             "IEXT,WRIT,SYST,POPN,PCLS,FRED,IMCP,MMOV,"
                             "SCPY,SCMP,SLEN,SCAT,SNCM,ASRT,ALCA,"
                             [(opc - I_OPEN) * 5]);
                    }
                    if (opc <= ADJ) {
                        printf(" %d\n", (int)*++le);
                    } else {
                        printf("\n");
                    }
                }
            }
            ++line;
        } else if (tk == '#') {
            // handle #line directive from preprocessor: #line N "file"
            if (p[0] == 'l' && p[1] == 'i' && p[2] == 'n' && p[3] == 'e' &&
                p[4] == ' ') {
                p = p + 5;
                int64_t newline = 0;
                while (*p >= '0' && *p <= '9') { newline = newline * 10 + *p++ - '0'; }
                line = newline - 1; // will be incremented at \n
                while (*p == ' ' || *p == '\t') { ++p; }
                if (*p == '"') {
                    ++p;
                    char *fstart = file_table + file_table_next;
                    while (*p && *p != '"' && *p != '\n' &&
                           file_table_next < 8192 - 1) {
                        file_table[file_table_next++] = *p++;
                    }
                    file_table[file_table_next++] = 0;
                    cur_file = (int64_t)fstart;
                }
            }
            while (*p != 0 && *p != '\n') { ++p; }
        } else if ((tk >= 'a' && tk <= 'z')
                   || (tk >= 'A' && tk <= 'Z')
                   || tk == '_') {
            pp = p - 1;
            while ((*p >= 'a' && *p <= 'z')
                   || (*p >= 'A' && *p <= 'Z')
                   || (*p >= '0' && *p <= '9')
                   || *p == '_') {
                tk = tk * 147 + *p++;
            }
            tk = (tk << 6) + (p - pp);
            id = sym;
            while (id[Tk]) {
                if (tk == id[Hash]
                    && !memcmp((char *)id[Name], pp, p - pp)) {
                    tk = id[Tk];
                    return;
                }
                id = id + Idsz;
            }
            id[Name] = (int64_t)pp;
            id[Hash] = tk;
            tk = id[Tk] = Id;
            return;
        } else if (tk >= '0' && tk <= '9') {
            if ((ival = tk - '0')) {
                while (*p >= '0' && *p <= '9') {
                    ival = ival * 10 + *p++ - '0';
                }
            } else if (*p == 'x' || *p == 'X') {
                while ((tk = *++p)
                       && ((tk >= '0' && tk <= '9')
                           || (tk >= 'a' && tk <= 'f')
                           || (tk >= 'A' && tk <= 'F'))) {
                    ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
                }
            } else {
                while (*p >= '0' && *p <= '7') {
                    ival = ival * 8 + *p++ - '0';
                }
            }
            tk = Num;
            return;
        } else if (tk == '/') {
            if (*p == '/') {
                ++p;
                while (*p != 0 && *p != '\n') {
                    ++p;
                }
            } else if (*p == '=') {
                ++p;
                tk = DivAssign;
            } else {
                tk = Div;
            }
            return;
        } else if (tk == '\'' || tk == '"') {
            pp = data;
            while (*p != 0 && *p != tk) {
                if ((ival = *p++) == '\\') {
                    if ((ival = *p++) == 'n') {
                        ival = '\n';
                    }
                }
                if (tk == '"') {
                    *data++ = ival;
                }
            }
            ++p;
            if (tk == '"') {
                *data++ = 0; // null terminate string
                ival = (int64_t)pp;
            } else {
                tk = Num;
            }
            return;
        } else if (tk == '=') {
            if (*p == '=') {
                ++p;
                tk = Eq;
            } else {
                tk = Assign;
            }
            return;
        } else if (tk == '+') {
            if (*p == '+') {
                ++p;
                tk = Inc;
            } else if (*p == '=') {
                ++p;
                tk = AddAssign;
            } else {
                tk = Add;
            }
            return;
        } else if (tk == '-') {
            if (*p == '-') {
                ++p;
                tk = Dec;
            } else if (*p == '>') {
                ++p;
                tk = Arrow;
            } else if (*p == '=') {
                ++p;
                tk = SubAssign;
            } else {
                tk = Sub;
            }
            return;
        } else if (tk == '!') {
            if (*p == '=') {
                ++p;
                tk = Ne;
            }
            return;
        } else if (tk == '<') {
            if (*p == '<') {
                ++p;
                if (*p == '=') {
                    ++p;
                    tk = ShlAssign;
                } else {
                    tk = Shl;
                }
            } else if (*p == '=') {
                ++p;
                tk = Le;
            } else {
                tk = Lt;
            }
            return;
        } else if (tk == '>') {
            if (*p == '>') {
                ++p;
                if (*p == '=') {
                    ++p;
                    tk = ShrAssign;
                } else {
                    tk = Shr;
                }
            } else if (*p == '=') {
                ++p;
                tk = Ge;
            } else {
                tk = Gt;
            }
            return;
        } else if (tk == '|') {
            if (*p == '|') {
                ++p;
                tk = Lor;
            } else if (*p == '=') {
                ++p;
                tk = OrAssign;
            } else {
                tk = Or;
            }
            return;
        } else if (tk == '&') {
            if (*p == '&') {
                ++p;
                tk = Lan;
            } else if (*p == '=') {
                ++p;
                tk = AndAssign;
            } else {
                tk = And;
            }
            return;
        } else if (tk == '^') {
            if (*p == '=') {
                ++p;
                tk = XorAssign;
            } else {
                tk = Xor;
            }
            return;
        } else if (tk == '%') {
            if (*p == '=') {
                ++p;
                tk = ModAssign;
            } else {
                tk = Mod;
            }
            return;
        } else if (tk == '*') {
            if (*p == '=') {
                ++p;
                tk = MulAssign;
            } else {
                tk = Mul;
            }
            return;
        } else if (tk == '[') {
            tk = Brak;
            return;
        } else if (tk == '?') {
            tk = Cond;
            return;
        } else if (tk == ',') {
            tk = Comma;
            return;
        } else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' ||
                   tk == '(' || tk == ')' || tk == ']' ||
                   tk == ':' || tk == '.') {
            return;
        }
    }
}

void fatal(char *s) {
    if (cur_file) {
        printf("%s:%d: %s\n", (char *)cur_file, (int)line, s);
    } else {
        printf("%d: %s\n", (int)line, s);
    }
    exit(-1);
}

void intrinsic(char *name, int64_t opcode) {
    p = name;
    next();
    id[Class] = Sys;
    id[Type] = INT64;
    id[Val] = opcode;
}

void intrinsics() {
    intrinsic("open", I_OPEN);
    intrinsic("read", I_READ);
    intrinsic("close", I_CLOS);
    intrinsic("printf", I_PRTF);
    intrinsic("malloc", I_MALC);
    intrinsic("free", I_FREE);
    intrinsic("memset", I_MSET);
    intrinsic("memcmp", I_MCMP);
    intrinsic("exit", I_EXIT);
    intrinsic("write", I_WRIT);
    intrinsic("system", I_SYST);
    intrinsic("popen", I_POPN);
    intrinsic("pclose", I_PCLS);
    intrinsic("fread", I_FRED);
    intrinsic("memcpy", I_MCPY);
    intrinsic("memmove", I_MMOV);
    intrinsic("strcpy", I_SCPY);
    intrinsic("strcmp", I_SCMP);
    intrinsic("strlen", I_SLEN);
    intrinsic("strcat", I_SCAT);
    intrinsic("strncmp", I_SNCM);
    intrinsic("assert", I_ASRT);
    intrinsic("alloca", I_ALCA);
    intrinsic("memread", I_MRED);
    intrinsic("memclose", I_MCLS);
    intrinsic("memwrite", I_MWRT);
    intrinsic("lseek", I_LSEEK);
    intrinsic("mmap", I_MMAP);
    intrinsic("munmap", I_MUNMAP);
    intrinsic("msync", I_MSYNC);
    intrinsic("ftruncate", I_FTRUNC);
    intrinsic("rename", I_REN);
}

void expect(int64_t t, char *s) { // expect token t and advance, else fatal
    if (tk == t) {
        next();
    } else {
        fatal(s);
    }
}

void skip_comma() { if (tk == Comma) { next(); } }
void skip_const() { while (tk == Const) { next(); } }
void require(int64_t t, char *s) { if (tk != t) { fatal(s); } }

void load() { // emit load instruction based on ty
    if (ty == CHAR) { *++e = LC;
    } else if (ty == INT32) { *++e = LI32;
    } else if (ty > INT64 && ty < FNPTR) { // struct: keep address
    } else { *++e = LI;
    }
}

void store() { // emit store instruction based on ty
    if (ty == CHAR) {
        *++e = SC;
    } else if (ty == INT32) {
        *++e = SI32;
    } else if (ty > INT64 && ty < FNPTR) {
        // struct assignment: memcpy from src (a) to dest (sp[0])
        int64_t sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
        *++e = PSH;           // push src; stack: [dest, src]
        *++e = IMM;
        *++e = sz;
        *++e = PSH;           // stack: [dest, src, size]
        *++e = MCPY;
        *++e = ADJ;
        *++e = 3;
    } else {
        *++e = SI;
    }
}

void member() { // member access
    require(Id, "bad struct member");
    int64_t *s = (int64_t *)struct_syms[ty - INT64 - 1];
    int64_t *m = (int64_t *)s[Sline];
    int64_t t = 0;
    while (m && !t) {
        if (m[0] == id[Hash]) {
            *++e = PSH;
            *++e = IMM;
            *++e = m[2];
            *++e = ADD;
            ty = m[1];
            load();
            t = 1;
        } else {
            m = (int64_t *)m[3];
        }
    }
    if (!t) { fatal("member not found"); }
    next();
}

void expression(int64_t lev);

void binop(int64_t op, int64_t lev) { // binary operator
    next();
    *++e = PSH;
    expression(lev);
    *++e = op;
    ty = INT64;
}

void emit_size(int64_t t) { // emit element size for pointer arithmetic
    if (t == CHAR) { *++e = sizeof(char);
    } else if (t == INT32) { *++e = 4;
    } else if (t > INT64 && t < FNPTR) {
        *++e = ((int64_t *)struct_syms[t - INT64 - 1])[Val];
    } else { *++e = sizeof(int64_t);
    }
}

void add(int64_t t) { // pointer-aware addition
    next();
    *++e = PSH;
    expression(Mul);
    if ((ty = t) > PTR) {
        *++e = PSH;
        *++e = IMM;
        emit_size(ty - PTR);
        *++e = MUL;
        ty = t;
    }
    *++e = ADD;
}

void sub(int64_t t) { // pointer-aware subtraction
    next();
    *++e = PSH;
    expression(Mul);
    if (t > PTR && t == ty) { // pointer - pointer = element count
        *++e = SUB;
        *++e = PSH;
        *++e = IMM;
        emit_size(t - PTR);
        *++e = DIV;
        ty = INT64;
    } else if ((ty = t) > PTR) { // pointer - int
        *++e = PSH;
        *++e = IMM;
        emit_size(ty - PTR);
        *++e = MUL;
        *++e = SUB;
        ty = t;
    } else {
        *++e = SUB;
    }
}

void number() {
    *++e = IMM;
    *++e = ival;
    next();
    ty = INT64;
}

void string_literal() {
    char *first, *ptrs[64], *result, *dst, *src;
    int n, i;
    first = (char *)ival;
    next();
    if (tk != '"') {
        *++e = IMM;
        *++e = (int64_t)first;
        data = (char *)(((int64_t)data + sizeof(int64_t)) & -sizeof(int64_t));
    } else {
        n = 0;
        ptrs[n++] = first;
        while (tk == '"') {
            ptrs[n++] = (char *)ival;
            next();
        }
        result = data;
        dst = result;
        i = 0;
        while (i < n) {
            src = ptrs[i++];
            while (*src) { *dst++ = *src++; }
        }
        *dst++ = 0;
        *++e = IMM;
        *++e = (int64_t)result;
        data = (char *)(((int64_t)dst + sizeof(int64_t)) & -sizeof(int64_t));
    }
    ty = PTR;
}

void size_of() {
    expect('(', "open paren expected in sizeof");
    ty = INT64;
    skip_const();
    if (tk == Int || tk == Bool) {
        next();
    } else if (tk == Int32_t) {
        next(); ty = INT32;
    } else if (tk == Int64_t) {
        next(); ty = INT64;
    } else if (tk == Char) {
        next(); ty = CHAR;
    } else if (tk == Struct || tk == Union) {
        next();
        if (tk == Id && id[Class] == Struct) { ty = id[Type]; next(); }
    } else if (tk == Id && id[Class] == Tdef) {
        ty = id[Type];
        next();
    }
    while (tk == Mul) { next(); ty = ty + PTR; }
    expect(')', "close paren expected in sizeof");
    *++e = IMM;
    emit_size(ty);
    ty = INT64;
}

void expression(int64_t lev);
void statement();

void if_stmt() {
    int64_t *b;
    expect('(', "open paren expected");
    expression(Comma);
    expect(')', "close paren expected");
    *++e = BZ;
    b = ++e;
    statement();
    if (tk == Else) {
        *b = (int64_t)(e + 3);
        *++e = JMP;
        b = ++e;
        next();
        statement();
    }
    *b = (int64_t)(e + 1);
}

void while_stmt() {
    int64_t *a, *b, j;
    int64_t saved_brk_sp, saved_cnt_sp;
    a = e + 1;
    expect('(', "open paren expected");
    expression(Comma);
    expect(')', "close paren expected");
    *++e = BZ;
    b = ++e;
    saved_brk_sp = brk_sp;
    saved_cnt_sp = cnt_sp;
    statement();
    *++e = JMP;
    *++e = (int64_t)a;
    *b = (int64_t)(e + 1);
    j = saved_cnt_sp;
    while (j < cnt_sp) { *(int64_t *)cnt_patches[j] = (int64_t)a; j = j + 1; }
    j = saved_brk_sp;
    while (j < brk_sp) {
        *(int64_t *)brk_patches[j] = (int64_t)(e + 1); j = j + 1;
    }
    cnt_sp = saved_cnt_sp;
    brk_sp = saved_brk_sp;
}

void do_while_stmt() {
    int64_t *a, j, inc_top;
    int64_t saved_brk_sp, saved_cnt_sp;
    a = e + 1;
    saved_brk_sp = brk_sp;
    saved_cnt_sp = cnt_sp;
    statement();
    if (tk == While) {
        next();
    } else {
        fatal("while expected after do");
    }
    if (tk == '(') {
        next();
    } else {
        fatal("open paren expected");
    }
    inc_top = (int64_t)(e + 1);
    expression(Comma);
    expect(')', "close paren expected");
    *++e = BNZ;
    *++e = (int64_t)a;
    expect(';', "semicolon expected");
    j = saved_cnt_sp;
    while (j < cnt_sp) { *(int64_t *)cnt_patches[j] = inc_top; j = j + 1; }
    j = saved_brk_sp;
    while (j < brk_sp) {
        *(int64_t *)brk_patches[j] = (int64_t)(e + 1); j = j + 1;
    }
    cnt_sp = saved_cnt_sp;
    brk_sp = saved_brk_sp;
}

void switch_stmt() {
    int64_t *b, *break_stack[256], **break_sp, case_val;
    expect('(', "open paren expected");
    expression(Comma);
    expect(')', "close paren expected");
    *++e = PSH;
    expect('{', "open brace expected");
    break_sp = break_stack;
    b = 0;
    while (tk != '}') {
        if (tk == Case) {
            if (b) { *b = (int64_t)(e + 1); }
            next();
            case_val = 0;
            if (tk == Num) {
                case_val = ival; next();
            } else if (tk == Id && id[Class] == Num) {
                case_val = id[Val]; next();
            } else {
                fatal("case: number or enum expected");
            }
            *++e = PEEK;
            *++e = PSH;
            *++e = IMM;
            *++e = case_val;
            *++e = EQ;
            *++e = BZ;
            b = ++e;
            expect(':', "colon expected");
        } else if (tk == Default) {
            if (b) { *b = (int64_t)(e + 1); }
            next();
            expect(':', "colon expected");
            b = 0;
        } else if (tk == Break) {
            next();
            if (tk == ';') {
                next();
            } else {
                fatal("semicolon expected");
            }
            *++e = JMP;
            *break_sp++ = ++e;
        } else {
            statement();
        }
    }
    next();
    if (b) { *b = (int64_t)(e + 1); }
    while (break_sp > break_stack) { **--break_sp = (int64_t)(e + 1); }
    *++e = ADJ;
    *++e = 1;
}

void return_stmt() {
    if (tk != ';') {
        expression(Comma);
        if (fn_ret_ty > INT64 && fn_ret_ty < FNPTR) {
            int64_t sty, sz;
            sty = fn_ret_ty - INT64 - 1;
            sz = ((int64_t *)struct_syms[sty])[Val];
            *++e = PSH;
            *++e = LEA;
            *++e = loc + 1;
            *++e = LI;
            *++e = PSH;
            *++e = SWAP;
            *++e = IMM;
            *++e = sz;
            *++e = PSH;
            *++e = MCPY;
            *++e = ADJ;
            *++e = 3;
            *++e = LEA;
            *++e = loc + 1;
            *++e = LI;
        }
    }
    *++e = LEV;
    expect(';', "semicolon expected");
}

void postinc(int64_t t) { // post-increment/decrement
    int64_t base_ty;
    if (*e == LC) { *e = PSH; *++e = LC;
    } else if (*e == LI32) { *e = PSH; *++e = LI32;
    } else if (*e == LI) { *e = PSH; *++e = LI;
    } else { fatal("bad lvalue in post-increment"); }
    *++e = PSH;
    *++e = IMM;
    if (ty > PTR) {
        base_ty = ty - PTR;
        if (base_ty == CHAR) { *++e = sizeof(char);
        } else if (base_ty == INT32) { *++e = 4;
        } else if (base_ty > INT64 && base_ty < FNPTR) {
            *++e = ((int64_t *)struct_syms[base_ty - INT64 - 1])[Val];
        } else { *++e = sizeof(int64_t); }
    } else { *++e = 1; }
    *++e = (t == Inc) ? ADD : SUB;
    store();
    *++e = PSH;
    *++e = IMM;
    if (ty > PTR) { emit_size(ty - PTR);
    } else { *++e = 1; }
    *++e = (t == Inc) ? SUB : ADD;
    next();
}

void expression(int64_t lev) {
    int64_t t, *d, base_ty;
    if (!tk) { fatal("unexpected eof in expression"); }
    switch (tk) {
        case Num:
            number();
            break;
        case '"':
            string_literal();
            break;
        case Sizeof:
            next();
            size_of();
            break;
        case Id:
            d = id;
            next();
            if (tk == '(') {
                next();
                t = 0;
                if (d[Class] == Fun && d[Type] > INT64 && d[Type] < FNPTR) {
                    *++e = LEA;
                    *++e = loc - struct_temp;
                    *++e = PSH;
                    ++t;
                }
                while (tk != ')') {
                    expression(Assign);
                    *++e = PSH;
                    ++t;
                    skip_comma();
                }
                next();
                if (d[Class] == Sys) {
                    *++e = d[Val];
                } else if (d[Class] == Fun) {
                    *++e = JSR;
                    *++e = d[Val];
                    if (!d[Val]) {
                        fwd_patches[fwd_sp] = (int64_t)d;
                        fwd_patches[fwd_sp + 1] = (int64_t)e;
                        fwd_sp = fwd_sp + 2;
                    }
                } else if (d[Type] == FNPTR) {
                    if (d[Class] == Loc) {
                        *++e = LEA;
                        *++e = loc - d[Val];
                    } else if (d[Class] == Glo) {
                        *++e = IMM;
                        *++e = d[Val];
                    }
                    *++e = LI;
                    *++e = JSRI;
                } else {
                    fatal("bad function call");
                }
                if (t) { *++e = ADJ; *++e = t; }
                ty = d[Type];
                if (ty == FNPTR) { ty = INT64; }
            } else if (d[Class] == Fun) {
                *++e = IMM;
                *++e = d[Val];
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
                    *++e = IMM;
                    *++e = d[Val];
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
        case '(':
            next();
            skip_const();
            if (tk == Int || tk == Bool || tk == Int32_t || tk == Int64_t ||
                tk == Char || tk == Struct || tk == Union ||
                (tk == Id && id[Class] == Tdef)) {
                t = INT64;
                if (tk == Int || tk == Bool) { next();
                } else if (tk == Int32_t) { next(); t = INT32;
                } else if (tk == Int64_t) { next(); t = INT64;
                } else if (tk == Char) { next(); t = CHAR;
                } else if (tk == Struct || tk == Union) {
                    next();
                    if (tk == Id && id[Class] == Struct) { t = id[Type]; next(); }
                } else if (tk == Id && id[Class] == Tdef) {
                    t = id[Type]; next();
                }
                while (tk == Mul) { next(); t = t + PTR; }
                if (tk == ')') {
                    next();
                } else {
                    fatal("bad cast");
                }
                expression(Inc);
                ty = t;
            } else {
                expression(Comma);
                expect(')', "close paren expected");
            }
            break;
        case Mul:
            next();
            expression(Inc);
            if (ty > INT64) {
                ty = ty - PTR;
            } else {
                fatal("bad dereference");
            }
            load();
            break;
        case And:
            next();
            expression(Inc);
            if (*e == LC || *e == LI32 || *e == LI) {
                --e;
            } else if (ty > INT64 && ty < FNPTR) {
                // struct: address is already in accumulator
            } else {
                fatal("bad address-of");
            }
            ty = ty + PTR;
            break;
        case '!':
            next();
            expression(Inc);
            *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ;
            ty = INT64;
            break;
        case '~':
            next();
            expression(Inc);
            *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR;
            ty = INT64;
            break;
        case Add:
            next();
            expression(Inc);
            ty = INT64;
            break;
        case Sub:
            next();
            *++e = IMM;
            if (tk == Num) {
                *++e = -ival;
                next();
            } else {
                *++e = -1;
                *++e = PSH;
                expression(Inc);
                *++e = MUL;
            }
            ty = INT64;
            break;
        case Inc:
        case Dec:
            t = tk;
            next();
            expression(Inc);
            if (*e == LC) { *e = PSH; *++e = LC;
            } else if (*e == LI32) { *e = PSH; *++e = LI32;
            } else if (*e == LI) { *e = PSH; *++e = LI;
            } else { fatal("bad lvalue in pre-increment"); }
            *++e = PSH;
            *++e = IMM;
            if (ty > PTR) {
                base_ty = ty - PTR;
                if (base_ty == CHAR) { *++e = sizeof(char);
                } else if (base_ty == INT32) { *++e = 4;
                } else if (base_ty > INT64 && base_ty < FNPTR) {
                    *++e = ((int64_t *)struct_syms[base_ty - INT64 - 1])[Val];
                } else { *++e = sizeof(int64_t); }
            } else { *++e = 1; }
            *++e = (t == Inc) ? ADD : SUB;
            store();
            break;
        default:
            fatal("bad expression");
    }
    // postfix operators
    while (tk == Brak || tk == '.' || tk == Arrow) {
        if (tk == Brak) {
            t = ty;
            next();
            *++e = PSH;
            expression(Assign);
            expect(']', "close bracket expected");
            if (t > INT64) {
                *++e = PSH;
                *++e = IMM;
                emit_size(t - PTR);
                *++e = MUL;
            } else {
                fatal("bad pointer in index");
            }
            *++e = ADD;
            ty = t - PTR;
            load();
        } else if (tk == '.') {
            next();
            if (ty <= INT64 || ty >= PTR) { fatal("not a struct"); }
            member();
        } else if (tk == Arrow) {
            next();
            if (ty < PTR) { fatal("not a pointer"); }
            ty = ty - PTR;
            if (ty <= INT64 || ty >= PTR) { fatal("not a struct pointer"); }
            member();
        }
    }
    // precedence climbing
    while (tk >= lev) {
        t = ty;
        switch (tk) {
            case Comma:
                *++e = PSH;  // push left operand
                next();
                expression(Comma);  // parse right operand
                *++e = ADJ;
                *++e = 1;  // discard left operand
                break;
            case Assign:
                next();
                if (*e == LC || *e == LI32 || *e == LI) {
                    *e = PSH;
                } else {
                    fatal("bad lvalue in assignment");
                }
                expression(Assign);
                ty = t;
                store();
                break;
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
            case SubAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(SubAssign);
                ty = t;
                *++e = SUB;
                store();
                break;
            case MulAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(MulAssign);
                ty = t;
                *++e = MUL;
                store();
                break;
            case DivAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(DivAssign);
                ty = t;
                *++e = DIV;
                store();
                break;
            case ModAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(ModAssign);
                ty = t;
                *++e = MOD;
                store();
                break;
            case AndAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(AndAssign);
                ty = t;
                *++e = AND;
                store();
                break;
            case OrAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(OrAssign);
                ty = t;
                *++e = OR;
                store();
                break;
            case XorAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(XorAssign);
                ty = t;
                *++e = XOR;
                store();
                break;
            case ShlAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(ShlAssign);
                ty = t;
                *++e = SHL;
                store();
                break;
            case ShrAssign:
                next();
                if (*e == LC) { *e = PSH; *++e = LC;
                } else if (*e == LI32) { *e = PSH; *++e = LI32;
                } else if (*e == LI) { *e = PSH; *++e = LI;
                } else { fatal("bad lvalue in compound assignment"); }
                *++e = PSH;
                expression(ShrAssign);
                ty = t;
                *++e = SHR;
                store();
                break;
            case Cond:
                next();
                *++e = BZ;
                d = ++e;
                expression(Assign);
                expect(':', "conditional missing colon");
                *d = (int64_t)(e + 3);
                *++e = JMP;
                d = ++e;
                expression(Cond);
                *d = (int64_t)(e + 1);
                break;
            case Lor:
                next();
                *++e = BNZ;
                d = ++e;
                expression(Lan);
                *d = (int64_t)(e + 1);
                ty = INT64;
                break;
            case Lan:
                next();
                *++e = BZ;
                d = ++e;
                expression(Or);
                *d = (int64_t)(e + 1);
                ty = INT64;
                break;
            case Or:  binop(OR, Xor);   break;
            case Xor: binop(XOR, And);  break;
            case And: binop(AND, Eq);   break;
            case Eq:  binop(EQ, Lt);    break;
            case Ne:  binop(NE, Lt);    break;
            case Lt:  binop(LT, Shl);   break;
            case Gt:  binop(GT, Shl);   break;
            case Le:  binop(LE, Shl);   break;
            case Ge:  binop(GE, Shl);   break;
            case Shl: binop(SHL, Add);  break;
            case Shr: binop(SHR, Add);  break;
            case Add: add(t);           break;
            case Sub: sub(t);           break;
            case Mul: binop(MUL, Inc);  break;
            case Div: binop(DIV, Inc);  break;
            case Mod: binop(MOD, Inc);  break;
            case Inc: postinc(Inc); break;
            case Dec: postinc(Dec); break;
            case Brak:
                next();
                *++e = PSH;
                expression(Assign);
                expect(']', "close bracket expected");
                if (t > PTR) {
                    *++e = PSH;
                    *++e = IMM;
                    emit_size(t - PTR);
                    *++e = MUL;
                } else if (t < PTR) {
                    fatal("pointer type expected");
                }
                *++e = ADD;
                ty = t - PTR;
                load();
                break;
            default:
                fatal("compiler error");
        }
    }
}

void statement() {
    int64_t *a, *b, *d;
    if (tk == If) {
        next();
        if_stmt();
    } else if (tk == While) {
        next();
        while_stmt();
    } else if (tk == For) {
        next();
        expect('(', "open paren expected");
        int64_t for_scope_mark = scope_sp;
        skip_const();
        int64_t bt;
        if (tk == Int || tk == Bool || tk == Int32_t || tk == Int64_t ||
            tk == Char || tk == Struct || tk == Union ||
            (tk == Id && id[Class] == Tdef)) {
            bt = INT64;
            if (tk == Int || tk == Bool) { next();
            } else if (tk == Int32_t) { next(); bt = INT32;
            } else if (tk == Int64_t) { next(); bt = INT64;
            } else if (tk == Char) { next(); bt = CHAR;
            } else if (tk == Struct || tk == Union) {
                next();
                if (tk != Id || id[Class] != Struct) { fatal("bad type"); }
                bt = id[Type];
                next();
            } else if (tk == Id && id[Class] == Tdef) {
                bt = id[Type]; next();
            }
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            require(Id, "bad for declaration");
            scope_stack[scope_sp] = (int64_t)id;
            scope_stack[scope_sp + 1] = id[Class];
            scope_stack[scope_sp + 2] = id[Type];
            scope_stack[scope_sp + 3] = id[Val];
            scope_stack[scope_sp + 4] = id[Extent];
            scope_sp = scope_sp + 5;
            id[Class] = Loc;
            id[Type] = ty;
            id[Extent] = 0;
            ++i;
            id[Val] = i;
            next();
            if (tk == Assign) {
                d = id;
                next();
                if (d[Type] > INT64 && d[Type] < FNPTR) {
                    expression(Assign);
                    int64_t sty = d[Type] - INT64 - 1;
                    int64_t ssz = ((int64_t *)struct_syms[sty])[Val];
                    *++e = PSH;
                    *++e = LEA;
                    *++e = loc - d[Val];
                    *++e = PSH;
                    *++e = SWAP;
                    *++e = IMM;
                    *++e = ssz;
                    *++e = PSH;
                    *++e = MCPY;
                    *++e = ADJ;
                    *++e = 3;
                } else {
                    *++e = LEA;
                    *++e = loc - d[Val];
                    *++e = PSH;
                    expression(Assign);
                    if (d[Type] == CHAR) {
                        *++e = SC;
                    } else if (d[Type] == INT32) {
                        *++e = SI32;
                    } else {
                        *++e = SI;
                    }
                }
            }
            if (tk == ';') {
                next();
            } else {
                fatal("semicolon expected");
            }
        } else if (tk != ';') {
            expression(Comma);
            if (tk == ';') {
                next();
            } else {
                fatal("semicolon expected");
            }
        } else {
            next();
        }
        a = e + 1;
        if (tk != ';') {
            expression(Assign);
        } else {
            *++e = IMM;
            *++e = 1;
        }
        if (tk == ';') {
            next();
        } else {
            fatal("semicolon expected");
        }
        *++e = BZ;
        b = ++e;
        int64_t *inc_e = e + 1;
        int64_t inc_buf[128], inc_len;
        if (tk != ')') {
            expression(Assign);
            inc_len = e - inc_e + 1;
            if (inc_len > 128) {
                fatal("for increment too complex");
            }
            memcpy(inc_buf, inc_e, inc_len * 8);
            e = inc_e - 1;
        } else {
            inc_len = 0;
        }
        expect(')', "close paren expected");
        int64_t saved_brk_sp = brk_sp, saved_cnt_sp = cnt_sp;
        statement();
        int64_t inc_top = (int64_t)(e + 1);
        if (inc_len) {
            memcpy(e + 1, inc_buf, inc_len * 8);
            e = e + inc_len;
        }
        *++e = JMP;
        *++e = (int64_t)a;
        *b = (int64_t)(e + 1);
        int64_t j = saved_cnt_sp;
        while (j < cnt_sp) {
            *(int64_t *)cnt_patches[j] = inc_top; j = j + 1;
        }
        j = saved_brk_sp;
        while (j < brk_sp) {
            *(int64_t *)brk_patches[j] = (int64_t)(e + 1); j = j + 1;
        }
        cnt_sp = saved_cnt_sp;
        brk_sp = saved_brk_sp;
        while (scope_sp > for_scope_mark) {
            scope_sp = scope_sp - 5;
            d = (int64_t *)scope_stack[scope_sp];
            d[Class] = scope_stack[scope_sp + 1];
            d[Type] = scope_stack[scope_sp + 2];
            d[Val] = scope_stack[scope_sp + 3];
            d[Extent] = scope_stack[scope_sp + 4];
        }
    } else if (tk == Do) {
        next();
        do_while_stmt();
    } else if (tk == Switch) {
        next();
        switch_stmt();
    } else if (tk == Return) {
        next();
        return_stmt();
    } else if (tk == '{') {
                next();
        int64_t mark = scope_sp;
        skip_const();
        int64_t bt, sz;
                while (tk == Int || tk == Bool || tk == Int32_t || tk == Int64_t ||
               tk == Char || tk == Struct || tk == Union ||
               (tk == Id && id[Class] == Tdef)) {
                        if (tk == Struct || tk == Union) {
                next();
                require(Id, "bad struct/union type");
                if (id[Class] == Struct) {
                    bt = id[Type];
                } else {
                    fatal("not a struct/union");
                }
                next();
            } else if (tk == Id && id[Class] == Tdef) {
                bt = id[Type];
                next();
            } else {
                bt = INT64;
                if (tk == Int || tk == Bool) {
                    next();
                } else if (tk == Int32_t) {
                    next();
                    bt = INT32;
                } else if (tk == Int64_t) {
                    next();
                    bt = INT64;
                } else if (tk == Char) {
                    next();
                    bt = CHAR;
                } else {
                    next();
                }
            }
            while (tk != ';') {
                ty = bt;
                int64_t is_fnptr = 0;
                while (tk == Mul) {
                    next();
                    ty = ty + PTR;
                }
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
                scope_stack[scope_sp] = (int64_t)id;
                scope_stack[scope_sp + 1] = id[Class];
                scope_stack[scope_sp + 2] = id[Type];
                scope_stack[scope_sp + 3] = id[Val];
                scope_stack[scope_sp + 4] = id[Extent];
                scope_sp = scope_sp + 5;
                id[Class] = Loc;
                id[Type] = ty;
                id[Extent] = 0;
                next();
                if (tk == Brak) {
                    next();
                    require(Num, "bad array size");
                    id[Extent] = ival;
                    next();
                    expect(']', "close bracket expected");
                    if (ty == CHAR) {
                        i = i + (ival + 7) / 8;
                    } else if (ty == INT32) {
                        i = i + (ival * 4 + 7) / 8;
                    } else if (ty > INT64 && ty < FNPTR) {
                        sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                        i = i + (ival * sz + 7) / 8;
                    } else {
                        i = i + ival;
                    }
                    id[Type] = ty + PTR;
                } else if (ty > INT64 && ty < FNPTR) {
                    sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                    i = i + (sz + 7) / 8;
                } else {
                    ++i;
                }
                id[Val] = i;
                if (tk == Assign) {
                    d = id;
                    next();
                    if (d[Type] > INT64 && d[Type] < FNPTR) {
                        expression(Assign); // struct init: eval RHS first
                        int64_t sty = d[Type] - INT64 - 1;
                        int64_t ssz = ((int64_t *)struct_syms[sty])[Val];
                        *++e = PSH;           // push src
                        *++e = LEA;
                        *++e = loc - d[Val];  // dest address
                        *++e = PSH;           // push dest
                        *++e = SWAP;          // now: dest, src
                        *++e = IMM;
                        *++e = ssz;
                        *++e = PSH;           // push size
                        *++e = MCPY;
                        *++e = ADJ;
                        *++e = 3;
                    } else {
                        *++e = LEA;
                        *++e = loc - d[Val];
                        *++e = PSH;
                        expression(Assign);
                        if (d[Type] == CHAR) {
                            *++e = SC;
                        } else if (d[Type] == INT32) {
                            *++e = SI32;
                        } else {
                            *++e = SI;
                        }
                    }
                }
                skip_comma();
            }
            next();
            skip_const();
        }
        while (tk != '}') {
            statement();
        }
        while (scope_sp > mark) {
            scope_sp = scope_sp - 5;
            id = (int64_t *)scope_stack[scope_sp];
            id[Class] = scope_stack[scope_sp + 1];
            id[Type] = scope_stack[scope_sp + 2];
            id[Val] = scope_stack[scope_sp + 3];
            id[Extent] = scope_stack[scope_sp + 4];
        }
        next();
    } else if (tk == Break) {
        next();
        expect(';', "semicolon expected");
        *++e = JMP;
        *++e = 0;
        brk_patches[brk_sp] = (int64_t)(e);
        brk_sp = brk_sp + 1;
    } else if (tk == Continue) {
        next();
        expect(';', "semicolon expected");
        *++e = JMP;
        *++e = 0;
        cnt_patches[cnt_sp] = (int64_t)(e);
        cnt_sp = cnt_sp + 1;
    } else if (tk == ';') {
        next();
    } else if (tk == Static || tk == Const || tk == Int || tk == Bool ||
               tk == Int32_t || tk == Int64_t || tk == Char || tk == Struct ||
               tk == Union || (tk == Id && id[Class] == Tdef)) {
        // mid-block declaration (possibly static)
                int64_t is_static = 0;
        if (tk == Static) { is_static = 1; next(); }
        skip_const();
        int64_t bt;
        if (tk == Struct || tk == Union) {
            next();
            if (tk != Id || id[Class] != Struct) { fatal("bad struct/union"); }
            bt = id[Type];
            next();
        } else if (tk == Id && id[Class] == Tdef) {
            bt = id[Type];
            next();
        } else {
            bt = INT64;
            if (tk == Int || tk == Bool) { next();
            } else if (tk == Int32_t) { next(); bt = INT32;
            } else if (tk == Int64_t) { next();
            } else if (tk == Char) { next(); bt = CHAR;
            }
        }
        while (tk != ';') {
            ty = bt;
            int64_t is_fnptr = 0;
            while (tk == Mul) { next(); ty = ty + PTR; }
            // Check for function pointer declarator: (*name)(params)
            if (tk == '(') {
                                next();
                expect(Mul, "* expected in fnptr decl");
                require(Id, "bad fnptr decl name");
                id[Type] = FNPTR;
                ty = FNPTR;
                next();
                expect(')', ") expected in fnptr decl");
                expect('(', "( expected for fnptr params");
                while (tk != ')') { next(); }
                expect(')', ") expected");
                is_fnptr = 1;
            }
            if (!is_fnptr) { require(Id, "bad local declaration"); }
            scope_stack[scope_sp] = (int64_t)id;
            scope_stack[scope_sp + 1] = id[Class];
            scope_stack[scope_sp + 2] = id[Type];
            scope_stack[scope_sp + 3] = id[Val];
            scope_stack[scope_sp + 4] = id[Extent];
            scope_sp = scope_sp + 5;
            if (is_static) {
                // static local: allocate in global data segment
                id[Class] = Glo;
                id[Type] = ty;
                id[Extent] = 0;
                id[Val] = (int64_t)data;
                if (!is_fnptr) { next(); }
                if (tk == Brak) {
                    next();
                    int64_t arr_size = 0;
                    if (tk == Num) { arr_size = ival; next(); }
                    expect(']', "close bracket expected");
                    id[Type] = ty + PTR;
                    if (tk == Assign) {
                        next();
                        expect('{', "{ expected for array init");
                        int64_t count = 0;
                        char *arr_start = data;
                        while (tk != '}') {
                            int64_t neg = 1;
                            if (tk == Sub) { neg = -1; next(); }
                            if (tk == Num) {
                                if (ty == CHAR) {
                                    *(char *)data = (char)(neg * ival);
                                    data = data + 1;
                                } else if (ty == INT32) {
                                    *(int32_t *)data = (int32_t)(neg * ival);
                                    data = data + 4;
                                } else {
                                    *(int64_t *)data = neg * ival;
                                    data = data + sizeof(int64_t);
                                }
                                next();
                            } else {
                                fatal("constant expected in static array");
                            }
                            count++;
                            skip_comma();
                        }
                        next();
                        if (arr_size == 0) { arr_size = count; }
                        id[Extent] = arr_size;
                        int64_t elem_sz = 1;
                        if (ty == INT32) { elem_sz = 4;
                        } else if (ty == INT64 || ty >= PTR) { elem_sz = 8;
                        }
                        int64_t total = arr_size * elem_sz;
                        int64_t used = data - arr_start;
                        while (used < total) { *(char *)data++ = 0; used++; }
                    } else {
                        if (arr_size == 0) { fatal("array size required"); }
                        id[Extent] = arr_size;
                        if (ty == CHAR) {
                            data = data + arr_size;
                        } else if (ty == INT32) {
                            data = data + arr_size * 4;
                        } else if (ty > INT64 && ty < FNPTR) {
                            int64_t sz;
                            sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                            data = data + arr_size * sz;
                        } else {
                            data = data + arr_size * sizeof(int64_t);
                        }
                    }
                } else {
                    // static scalar - initialize to 0 (done by data segment)
                    int64_t init_val = 0;
                    if (tk == Assign) {
                        next();
                        int64_t neg = 1;
                        if (tk == Sub) { neg = -1; next(); }
                        if (tk == Num) { init_val = neg * ival; next();
                        } else if (tk == '"') {
                            init_val = ival; next();
                            id[Val] = (int64_t)data;
                        } else { fatal("static local: constant initializer");
                        }
                    }
                    if (ty == CHAR) {
                        *(char *)data = (char)init_val;
                        data = data + sizeof(char);
                    } else if (ty == INT32) {
                        *(int32_t *)data = (int32_t)init_val;
                        data = data + 4;
                    } else if (ty > INT64 && ty < FNPTR) {
                        int64_t sz;
                        sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                        data = data + sz;
                    } else {
                        *(int64_t *)data = init_val;
                        data = data + sizeof(int64_t);
                    }
                }
            } else {
                // regular local variable
                id[Class] = Loc;
                id[Type] = ty;
                id[Extent] = 0;
                if (!is_fnptr) { next(); }
                if (tk == Brak) {
                    next();
                    int64_t arr_size = 0;
                    if (tk == Num) { arr_size = ival; next(); }
                    expect(']', "close bracket expected");
                    id[Type] = ty + PTR;
                    int64_t elem_sz = 1;
                    if (ty == INT32) { elem_sz = 4;
                    } else if (ty == INT64 || ty >= PTR) { elem_sz = 8;
                    } else if (ty > INT64 && ty < FNPTR) {
                        elem_sz = ((int64_t *)struct_syms[ty-INT64-1])[Val];
                    }
                    if (tk == Assign) {
                        next();
                        expect('{', "{ expected for array init");
                        int64_t init_vals[256];
                        int64_t count = 0;
                        while (tk != '}' && count < 256) {
                            int64_t neg = 1;
                            if (tk == Sub) { neg = -1; next(); }
                            require(Num, "constant expected");
                            init_vals[count++] = neg * ival;
                            next();
                            skip_comma();
                        }
                        next();
                        if (arr_size == 0) { arr_size = count; }
                        id[Extent] = arr_size;
                        if (ty == CHAR) {
                            i = i + (arr_size + 7) / 8;
                        } else if (ty == INT32) {
                            i = i + (arr_size * 4 + 7) / 8;
                        } else {
                            i = i + arr_size;
                        }
                        id[Val] = i;
                        // now emit init code
                        int64_t j;
                        j = 0;
                        while (j < count) {
                            *++e = LEA;
                            *++e = loc - id[Val];
                            if (ty == CHAR) {
                                *++e = PSH; *++e = IMM; *++e = j; *++e = ADD;
                            } else if (ty == INT32) {
                                *++e = PSH; *++e = IMM; *++e = j * 4; *++e = ADD;
                            } else {
                                *++e = PSH; *++e = IMM; *++e = j * 8; *++e = ADD;
                            }
                            *++e = PSH;
                            *++e = IMM;
                            *++e = init_vals[j];
                            if (ty == CHAR) { *++e = SC;
                            } else if (ty == INT32) { *++e = SI32;
                            } else { *++e = SI;
                            }
                            j++;
                        }
                    } else {
                        if (arr_size == 0) { fatal("array size required"); }
                        id[Extent] = arr_size;
                        if (ty == CHAR) {
                            i = i + (arr_size + 7) / 8;
                        } else if (ty == INT32) {
                            i = i + (arr_size * 4 + 7) / 8;
                        } else if (ty > INT64 && ty < FNPTR) {
                            int64_t sz;
                            sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                            i = i + (arr_size * sz + 7) / 8;
                        } else {
                            i = i + arr_size;
                        }
                        id[Val] = i;
                    }
                } else if (ty > INT64 && ty < FNPTR) {
                    int64_t sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                    i = i + (sz + 7) / 8;
                } else {
                    ++i;
                }
                id[Val] = i;
                if (tk == Assign) {
                    d = id;
                    next();
                    if (d[Type] > INT64 && d[Type] < FNPTR) {
                        expression(Assign);
                        int64_t sty = d[Type] - INT64 - 1;
                        int64_t ssz = ((int64_t *)struct_syms[sty])[Val];
                        *++e = PSH;
                        *++e = LEA;
                        *++e = loc - d[Val];
                        *++e = PSH;
                        *++e = SWAP;
                        *++e = IMM;
                        *++e = ssz;
                        *++e = PSH;
                        *++e = MCPY;
                        *++e = ADJ;
                        *++e = 3;
                    } else {
                        *++e = LEA;
                        *++e = loc - d[Val];
                        *++e = PSH;
                        expression(Assign);
                        if (d[Type] == CHAR) { *++e = SC;
                        } else if (d[Type] == INT32) { *++e = SI32;
                        } else { *++e = SI;
                        }
                    }
                }
            }
            skip_comma();
        }
        next();
    } else {
        expression(Assign);
        expect(';', "semicolon expected");
    }
}

char* mem_read(const char* filename) {
    char* return_ptr = 0;
    int file_descriptor = open(filename, O_RDONLY);
    
    if (file_descriptor != -1) {
        int64_t file_size = lseek(file_descriptor, 0, 2); // SEEK_END
        lseek(file_descriptor, 0, 0); // SEEK_SET
        
        int64_t total_size = file_size + 16;
        
        char* mapped_memory = (char*)mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        
        if (mapped_memory != MAP_FAILED) {
            int64_t* metadata = (int64_t*)mapped_memory;
            metadata[0] = (int64_t)file_descriptor;
            metadata[1] = total_size;
            
            read(file_descriptor, mapped_memory + 16, file_size);
            return_ptr = (char*)(mapped_memory + 16);
        }
        
        if (mapped_memory == MAP_FAILED) {
            close(file_descriptor);
        }
    }
    
    return return_ptr;
}

void mem_close(char* pointer_to_memread_file) {
    if (pointer_to_memread_file != 0) {
        char* base_pointer = (char*)pointer_to_memread_file - 16;
        int64_t* metadata = (int64_t*)base_pointer;
        
        int file_descriptor = (int)metadata[0];
        int64_t total_size = metadata[1];
        
        close(file_descriptor);
        munmap(base_pointer, total_size);
    }
}

bool mem_write(const char* filename, const char* data, int64_t bytes) {
    bool success_flag = false;
    char temporary_name[1024];
    strcpy(temporary_name, filename);
    strcat(temporary_name, ".tmp");
    
    int file_descriptor = open(temporary_name, O_CREAT | O_RDWR | O_TRUNC, 0644);
    
    if (file_descriptor != -1) {
        int truncate_result = ftruncate(file_descriptor, bytes);
        
        if (truncate_result != -1) {
            char* mapped_memory = (char*)mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
            
            if (mapped_memory != MAP_FAILED) {
                memcpy(mapped_memory, data, bytes);
                msync(mapped_memory, bytes, MS_SYNC);
                munmap(mapped_memory, bytes);
                
                int rename_result = rename(temporary_name, filename);
                if (rename_result == 0) {
                    success_flag = true;
                }
            }
        }
        
        close(file_descriptor);
    }
    
    return success_flag;
}

// preprocessor helpers
int pp_streq(char *a, int alen, char *b, int blen) {
    if (alen != blen) { return 0; }
    int j = 0;
    while (j < alen) { if (a[j] != b[j]) { return 0; } j++; }
    return 1;
}

int pp_find(char *name, int len) {
    int j = 0;
    while (j < pp_count) {
        if (pp_streq(name, len, pp_names[j], pp_nlen[j])) { return j; }
        j++;
    }
    return -1;
}

void pp_define(char *name, int nlen, char *val, int vlen, int pcount) {
    int idx = pp_find(name, nlen);
    if (idx < 0) { idx = pp_count++; }
    pp_names[idx] = name;
    pp_nlen[idx] = nlen;
    pp_values[idx] = val;
    pp_vlen[idx] = vlen;
    pp_pcount[idx] = pcount;
}

void pp_undef(char *name, int nlen) {
    int idx = pp_find(name, nlen);
    if (idx >= 0) {
        pp_count--;
        while (idx < pp_count) {
            pp_names[idx] = pp_names[idx + 1];
            pp_nlen[idx] = pp_nlen[idx + 1];
            pp_values[idx] = pp_values[idx + 1];
            pp_vlen[idx] = pp_vlen[idx + 1];
            idx++;
        }
    }
}

int pp_is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

int pp_is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int pp_skip_ws(char *s) {
    int n = 0;
    while (s[n] == ' ' || s[n] == '\t') { n++; }
    return n;
}

int pp_get_ident(char *s, int *len) {
    int n = pp_skip_ws(s);
    if (!pp_is_ident_start(s[n])) { *len = 0; return n; }
    int start = n;
    while (pp_is_ident_char(s[n])) { n++; }
    *len = n - start;
    return start;
}

int pp_active() {
    if (pp_cond_sp == 0) { return 1; }
    return pp_cond[pp_cond_sp - 1];
}

int pp_was_included(char *file) {
    int j = 0;
    while (j < pp_once_count) {
        if (strcmp(pp_once[j], file) == 0) { return 1; }
        j++;
    }
    return 0;
}

char *preprocess(char *src, int srclen, char *out, char *filename, int depth);

char *pp_include(char *out, char *file, int depth) {
    if (depth > 16) {
        printf("pp: include depth exceeded\n");
        return 0;
    }
    if (pp_was_included(file)) { return out; }
    int fd = open(file, 0);
    if (fd < 0) {
        printf("pp: could not open %s\n", file);
        return 0;
    }
    // read in 64K chunks, grow as needed
    int bufsz = 65536;
    char *buf = malloc(bufsz);
    if (!buf) { close(fd); return 0; }
    int total = 0;
    int n;
    while ((n = read(fd, buf + total, bufsz - total - 1)) > 0) {
        total = total + n;
        if (total >= bufsz - 1024) {
            int newsz = bufsz * 2;
            char *newbuf = malloc(newsz);
            if (!newbuf) { free(buf); close(fd); return 0; }
            memcpy(newbuf, buf, total);
            free(buf);
            buf = newbuf;
            bufsz = newsz;
        }
    }
    close(fd);
    if (total <= 0) { free(buf); return out; }
    buf[total] = 0;
    char *result = preprocess(buf, total, out, file, depth + 1);
    free(buf);
    return result;
}

char *pp_embed(char *out, char *file, int add_null) {
    int fd = open(file, 0);
    if (fd < 0) {
        printf("pp: could not embed %s\n", file);
        return 0;
    }
    int bufsz = 65536;
    char *buf = malloc(bufsz);
    if (!buf) { close(fd); return 0; }
    int total = 0;
    int n;
    while ((n = read(fd, buf + total, bufsz - total - 1)) > 0) {
        total = total + n;
        if (total >= bufsz - 1024) {
            int newsz = bufsz * 2;
            char *newbuf = malloc(newsz);
            if (!newbuf) { free(buf); close(fd); return 0; }
            memcpy(newbuf, buf, total);
            free(buf);
            buf = newbuf;
            bufsz = newsz;
        }
    }
    close(fd);
    if (total < 0) { free(buf); return 0; }
    int j = 0;
    while (j < total) {
        int v = buf[j] & 0xFF;
        if (j > 0) { *out++ = ','; *out++ = ' '; }
        if (v >= 100) { *out++ = '0' + (v / 100); }
        if (v >= 10) { *out++ = '0' + ((v / 10) % 10); }
        *out++ = '0' + (v % 10);
        j++;
    }
    if (add_null) {
        if (total > 0) { *out++ = ','; *out++ = ' '; }
        *out++ = '0';
    }
    free(buf);
    return out;
}

// OS detection: set at startup based on runtime check
int64_t pp_os_is_linux;
int64_t pp_os_is_apple;
int64_t pp_os_is_windows;

int pp_os_check(char *name, int len) {
    if (pp_streq(name, len, "linux", 5)) { return pp_os_is_linux; }
    if (pp_streq(name, len, "apple", 5)) { return pp_os_is_apple; }
    if (pp_streq(name, len, "windows", 7)) { return pp_os_is_windows; }
    return 0;
}

void pp_os_init() {
    pp_os_is_linux = 0;
    pp_os_is_apple = 0;
    pp_os_is_windows = 0;
    // Try to detect OS via uname - if it exists, we're on Linux/Unix
    // Simple heuristic: check if /proc exists (Linux-specific)
    int fd = open("/proc/version", 0);
    if (fd >= 0) {
        pp_os_is_linux = 1;
        close(fd);
        return;
    }
    // Check for macOS
    fd = open("/System/Library", 0);
    if (fd >= 0) {
        pp_os_is_apple = 1;
        close(fd);
        return;
    }
    // Otherwise assume Linux for now (most common case for cx)
    pp_os_is_linux = 1;
}

char *preprocess(char *src, int srclen, char *out, char *filename, int depth) {
    // strip /* */ block comments in-place, preserving newlines
    char *r = src;
    char *w = src;
    char *srcend = src + srclen;
    while (r < srcend) {
        if (r[0] == '/' && r + 1 < srcend && r[1] == '*') {
            r = r + 2;
            while (r < srcend && !(r[0] == '*' && r + 1 < srcend && r[1] == '/')) {
                if (*r == '\n') { *w++ = '\n'; }
                r++;
            }
            if (r < srcend) { r = r + 2; }
        } else if (*r == '"' || *r == '\'') {
            char quote = *r;
            *w++ = *r++;
            while (r < srcend && *r != quote) {
                if (*r == '\\' && r + 1 < srcend) { *w++ = *r++; }
                *w++ = *r++;
            }
            if (r < srcend) { *w++ = *r++; }
        } else {
            *w++ = *r++;
        }
    }
    srclen = w - src;
    *w = 0;
    char *s = src;
    char *end = src + srclen;
    int pp_line_local = 1;
    int pragma_once = 0;
    // emit initial #line
    char *o = out;
    *o++ = '#'; *o++ = 'l'; *o++ = 'i'; *o++ = 'n'; *o++ = 'e';
    *o++ = ' '; *o++ = '1'; *o++ = ' '; *o++ = '"';
    char *fn = filename;
    while (*fn) { *o++ = *fn++; }
    *o++ = '"'; *o++ = '\n';
    while (s < end) {
        char *line_start = s;
        // find end of line
        char *eol = s;
        while (eol < end && *eol != '\n') { eol++; }
        int linelen = eol - s;
        // skip leading whitespace
        int ws = pp_skip_ws(s);
        if (s[ws] == '#') {
            // directive
            int pos = ws + 1;
            pos = pos + pp_skip_ws(s + pos);
            // parse directive name
            int dlen;
            int dstart = pos + pp_get_ident(s + pos, &dlen);
            pos = dstart + dlen;
            if (pp_streq(s + dstart, dlen, "define", 6) && pp_active()) {
                pos = pos + pp_skip_ws(s + pos);
                int nlen;
                int nstart = pos + pp_get_ident(s + pos, &nlen);
                pos = nstart + nlen;
                int pcount = -1; // -1 = object-like macro
                int macro_idx = pp_find(s + nstart, nlen);
                if (macro_idx < 0) { macro_idx = pp_count; }
                if (s[pos] == '(') {
                    // function-like macro
                    pcount = 0;
                    pos++;
                    while (s[pos] != ')' && pos < linelen) {
                        pos = pos + pp_skip_ws(s + pos);
                        if (s[pos] == ')') { break; }
                        int plen;
                        int pstart = pos + pp_get_ident(s + pos, &plen);
                        if (plen > 0 && plen < 16 && pcount < 8) {
                            int pi = 0;
                            int pbase = macro_idx * 128 + pcount * 16;
                            while (pi < plen) {
                                pp_params[pbase + pi] = s[pstart + pi];
                                pi++;
                            }
                            pp_params[pbase + pi] = 0;
                            pcount++;
                        }
                        pos = pstart + plen;
                        pos = pos + pp_skip_ws(s + pos);
                        if (s[pos] == ',') { pos++; }
                    }
                    if (s[pos] == ')') { pos++; }
                }
                pos = pos + pp_skip_ws(s + pos);
                int vstart = pos;
                int vlen = linelen - vstart;
                while (vlen > 0 && (s[vstart + vlen - 1] == ' ' ||
                       s[vstart + vlen - 1] == '\t' ||
                       s[vstart + vlen - 1] == '\r')) { vlen--; }
                pp_define(s + nstart, nlen, s + vstart, vlen, pcount);
            } else if (pp_streq(s + dstart, dlen, "undef", 5) && pp_active()) {
                pos = pos + pp_skip_ws(s + pos);
                int nlen;
                int nstart = pos + pp_get_ident(s + pos, &nlen);
                pp_undef(s + nstart, nlen);
            } else if (pp_streq(s + dstart, dlen, "ifdef", 5)) {
                pos = pos + pp_skip_ws(s + pos);
                int nlen;
                int nstart = pos + pp_get_ident(s + pos, &nlen);
                int defined = pp_find(s + nstart, nlen) >= 0;
                int parent_active = pp_active();
                pp_cond[pp_cond_sp++] = parent_active && defined;
            } else if (pp_streq(s + dstart, dlen, "ifndef", 6)) {
                pos = pos + pp_skip_ws(s + pos);
                int nlen;
                int nstart = pos + pp_get_ident(s + pos, &nlen);
                int defined = pp_find(s + nstart, nlen) >= 0;
                int parent_active = pp_active();
                pp_cond[pp_cond_sp++] = parent_active && !defined;
            } else if (pp_streq(s + dstart, dlen, "if", 2)) {
                pos = pos + pp_skip_ws(s + pos);
                int cond_result = 0;
                // check for os(name)
                if (pp_streq(s + pos, 2, "os", 2) && s[pos + 2] == '(') {
                    int pstart = pos + 3;
                    int pend = pstart;
                    while (pend < linelen && s[pend] != ')') { pend++; }
                    cond_result = pp_os_check(s + pstart, pend - pstart);
                } else if (s[pos] == '0') {
                    cond_result = 0;
                } else if (s[pos] == '1') {
                    cond_result = 1;
                }
                int parent_active = pp_active();
                pp_cond[pp_cond_sp++] = parent_active && cond_result;
            } else if (pp_streq(s + dstart, dlen, "else", 4)) {
                if (pp_cond_sp > 0) {
                    int parent = pp_cond_sp > 1 ? pp_cond[pp_cond_sp - 2] : 1;
                    pp_cond[pp_cond_sp - 1] = parent && !pp_cond[pp_cond_sp - 1];
                }
            } else if (pp_streq(s + dstart, dlen, "elif", 4)) {
                if (pp_cond_sp > 0 && !pp_cond[pp_cond_sp - 1]) {
                    pos = pos + pp_skip_ws(s + pos);
                    int cond_result = 0;
                    if (pp_streq(s + pos, 2, "os", 2) && s[pos + 2] == '(') {
                        int pstart = pos + 3;
                        int pend = pstart;
                        while (pend < linelen && s[pend] != ')') { pend++; }
                        cond_result = pp_os_check(s + pstart, pend - pstart);
                    } else if (s[pos] == '1') {
                        cond_result = 1;
                    }
                    int parent = pp_cond_sp > 1 ? pp_cond[pp_cond_sp - 2] : 1;
                    pp_cond[pp_cond_sp - 1] = parent && cond_result;
                }
            } else if (pp_streq(s + dstart, dlen, "endif", 5)) {
                if (pp_cond_sp > 0) { pp_cond_sp--; }
            } else if (pp_streq(s + dstart, dlen, "pragma", 6) && pp_active()) {
                pos = pos + pp_skip_ws(s + pos);
                int plen;
                int pstart = pos + pp_get_ident(s + pos, &plen);
                if (pp_streq(s + pstart, plen, "once", 4)) {
                    pragma_once = 1;
                    if (pp_once_count < 256) {
                        pp_once[pp_once_count++] = filename;
                    }
                }
            } else if (pp_streq(s + dstart, dlen, "include", 7) && pp_active()) {
                pos = pos + pp_skip_ws(s + pos);
                char delim = s[pos];
                if (delim == '"' || delim == '<') {
                    char enddelim = (delim == '<') ? '>' : '"';
                    int fstart = pos + 1;
                    int fend = fstart;
                    while (fend < linelen && s[fend] != enddelim) { fend++; }
                    int flen = fend - fstart;
                    char fnamebuf[256];
                    if (flen < 256) {
                        memcpy(fnamebuf, s + fstart, flen);
                        fnamebuf[flen] = 0;
                        // skip system includes (intrinsics handle stdlib)
                        if (delim == '<') {
                            // skip <stdio.h> etc - intrinsics provide these
                        } else {
                            o = pp_include(o, fnamebuf, depth);
                            if (!o) { return 0; }
                            // emit #line to restore position
                            *o++ = '#'; *o++ = 'l'; *o++ = 'i'; *o++ = 'n';
                            *o++ = 'e'; *o++ = ' ';
                            int ln = pp_line_local + 1;
                            if (ln >= 10000) { *o++ = '0' + (ln / 10000) % 10; }
                            if (ln >= 1000) { *o++ = '0' + (ln / 1000) % 10; }
                            if (ln >= 100) { *o++ = '0' + (ln / 100) % 10; }
                            if (ln >= 10) { *o++ = '0' + (ln / 10) % 10; }
                            *o++ = '0' + ln % 10;
                            *o++ = ' '; *o++ = '"';
                            fn = filename;
                            while (*fn) { *o++ = *fn++; }
                            *o++ = '"'; *o++ = '\n';
                        }
                    }
                }
            } else if (pp_streq(s + dstart, dlen, "embed", 5) && pp_active()) {
                pos = pos + pp_skip_ws(s + pos);
                if (s[pos] == '"') {
                    int fstart = pos + 1;
                    int fend = fstart;
                    while (fend < linelen && s[fend] != '"') { fend++; }
                    int flen = fend - fstart;
                    char fnamebuf[256];
                    if (flen < 256) {
                        memcpy(fnamebuf, s + fstart, flen);
                        fnamebuf[flen] = 0;
                        // check for trailing , 0 for null terminator
                        int add_null = 0;
                        int after = fend + 1;
                        after = after + pp_skip_ws(s + after);
                        if (s[after] == ',') {
                            after++;
                            after = after + pp_skip_ws(s + after);
                            if (s[after] == '0') { add_null = 1; }
                        }
                        o = pp_embed(o, fnamebuf, add_null);
                        if (!o) { return 0; }
                    }
                }
            } else if (pp_streq(s + dstart, dlen, "line", 4)) {
                // #line directive - pass through
                char *c = s;
                while (c < eol) { *o++ = *c++; }
                *o++ = '\n';
            }
            // output newline to preserve line count
            *o++ = '\n';
        } else if (pp_active()) {
            // regular line - perform macro substitution
            char *c = s;
            while (c < eol) {
                if (pp_is_ident_start(*c)) {
                    char *istart = c;
                    while (c < eol && pp_is_ident_char(*c)) { c++; }
                    int ilen = c - istart;
                    int idx = pp_find(istart, ilen);
                    if (idx >= 0 && pp_pcount[idx] >= 0 && *c == '(') {
                        // function-like macro - parse arguments
                        c++; // skip '('
                        char *args[8];
                        int arglens[8];
                        int argc = 0;
                        int depth = 1;
                        char *astart = c;
                        while (c < eol && depth > 0) {
                            if (*c == '(') { depth++;
                            } else if (*c == ')') {
                                depth--;
                                if (depth == 0) {
                                    if (argc < 8 && c > astart) {
                                        args[argc] = astart;
                                        arglens[argc] = c - astart;
                                        argc++;
                                    }
                                }
                            } else if (*c == ',' && depth == 1) {
                                if (argc < 8) {
                                    args[argc] = astart;
                                    arglens[argc] = c - astart;
                                    argc++;
                                }
                                astart = c + 1;
                            }
                            c++;
                        }
                        // substitute body with args replacing params
                        char *body = pp_values[idx];
                        int blen = pp_vlen[idx];
                        int bi = 0;
                        while (bi < blen) {
                            if (pp_is_ident_start(body[bi])) {
                                int bstart = bi;
                                while (bi < blen && pp_is_ident_char(body[bi])) { bi++; }
                                int bilen = bi - bstart;
                                int pi = 0;
                                int found = -1;
                                while (pi < pp_pcount[idx]) {
                                    int pbase = idx * 128 + pi * 16;
                                    int plen = 0;
                                    while (pp_params[pbase + plen]) { plen++; }
                                    if (bilen == plen) {
                                        int match = 1;
                                        int k = 0;
                                        while (k < bilen) {
                                            if (body[bstart + k] != pp_params[pbase + k]) {
                                                match = 0;
                                            }
                                            k++;
                                        }
                                        if (match) { found = pi; }
                                    }
                                    pi++;
                                }
                                if (found >= 0 && found < argc) {
                                    int ai = 0;
                                    while (ai < arglens[found]) {
                                        *o++ = args[found][ai++];
                                    }
                                } else {
                                    int k = bstart;
                                    while (k < bi) { *o++ = body[k++]; }
                                }
                            } else {
                                *o++ = body[bi++];
                            }
                        }
                    } else if (idx >= 0 && pp_pcount[idx] < 0 && pp_vlen[idx] > 0) {
                        // object-like macro
                        int j = 0;
                        while (j < pp_vlen[idx]) { *o++ = pp_values[idx][j++]; }
                    } else {
                        // copy identifier as-is
                        while (istart < c) { *o++ = *istart++; }
                    }
                } else if (*c == '"' || *c == '\'') {
                    // string or char literal - copy verbatim
                    char quote = *c;
                    *o++ = *c++;
                    while (c < eol && *c != quote) {
                        if (*c == '\\' && c + 1 < eol) { *o++ = *c++; }
                        *o++ = *c++;
                    }
                    if (c < eol) { *o++ = *c++; }
                } else if (c[0] == '/' && c + 1 < eol && c[1] == '/') {
                    // line comment - skip rest of line
                    break;
                } else {
                    *o++ = *c++;
                }
            }
            *o++ = '\n';
        }
        // advance to next line
        pp_line_local++;
        s = eol;
        if (s < end && *s == '\n') { s++; }
    }
    *o = 0;
    return o;
}

void add_symbol(char *name, int64_t val) {
    p = name;
    next();
    id[Class] = Num;
    id[Type] = INT64;
    id[Val] = val;
}

void init_definitions() {
    // From sys/mman.h
    add_symbol("PROT_READ", 1);
    add_symbol("PROT_WRITE", 2);
    add_symbol("MAP_SHARED", 1);
    add_symbol("MAP_PRIVATE", 2);
    add_symbol("MAP_FAILED", 0xFFFFFFFFFFFFFFFF); // (void*)-1
    add_symbol("MAP_ANON", 0x1000);
    add_symbol("MS_SYNC", 0x10000); // from macOS
    // From fcntl.h
    add_symbol("O_RDONLY", 0);
    add_symbol("O_WRONLY", 1);
    add_symbol("O_RDWR", 2);
    add_symbol("O_CREAT", 0x200);
    add_symbol("O_TRUNC", 0x400);
    // From unistd.h (for lseek)
    add_symbol("SEEK_SET", 0);
    add_symbol("SEEK_CUR", 1);
    add_symbol("SEEK_END", 2);
}

int64_t *compile(char *filename) {
    int64_t fd;
    fd = open(filename, 0);
    if (fd < 0) {
        printf("could not open(%s)\n", filename);
        return 0;
    }
    sym = sym_pool;
    le = e = code_pool;
    if (!(data = malloc(poolsz * 8))) {
        printf("could not malloc(%d) data area\n", (int)(poolsz * 8));
        return 0;
    }
    if (!(src_pool = malloc(srcbufsz))) {
        printf("could not malloc(%d) src_pool area\n", (int)srcbufsz);
        return 0;
    }
    struct_syms = struct_syms_arr;
    memset(sym, 0, poolsz);
    memset(e, 0, poolsz);
    memset(data, 0, poolsz * 8);
    memset(struct_syms, 0, 256 * sizeof(int64_t));
    // keywords
    p = "bool char const else enum if inline int int32_t int64_t return sizeof "
        "static struct typedef union void while for do switch case default "
        "break continue "
        "void true false main";
    i = Bool;
    while (i <= Continue) { next(); id[Tk] = i++; }
    { char *save_p = p; intrinsics(); p = save_p; } // intrinsics
    { char *save_p = p; init_definitions(); p = save_p; } // constants
    next(); id[Tk] = Char; // void type
    next(); id[Class] = Num; id[Type] = INT64; id[Val] = 1; // true
    next(); id[Class] = Num; id[Type] = INT64; id[Val] = 0; // false
    next();
    int64_t *idmain = id;  // track main
    // read source
    char *raw_src = malloc(srcbufsz);
    if (!raw_src) {
        printf("could not malloc raw_src\n");
        return 0;
    }
    i = read(fd, raw_src, srcbufsz - 1);
    if (i <= 0) {
        printf("read() returned %d\n", (int)i);
        return 0;
    }
    raw_src[i] = 0;
    close(fd);
    // preprocess
    pp_count = 0;
    pp_cond_sp = 0;
    pp_once_count = 0;
    pp_os_init();
    // process -D command line defines
    int di = 0;
    while (di < cmdline_def_count) {
        char *def = cmdline_defs[di];
        char *eq = def;
        while (*eq && *eq != '=') { eq++; }
        int nlen = eq - def;
        if (*eq == '=') {
            int vlen = 0;
            char *v = eq + 1;
            while (v[vlen]) { vlen++; }
            pp_define(def, nlen, v, vlen, -1);
        } else {
            pp_define(def, nlen, "1", 1, -1);
        }
        di++;
    }
    char *pp_end = preprocess(raw_src, i, src_pool, filename, 0);
    if (!pp_end) {
        printf("preprocessing failed\n");
        return 0;
    }
    free(raw_src);
    lp = p = src_pool;
    // parse
    line = 1;
    next();
    int64_t bt, ty, sz, *s, *m, *d, *t, j;
    while (tk) {
        while (tk == Const || tk == Static || tk == Inline) { next(); }
        bt = INT64; // basetype
        if (tk == Int || tk == Bool) {
            next();
        } else if (tk == Int32_t) {
            next();
            bt = INT32;
        } else if (tk == Int64_t) {
            next();
            bt = INT64;
        } else if (tk == Char) {
            next();
            bt = CHAR;
        } else if (tk == Id && id[Class] == Tdef) {
            bt = id[Type];
            next();
        } else if (tk == Enum) {
            next();
            if (tk != '{') {
                next();
            }
            if (tk == '{') {
                next();
                i = 0;
                while (tk != '}') {
                    require(Id, "bad enum identifier");
                    next();
                    if (tk == Assign) {
                        next();
                        int64_t neg = 1;
                        if (tk == Sub) { neg = -1; next(); }
                        require(Num, "bad enum initializer");
                        i = neg * ival;
                        next();
                    }
                    id[Class] = Num;
                    id[Type] = INT64;
                    id[Val] = i++;
                    skip_comma();
                }
                next();
            }
        } else if (tk == Struct || tk == Union) {
            int64_t is_union = (tk == Union);
            next();
            if (tk != '{') {
                s = id;
                next();
                if (s[Class] == Struct) { bt = s[Type]; }
                if (tk == '{') { id = s; } // restore id if defining now
            }
            if (tk == '{') {
                s = id;
                s[Class] = Struct; // unions use same Class as structs
                if (num_structs >= PTR - INT64 - 1) { fatal("too many structs"); }
                s[Type] = INT64 + 1 + num_structs;
                struct_syms[num_structs] = (int64_t)s;
                num_structs = num_structs + 1;
                s[Sline] = 0;
                next();
                i = 0;
                while (tk != '}') {
                    skip_const();
                    bt = INT64;
                    if (tk == Int || tk == Bool) { next();
                    } else if (tk == Int32_t) { next(); bt = INT32;
                    } else if (tk == Int64_t) { next(); bt = INT64;
                    } else if (tk == Char) { next(); bt = CHAR;
                    } else if (tk == Struct || tk == Union) {
                        next();
                        if (tk == '{') { fatal("nested struct/union def"); }
                        if (id[Class] == Struct) {
                            bt = id[Type];
                        } else {
                            fatal("bad struct/union type");
                        }
                        next();
                    } else if (tk == Id && id[Class] == Tdef) {
                        bt = id[Type]; next();
                    }
                    while (tk != ';') {
                        ty = bt;
                        while (tk == Mul) { next(); ty = ty + PTR; }
                        require(Id, "bad member declaration");
                        if (id[Class] == Loc) { fatal("duplicate member"); }
                        m = members_pool + members_next;
                        members_next = members_next + 4;
                        m[0] = id[Hash];
                        m[1] = ty;
                        if (is_union) {
                            m[2] = 0; // union: all members at offset 0
                        } else {
                            // struct: align offset before member
                            if (ty == INT32) {
                                i = ((i + 3) / 4) * 4;
                            } else if (ty != CHAR) {
                                i = ((i + 7) / 8) * 8;
                            }
                            m[2] = i;
                        }
                        m[3] = s[Sline];
                        s[Sline] = (int64_t)m;
                        // compute member size
                        int64_t msz;
                        if (ty == CHAR) {
                            msz = sizeof(char);
                        } else if (ty == INT32) {
                            msz = 4;
                        } else if (ty > INT64 && ty < FNPTR) {
                            msz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                        } else {
                            msz = sizeof(int64_t);
                        }
                        if (is_union) {
                            if (msz > i) { i = msz; } // union: max size
                        } else {
                            i = i + msz; // struct: accumulate
                        }
                        next();
                        skip_comma();
                    }
                    next();
                }
                s[Val] = ((i + 7) / 8) * 8; // pad to 8-byte boundary
                next();
            }
        } else if (tk == Typedef) {
            next();
            skip_const();
            bt = INT64;
            if (tk == Int || tk == Bool) { next();
            } else if (tk == Int32_t) { next(); bt = INT32;
            } else if (tk == Int64_t) { next();
            } else if (tk == Char) { next(); bt = CHAR;
            } else if (tk == Void) { next(); bt = INT64; // void fnptr
            } else if (tk == Struct || tk == Union) {
                next();
                if (tk != Id || id[Class] != Struct) { fatal("bad typedef"); }
                bt = id[Type];
                next();
            } else if (tk == Id && id[Class] == Tdef) {
                bt = id[Type];
                next();
            }
            // Check for function pointer typedef: typedef type (*name)(...)
            if (tk == '(') {
                next();
                expect(Mul, "* expected in fnptr typedef");
                require(Id, "bad fnptr typedef name");
                id[Class] = Tdef;
                id[Type] = FNPTR;
                next();
                expect(')', "close paren expected in fnptr typedef");
                expect('(', "open paren expected for fnptr params");
                while (tk != ')') { next(); }
                expect(')', "close paren expected");
                expect(';', "semicolon expected");
                continue;
            }
            while (tk == Mul) { next(); bt = bt + PTR; }
            require(Id, "bad typedef name");
            id[Class] = Tdef;
            id[Type] = bt;
            next();
            expect(';', "semicolon expected");
            continue;
        }
        while (tk != ';' && tk != '}') {
            ty = bt;
            while (tk == Mul) {
                next();
                ty = ty + PTR;
            }
            require(Id, "bad global declaration");
            if (id[Class] && id[Class] != Fun) { fatal("duplicate global def"); }
            d = id; // save function identifier
            next();
            d[Type] = ty;
            if (tk == '(') { // function
                next();
                i = 0;
                while (tk != ')') {
                    skip_const();
                    ty = INT64;
                    if (tk == Int || tk == Bool) {
                        next();
                    } else if (tk == Int32_t) {
                        next();
                        ty = INT32;
                    } else if (tk == Int64_t) {
                        next();
                        ty = INT64;
                    } else if (tk == Char) {
                        next();
                        ty = CHAR;
                    } else if (tk == Struct || tk == Union) {
                        next();
                        if (tk != Id || id[Class] != Struct) { fatal("bad type"); }
                        ty = id[Type];
                        next();
                    } else if (tk == Id && id[Class] == Tdef) {
                        ty = id[Type];
                        next();
                    }
                    if (tk != ')') { // not (void)
                        // Check for function pointer: type (*name)(...)
                        if (tk == '(') {
                            next();
                            expect(Mul, "* expected in function pointer");
                            require(Id, "bad fnptr name");
                            if (id[Class] == Loc) { fatal("dup param"); }
                            id[HClass] = id[Class];
                            id[Class] = Loc;
                            id[HType] = id[Type];
                            id[Type] = FNPTR;
                            id[HVal] = id[Val];
                            id[Val] = i++;
                            next();
                            expect(')', ") expected in fnptr");
                            expect('(', "( expected for fnptr params");
                            while (tk != ')') { next(); }
                            next();
                        } else {
                            while (tk == Mul) {
                                next();
                                ty = ty + PTR;
                            }
                            require(Id, "bad parameter decl");
                            if (id[Class] == Loc) { fatal("dup parameter"); }
                            id[HClass] = id[Class];
                            id[Class] = Loc;
                            id[HType] = id[Type];
                            id[Type] = ty;
                            id[HVal] = id[Val];
                            id[Val] = i++;
                            if (ty > INT64 && ty < FNPTR) { // struct param
                                id[Extent] = -1; // marker for struct param
                            } else {
                                id[Extent] = 0;
                            }
                            next();
                        }
                        skip_comma();
                    }
                }
                next();
                if (tk == ';') { // forward declaration
                    id = sym; // unwind params
                    while (id[Tk]) {
                        if (id[Class] == Loc) {
                            id[Class] = id[HClass];
                            id[Type] = id[HType];
                            id[Val] = id[HVal];
                        }
                        id = id + Idsz;
                    }
                    if (d[Class] != Fun) {
                        d[Class] = Fun;
                        d[Val] = 0; // no code yet
                    }
                    // ';' already current token,
                    // outer loop's next() will consume it
                    break;
                }
                if (tk != '{') {
                    fatal("bad function definition");
                }
                if (d[Class] == Fun && d[Val]) {
                    fatal("duplicate function definition");
                }
                d[Class] = Fun;
                d[Val] = (int64_t)(e + 1);
                // patch forward calls
                j = 0;
                while (j < fwd_sp) {
                    if (fwd_patches[j] == (int64_t)d) {
                        *(int64_t *)fwd_patches[j + 1] = d[Val];
                    }
                    j = j + 2;
                }
                loc = ++i;
                fn_ret_ty = d[Type];
                i = i + 8;          // reserve 8 slots for struct returns
                struct_temp = i;
                next();
                *++e = ENT;
                t = ++e;            // placeholder for ENT operand
                // copy struct params to local (pass-by-value)
                {
                    int64_t *pid = sym;
                    while (pid[Tk]) {
                        if (pid[Class] == Loc && pid[Extent] == -1) {
                            int64_t sty = pid[Type];
                            int64_t sz;
                            sz = ((int64_t *)struct_syms[sty - INT64 - 1])[Val];
                            int64_t slots = (sz + 7) / 8;
                            int64_t param_slot = pid[Val];
                            i = i + slots;
                            int64_t local_slot = i;
                            *++e = LEA; *++e = loc - local_slot; *++e = PSH;
                            *++e = LEA; *++e = loc - param_slot;
                            *++e = LI; *++e = PSH;  // deref param ptr
                            *++e = IMM; *++e = sz; *++e = PSH;
                            *++e = MCPY; *++e = ADJ; *++e = 3;
                            pid[Val] = local_slot;
                            pid[Extent] = 0;
                        }
                        pid = pid + Idsz;
                    }
                }
                int64_t fn_scope_mark = scope_sp;
                skip_const();
                while (tk == Int || tk == Bool || tk == Int32_t ||
                       tk == Int64_t || tk == Char || tk == Struct ||
                       tk == Union || (tk == Id && id[Class] == Tdef)) {
                    if (tk == Struct || tk == Union) {
                        next();
                        require(Id, "bad struct/union type");
                        if (id[Class] == Struct) {
                            bt = id[Type];
                        } else {
                            fatal("not a struct/union");
                        }
                        next();
                    } else if (tk == Id && id[Class] == Tdef) {
                        bt = id[Type];
                        next();
                    } else {
                        bt = INT64;
                        if (tk == Int || tk == Bool) {
                            next();
                        } else if (tk == Int32_t) {
                            next();
                            bt = INT32;
                        } else if (tk == Int64_t) {
                            next();
                            bt = INT64;
                        } else if (tk == Char) {
                            next();
                            bt = CHAR;
                        } else {
                            next();
                        }
                    }
                    while (tk != ';') {
                        ty = bt;
                        int64_t is_fnptr = 0;
                        while (tk == Mul) {
                            next();
                            ty = ty + PTR;
                        }
                        // Check for function pointer declarator: (*name)(params)
                        if (tk == '(') {
                            next();
                            expect(Mul, "* expected in fnptr decl");
                            require(Id, "bad fnptr decl name");
                            id[Type] = FNPTR;
                            ty = FNPTR;
                            next();
                            expect(')', ") expected in fnptr decl");
                            // Params are optional
                            if (tk == '(') {
                                next();
                                while (tk != ')') { next(); }
                                expect(')', ") expected");
                            }
                            is_fnptr = 1;
                        }
                        if (!is_fnptr) { require(Id, "bad local declaration"); }
                        if (id[Class] == Loc) { fatal("dup local def"); }
                        id[HClass] = id[Class];
                        id[Class] = Loc;
                        id[HType] = id[Type];
                        id[Type] = ty;
                        id[HVal] = id[Val];
                        id[Extent] = 0;
                        if (!is_fnptr) { next(); }
                        if (tk == Brak) {
                            next();
                            int64_t arr_size = 0;
                            if (tk == Num) { arr_size = ival; next(); }
                            expect(']', "close bracket expected");
                            id[Type] = ty + PTR;
                            int64_t elem_sz = 1;
                            if (ty == INT32) { elem_sz = 4;
                            } else if (ty == INT64 || ty >= PTR) { elem_sz = 8;
                            } else if (ty > INT64 && ty < FNPTR) {
                                elem_sz = ((int64_t*)struct_syms[ty-INT64-1])[Val];
                            }
                            if (tk == Assign) {
                                next();
                                expect('{', "{ expected for array init");
                                int64_t init_vals[256];
                                int64_t count = 0;
                                while (tk != '}' && count < 256) {
                                    int64_t neg = 1;
                                    if (tk == Sub) { neg = -1; next(); }
                                    require(Num, "constant expected");
                                    init_vals[count++] = neg * ival;
                                    next();
                                    skip_comma();
                                }
                                next();
                                if (arr_size == 0) { arr_size = count; }
                                id[Extent] = arr_size;
                                if (ty == CHAR) {
                                    i = i + (arr_size + 7) / 8;
                                } else if (ty == INT32) {
                                    i = i + (arr_size * 4 + 7) / 8;
                                } else {
                                    i = i + arr_size;
                                }
                                id[Val] = i;
                                int64_t j = 0;
                                while (j < count) {
                                    *++e = LEA;
                                    *++e = loc - id[Val];
                                    if (ty == CHAR) {
                                        *++e = PSH; *++e = IMM; *++e = j; *++e = ADD;
                                    } else if (ty == INT32) {
                                        *++e = PSH; *++e = IMM; *++e = j*4; *++e = ADD;
                                    } else {
                                        *++e = PSH; *++e = IMM; *++e = j*8; *++e = ADD;
                                    }
                                    *++e = PSH;
                                    *++e = IMM;
                                    *++e = init_vals[j];
                                    if (ty == CHAR) { *++e = SC;
                                    } else if (ty == INT32) { *++e = SI32;
                                    } else { *++e = SI;
                                    }
                                    j++;
                                }
                            } else {
                                if (arr_size == 0) { fatal("array size required"); }
                                id[Extent] = arr_size;
                                if (ty == CHAR) {
                                    i = i + (arr_size + 7) / 8;
                                } else if (ty == INT32) {
                                    i = i + (arr_size * 4 + 7) / 8;
                                } else if (ty > INT64 && ty < FNPTR) {
                                    s = (int64_t *)struct_syms[ty - INT64 - 1];
                                    i = i + (arr_size * s[Val] + 7) / 8;
                                } else {
                                    i = i + arr_size;
                                }
                                id[Val] = i;
                            }
                        } else if (ty > INT64 && ty < FNPTR) {
                            sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                            i = i + (sz + 7) / 8;
                        } else {
                            ++i;
                        }
                        if (id[Val] == 0) { id[Val] = i; }
                        if (tk == Assign) {
                            d = id;
                            next();
                            if (d[Type] > INT64 && d[Type] < FNPTR) {
                                expression(Assign);
                                int64_t sty = d[Type] - INT64 - 1;
                                int64_t *ss = (int64_t *)struct_syms[sty];
                                int64_t ssz = ss[Val];
                                *++e = PSH;
                                *++e = LEA;
                                *++e = loc - d[Val];
                                *++e = PSH;
                                *++e = SWAP;
                                *++e = IMM;
                                *++e = ssz;
                                *++e = PSH;
                                *++e = MCPY;
                                *++e = ADJ;
                                *++e = 3;
                            } else {
                                *++e = LEA;
                                *++e = loc - d[Val];
                                *++e = PSH;
                                expression(Assign);
                                if (d[Type] == CHAR) {
                                    *++e = SC;
                                } else if (d[Type] == INT32) {
                                    *++e = SI32;
                                } else {
                                    *++e = SI;
                                }
                            }
                        }
                        skip_comma();
                    }
                    next();
                    skip_const();
                }
                while (tk != '}') {
                    statement();
                }
                *t = i - loc; // patch ENT operand (includes block locals)
                *++e = LEV;
                // restore scope_stack for static locals declared in function
                while (scope_sp > fn_scope_mark) {
                    scope_sp = scope_sp - 5;
                    d = (int64_t *)scope_stack[scope_sp];
                    d[Class] = scope_stack[scope_sp + 1];
                    d[Type] = scope_stack[scope_sp + 2];
                    d[Val] = scope_stack[scope_sp + 3];
                    d[Extent] = scope_stack[scope_sp + 4];
                }
                id = sym; // unwind symbol table locals
                while (id[Tk]) {
                    if (id[Class] == Loc) {
                        id[Class] = id[HClass];
                        id[Type] = id[HType];
                        id[Val] = id[HVal];
                    }
                    id = id + Idsz;
                }
            } else {
                id[Class] = Glo;
                id[Val] = (int64_t)data;
                id[Extent] = 0;
                if (tk == Brak) {
                    next();
                    int64_t arr_size = 0;
                    if (tk == Num) { arr_size = ival; next(); }
                    expect(']', "close bracket expected");
                    id[Type] = ty + PTR;
                    if (tk == Assign) {
                        next();
                        expect('{', "{ expected for array init");
                        int64_t count = 0;
                        char *arr_start = data;
                        while (tk != '}') {
                            int64_t neg = 1;
                            if (tk == Sub) { neg = -1; next(); }
                            if (tk == Num) {
                                if (ty == CHAR) {
                                    *(char *)data = (char)(neg * ival);
                                    data = data + 1;
                                } else if (ty == INT32) {
                                    *(int32_t *)data = (int32_t)(neg * ival);
                                    data = data + 4;
                                } else {
                                    *(int64_t *)data = neg * ival;
                                    data = data + sizeof(int64_t);
                                }
                                next();
                            } else if (tk == '"' && ty == CHAR) {
                                char *s = (char *)ival;
                                while (*s) { *(char *)data++ = *s++; }
                                next();
                            } else {
                                fatal("constant expected in array init");
                            }
                            count++;
                            skip_comma();
                        }
                        next();
                        if (arr_size == 0) { arr_size = count; }
                        id[Extent] = arr_size;
                        // pad to declared size
                        int64_t elem_sz = 1;
                        if (ty == INT32) { elem_sz = 4;
                        } else if (ty == INT64 || ty >= PTR) { elem_sz = 8;
                        } else if (ty > INT64 && ty < FNPTR) {
                            elem_sz = ((int64_t *)struct_syms[ty-INT64-1])[Val];
                        }
                        int64_t total = arr_size * elem_sz;
                        int64_t used = data - arr_start;
                        while (used < total) { *(char *)data++ = 0; used++; }
                    } else {
                        if (arr_size == 0) { fatal("array size required"); }
                        id[Extent] = arr_size;
                        if (ty == CHAR) {
                            data = data + arr_size;
                        } else if (ty == INT32) {
                            data = data + arr_size * 4;
                        } else if (ty > INT64 && ty < FNPTR) {
                            sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                            data = data + arr_size * sz;
                        } else {
                            data = data + arr_size * sizeof(int64_t);
                        }
                    }
                } else {
                    int64_t init_val = 0;
                    if (tk == Assign) {
                        next();
                        int64_t neg = 1;
                        if (tk == Sub) { neg = -1; next(); }
                        if (tk == Num) { init_val = neg * ival; next();
                        } else if (tk == '"') {
                            init_val = ival; next();
                            id[Val] = (int64_t)data; // data moved during lex
                        } else { fatal("constant initializer required");
                        }
                    }
                    if (ty == CHAR) {
                        *(char *)data = (char)init_val;
                        data = data + sizeof(char);
                    } else if (ty == INT32) {
                        *(int32_t *)data = (int32_t)init_val;
                        data = data + 4;
                    } else if (ty > INT64 && ty < FNPTR) {
                        sz = ((int64_t *)struct_syms[ty - INT64 - 1])[Val];
                        data = data + sz;
                    } else {
                        *(int64_t *)data = init_val;
                        data = data + sizeof(int64_t);
                    }
                }
            }
            skip_comma();
        }
        next();
    }
    // check for unresolved forward declarations
    j = 0;
    while (j < fwd_sp) {
        d = (int64_t *)fwd_patches[j];
        if (!d[Val]) {
            printf("unresolved forward declaration: %.*s\n", 
                   (int)(d[Hash] & 0x3F), (char *)d[Name]);
            return 0;
        }
        j = j + 2;
    }
    if (!idmain[Val]) {
        printf("main() not defined\n");
        return 0;
    }
    return (int64_t *)idmain[Val];
}

int run(int64_t *pc, int argc, char **argv) {
    int64_t *sp, *bp, *t, a, cycle;
    sp = (int64_t *)((int64_t)stack_pool + poolsz);
    bp = sp;
    *--sp = EXIT; // call exit if main returns
    *--sp = PSH;
    t = sp;
    *--sp = argc;
    *--sp = (int64_t)argv;
    *--sp = (int64_t)t;
    a = 0;
    cycle = 0;
    while (1) {
        i = *pc++;
        ++cycle;
        if (debug) {
            if (i < I_OPEN) {
                printf("%d> %.4s", (int)cycle,
                    &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,"
                     "LEV ,LI  ,LC  ,LI32,SI  ,SC  ,SI32,PSH ,"
                     "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,"
                     "GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                     "DUP ,SPGT,SWAP,JSRI,MCPY,EXIT,PEEK,"[i * 5]);
            } else {
                printf("%d> %.4s", (int)cycle,
                    &"OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,"
                     "IEXT,WRIT,SYST,POPN,PCLS,FRED,IMCP,MMOV,"
                     "SCPY,SCMP,SLEN,SCAT,SNCM,ASRT,ALCA,"
                     [(i - I_OPEN) * 5]);
            }
            if (i <= ADJ) {
                printf(" %d\n", (int)*pc);
            } else {
                printf("\n");
            }
        }
        switch (i) {
            case LEA:  a = (int64_t)(bp + *pc++); break;
            case IMM:  a = *pc++; break;
            case JMP:  pc = (int64_t *)*pc; break;
            case JSR:  *--sp = (int64_t)(pc + 1); pc = (int64_t *)*pc; break;
            case BZ:   pc = a ? pc + 1 : (int64_t *)*pc; break;
            case BNZ:  pc = a ? (int64_t *)*pc : pc + 1; break;
            case ENT:  *--sp = (int64_t)bp; bp = sp; sp = sp - *pc++; break;
            case ADJ:  sp = sp + *pc++; break;
            case LEV:
                sp = bp; bp = (int64_t *)*sp++;
                pc = (int64_t *)*sp++; break;
            case LI:   a = *(int64_t *)a; break;
            case LC:   a = *(char *)a; break;
            case LI32: a = *(int32_t *)a; break;
            case SI:   *(int64_t *)*sp++ = a; break;
            case SC:   a = *(char *)*sp++ = a; break;
            case SI32: *(int32_t *)*sp++ = (int32_t)a; break;
            case PSH:  *--sp = a; break;
            case OR:   a = *sp++ | a; break;
            case XOR:  a = *sp++ ^ a; break;
            case AND:  a = *sp++ & a; break;
            case EQ:   a = *sp++ == a; break;
            case NE:   a = *sp++ != a; break;
            case LT:   a = *sp++ < a; break;
            case GT:   a = *sp++ > a; break;
            case LE:   a = *sp++ <= a; break;
            case GE:   a = *sp++ >= a; break;
            case SHL:  a = *sp++ << a; break;
            case SHR:  a = *sp++ >> a; break;
            case ADD:  a = *sp++ + a; break;
            case SUB:  a = *sp++ - a; break;
            case MUL:  a = *sp++ * a; break;
            case DIV:  a = *sp++ / a; break;
            case MOD:  a = *sp++ % a; break;
            case DUP:  *--sp = a; *--sp = a; break;
            case SPGET: a = (int64_t)sp; break;
            case SWAP: a = sp[0]; sp[0] = sp[1]; sp[1] = a; break;
            case PEEK: a = *sp; break;
            case JSRI: *--sp = (int64_t)pc; pc = (int64_t *)a; break;
            case MCPY:
                a = (int64_t)memcpy((char *)sp[2], (char *)sp[1], *sp); break;
            case EXIT:
                printf("exit(%d) cycle = %d\n", (int)*sp, (int)cycle);
                return *sp;
        // intrinsics
            case I_OPEN:
                a = (pc[1] == 2) ? open((char *)sp[1], *sp)
                                 : open((char *)sp[2], sp[1], *sp);
                break;
            case I_READ: a = read(sp[2], (char *)sp[1], *sp); break;
            case I_CLOS: a = close(*sp); break;
            case I_PRTF:
                t = sp + pc[1];
                a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]);
                break;
            case I_MALC: a = (int64_t)malloc(*sp); break;
            case I_FREE: free((void *)*sp); break;
            case I_MSET: a = (int64_t)memset((char *)sp[2], sp[1], *sp); break;
            case I_MCMP: a = memcmp((char *)sp[2], (char *)sp[1], *sp); break;
            case I_EXIT:
            printf("exit(%d) cycle = %d\n", (int)*sp, (int)cycle);
            return *sp;
            case I_WRIT: a = write(sp[2], (char *)sp[1], *sp); break;
            case I_SYST: a = system((char *)*sp); break;
            case I_POPN: a = (int64_t)popen((char *)sp[1], (char *)*sp); break;
            case I_PCLS: a = pclose((void *)*sp); break;
            case I_FRED:
                a = fread((void *)sp[3], sp[2], sp[1], (void *)*sp); break;
            case I_MCPY:
                a = (int64_t)memcpy((char *)sp[2], (char *)sp[1], *sp); break;
            case I_MMOV:
                a = (int64_t)memmove((char *)sp[2], (char *)sp[1], *sp); break;
            case I_SCPY: a = (int64_t)strcpy((char *)sp[1], (char *)*sp); break;
            case I_SCMP: a = strcmp((char *)sp[1], (char *)*sp); break;
            case I_SLEN: a = strlen((char *)*sp); break;
            case I_SCAT: a = (int64_t)strcat((char *)sp[1], (char *)*sp); break;
            case I_SNCM: a = strncmp((char *)sp[2], (char *)sp[1], *sp); break;
            case I_ASRT:
                if (!*sp) {
                    printf("assert failed at cycle %d\n", (int)cycle);
                    return -1;
                }
                break;
            case I_ALCA: a = (int64_t)malloc(*sp); break;
            case I_MRED: a = (int64_t)mem_read((const char *)*sp); break;
            case I_MCLS: mem_close((char *)*sp); break;
            case I_MWRT: a = mem_write((const char*)sp[2], (const char*)sp[1], sp[0]); break;
            case I_LSEEK: a = lseek(sp[2], sp[1], sp[0]); break;
            case I_MMAP: a = (int64_t)mmap((void*)sp[5], sp[4], sp[3], sp[2], sp[1], sp[0]); break;
            case I_MUNMAP: a = munmap((void*)sp[1], sp[0]); break;
            case I_MSYNC: a = msync((void*)sp[2], sp[1], sp[0]); break;
            case I_FTRUNC: a = ftruncate(sp[1], sp[0]); break;
            case I_REN: a = rename((const char*)sp[1], (const char*)sp[0]); break;
            default:
                printf("unknown instruction = %d! cycle = %d\n",
                    (int)i, (int)cycle);
                return -1;
        }
    }
}

int main(int argc, char **argv) {
    int64_t *pc;
    --argc; ++argv;
    while (argc > 0 && **argv == '-') {
        if ((*argv)[1] == 's') {
            src = 1; --argc; ++argv;
        } else if ((*argv)[1] == 'd' && (*argv)[2] == 0) {
            debug = 1; --argc; ++argv;
        } else if ((*argv)[1] == 'D') {
            if (cmdline_def_count < 64) {
                cmdline_defs[cmdline_def_count++] = *argv + 2;
            }
            --argc; ++argv;
        } else if ((*argv)[1] == '-') {
            --argc; ++argv;
            break;
        } else {
            break;
        }
    }
    if (argc < 1) {
        printf("usage: cx [-s] [-d] [-Dname[=value]] file ...\n");
        return -1;
    }
    pc = compile(*argv);
    if (!pc) { return -1; }
    if (src) { return 0; }
    return run(pc, argc, argv);
}
