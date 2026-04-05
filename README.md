c4 - C in four functions
========================

An exercise in minimalism.

Try the following:

    gcc -o c4 c4.c
    ./c4 hello.c
    ./c4 -s hello.c
    
    ./c4 c4.c hello.c
    ./c4 c4.c c4.c hello.c


cx - C compiler
===============

~An exercise in minimalism~.
More C89/C99 like features.

Try the following
(If your system does not have "cx" use clang or gcc instead):

    mkdir -p build
    cc -o build/cx cx.c
    ./build/cx hello.c
    ./build/cx -s hello.c

    ./build/cx cx.c hello.c
    ./build/cx cx.c cx.c hello.c
    ./build/cx cx.c cx.c test/tests.c

Command Line Options
--------------------

    -s      dump source and assembly
    -d      dump debug execution trace
    --      end of options (pass remaining arguments to script)

Run tests:

    ./build/cx test/tests.c

Documentation
-------------

- [LANGUAGE.md](LANGUAGE.md) - EBNF grammar and supported C99 features

