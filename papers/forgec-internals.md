# forgec Internals — Compiler Reference

**Version:** 1.0
**Status:** Living Document
**Audience:** Contributors, maintainers, and curious readers
**Companion:** See `papers/specs/` for the Forge language specification

---

## Preface

This document describes the internal design of `forgec`, the bootstrap
compiler for the Forge programming language. It covers architecture,
data structures, optimization strategy, and the reasoning behind every
major design decision.

`forgec` is written in C99. It has no external dependencies. It produces
ELF64 object files for x86-64. The entire compiler targets approximately
20,000 lines of source. This is a design constraint — a compiler you can
read in a weekend is a compiler you can trust and modify.

The guiding philosophy is **"worse is better"**: a simple, correct,
working implementation is worth more than a complex, theoretically
optimal one that takes years to ship. Every decision in this document
prioritizes simplicity of implementation over completeness of feature.

---

## Table of Contents

1. [Repository Layout](#1-repository-layout)
2. [Build Instructions](#2-build-instructions)
3. [Architecture Overview](#3-architecture-overview)
4. [Memory Management — The Arena Allocator](#4-memory-management--the-arena-allocator)
5. [Error Reporting](#5-error-reporting)
6. [Phase 1 — The Lexer](#6-phase-1--the-lexer)
7. [Phase 2 — The Parser](#7-phase-2--the-parser)
8. [Phase 3 — Semantic Analysis](#8-phase-3--semantic-analysis)
9. [Phase 4 — IR Generation](#9-phase-4--ir-generation)
10. [Phase 5 — Optimization Passes](#10-phase-5--optimization-passes)
11. [Phase 6 — Code Generation](#11-phase-6--code-generation)
12. [Phase 7 — ELF Emission](#12-phase-7--elf-emission)
13. [The IR Design](#13-the-ir-design)
14. [Register Allocation](#14-register-allocation)
15. [The x86-64 Backend](#15-the-x86-64-backend)
16. [Purity Enforcement](#16-purity-enforcement)
17. [What forgec Deliberately Does Not Do](#17-what-forgec-deliberately-does-not-do)
18. [Bootstrapping](#18-bootstrapping)
19. [Testing Strategy](#19-testing-strategy)
20. [Further Reading](#20-further-reading)

---

## 1. Repository Layout

```
forgec/
  src/
    headers/
      arena.h       — arena allocator interface
      util.h        — error reporting, file loading, string utilities
      lexer.h       — token types, lexer state
      parser.h      — AST node types, parser state
      sema.h        — symbol table, type checker state
      ir.h          — IR instruction types, basic blocks, functions
      codegen.h     — register allocator, instruction emitter state
      elf.h         — ELF64 section builder

    implementation/
      main.c        — entry point, CLI argument handling, pipeline
      arena.c       — arena allocator implementation
      util.c        — error reporting, load_file, string interning
      lexer.c       — tokenizer
      parser.c      — recursive descent parser
      sema.c        — type checker, purity verifier, scope resolver
      ir.c          — AST to IR lowering
      codegen.c     — IR to x86-64 machine code
      elf.c         — machine code to ELF64 object file

  papers/
    specs/          — Forge language specification versions
    forgec-internals.md  — this document

  examples/         — reference Forge programs
  bin/              — compiled binary output (not committed)
  Makefile
```

Each `.c` file has exactly one responsibility. Each `.h` file exposes
exactly the interface its corresponding `.c` file provides. No file
should need to reach into another file's internals.

---

## 2. Build Instructions

```sh
# Build forgec
make

# Clean build artifacts
make clean

# Debug build (no optimization, assertions enabled)
make debug

# Run against a Forge source file
bin/forgec input.fg -o output.o

# Debug flags — dump internal state at each phase
bin/forgec --dump-tokens input.fg
bin/forgec --dump-ast    input.fg
bin/forgec --dump-ir     input.fg
```

The compiler is built with `-std=c99 -Wall -Wextra -pedantic`. It must
build cleanly with zero warnings. If you add code that produces a
warning, fix the warning before committing.

---

## 3. Architecture Overview

`forgec` is a single-pass, single-threaded compiler organized as a
linear pipeline. Source text enters at one end. An ELF64 object file
exits at the other. Each phase transforms its input into a new
representation that the next phase consumes.

```
Source text (.fg file)
        |
        v
[Phase 1: Lexer]
        |   Token stream
        v
[Phase 2: Parser]
        |   Abstract Syntax Tree (AST)
        v
[Phase 3: Semantic Analysis]
        |   Decorated AST (types resolved, scopes checked)
        v
[Phase 4: IR Generation]
        |   Flat three-address IR
        v
[Phase 5: Optimization]
        |   Optimized IR
        v
[Phase 6: Code Generation]
        |   x86-64 machine code bytes
        v
[Phase 7: ELF Emission]
        |
        v
ELF64 object file (.o)
```

Each phase is independently testable. The `--dump-*` flags halt the
pipeline after a given phase and print the current representation. This
is the primary debugging tool during development.

No phase feeds back into a previous phase. There are no multiple passes
over the AST. Once a representation has been consumed by the next phase,
it is freed. This keeps memory usage low and compilation fast.

### Why Single-Threaded?

forgec does not use threads. Forge's module system ensures each module
is compiled exactly once — there are no header files to re-parse. A
full kernel compiles in under one second on a single core. There is
nothing to parallelize inside the compiler.

Parallelism, when needed, is handled by Anvil (the build system) at
the process level: independent modules can be compiled by separate
forgec processes simultaneously. This requires no changes to forgec.

---

## 4. Memory Management — The Arena Allocator

`forgec` uses arena allocation throughout. There is no `malloc`/`free`
pairing anywhere in the compiler except inside `arena.c` itself.

An arena is a large contiguous block of memory. Allocations bump a
pointer forward. When a phase completes, the entire arena is reset
(the pointer moves back to the start) or freed (the block is released
to the OS). This is:

- **Fast** — allocation is a pointer increment and a bounds check
- **Simple** — no fragmentation, no use-after-free, no double-free
- **Cache-friendly** — all objects of a phase are contiguous in memory

### Arena Lifecycle

```
main() creates arenas:
  ast_arena  — holds tokens, AST nodes, symbol names
  ir_arena   — holds IR instructions, basic blocks
  code_buf   — output buffer for machine code bytes

Lexer    → uses ast_arena
Parser   → uses ast_arena
Sema     → uses ast_arena
IR lower → uses ir_arena, reads ast_arena
           → frees ast_arena when done (AST no longer needed)
Codegen  → uses code_buf, reads ir_arena
           → frees ir_arena when done
ELF emit → reads code_buf
           → frees code_buf when done
```

### Arena Sizing

Default sizes:

```c
ast_arena  = arena_new(16 * 1024 * 1024);   /* 16 MiB */
ir_arena   = arena_new(8  * 1024 * 1024);   /*  8 MiB */
code_buf   = buffer_new(4  * 1024 * 1024);  /*  4 MiB */
```

These are compile-time constants. If a large file exceeds them, the
compiler aborts with a clear error message. For a kernel codebase
these sizes are more than sufficient. They can be increased if needed.

### Alignment

All arena allocations are 8-byte aligned. The arena_alloc function
rounds the requested size up to the next multiple of 8 before bumping
the pointer. This ensures all allocated objects are safely accessible
on x86-64 regardless of their declared type.

---

## 5. Error Reporting

All errors go through `error_at()` in `util.c`. There are no other
error reporting mechanisms in the compiler.

```c
void error_at(const char* file, int line, int col,
              const char* fmt, ...);
```

Output format:
```
forgec: error: kernel/boot.fg:42:17: unexpected token '@'
```

`error_at()` prints the message to `stderr` and calls `exit(1)`.
forgec stops at the first error. Error recovery (continuing after an
error to find more errors) is not implemented in the bootstrap
compiler. This is a deliberate simplification.

Every component that detects an error calls `error_at()` with the
source location of the offending token or node. Source locations
(filename, line, column) are stored in tokens and propagated into
AST nodes and IR instructions.

`die()` is a variant for internal compiler errors — situations that
should never happen if the compiler is correct:

```c
void die(const char* fmt, ...);
```

Output format:
```
forgec: internal error: arena exhausted — this is a compiler bug
```

If you see `die()` output, it is a bug in forgec, not in the user's
Forge code.

---

## 6. Phase 1 — The Lexer

**Input:** Source text (contiguous buffer in memory)
**Output:** Token stream (on demand — lexer is pull-based)
**File:** `src/implementation/lexer.c`

### Design

The lexer is pull-based. The parser calls `lexer_next_token()` each
time it needs the next token. The lexer does not produce a list of all
tokens upfront. This keeps memory usage low and avoids an extra pass
over the source.

Tokens do not copy their text. A `Token` contains a pointer into the
source buffer and a length. The source buffer lives for the duration
of compilation. To print a token's text:

```c
printf("%.*s", token.length, token.start);
```

### Token Structure

```c
typedef struct {
    TokenType   type;
    const char* start;    /* pointer into source buffer */
    int         length;   /* byte length of token text */
    int         line;     /* 1-based line number */
    int         col;      /* 1-based column number */
} Token;
```

### Lexer State

```c
typedef struct {
    const char* source;     /* entire file in memory */
    const char* cursor;     /* current scan position */
    const char* filename;   /* for error messages */
    int         line;
    int         col;
} LexerState;
```

### What the Lexer Handles

- All keywords (see §3.3 of the language spec)
- All attributes (`@mut`, `@pub`, `@noreturn`, etc.)
- All operators including multi-character (`<<`, `<<=`, `..`, `..=`,
  `<-`)
- Integer literals with type suffixes (`42uint32`, `0xFFuptr`)
- Float literals with type suffixes (`3.14float32`)
- String literals including raw strings (`r"..."`)
- Character literals including escape sequences
- Single-line comments (`//`)
- Block comments including nested block comments (`/* /* */ */`)
- Documentation comments (`///`)

### Keyword Recognition

After lexing an identifier, the lexer checks if it matches a keyword
using a linear scan of a static keyword table. With fewer than 30
keywords, linear scan is faster than a hash map due to cache effects.

Attributes are recognized similarly after the `@` character is consumed.
An unknown `@word` produces `TOKEN_ERROR`.

### What the Lexer Does Not Handle

- Integer literal validation (does the value fit the declared suffix
  type?) — handled by the parser
- String escape processing — handled by the parser when building string
  constant values
- Encoding validation — the lexer treats source as bytes, not Unicode.
  String contents may contain arbitrary UTF-8 but identifiers are
  ASCII only.

---

## 7. Phase 2 — The Parser

**Input:** Token stream from the lexer
**Output:** Abstract Syntax Tree (AST)
**File:** `src/implementation/parser.c`

### Design

The parser is a hand-written recursive descent parser. There is one
parsing function per grammar production. The parser consumes tokens
from the lexer on demand.

Recursive descent was chosen because:
- It is simple to write and simple to understand
- The grammar is naturally expressed as mutually recursive functions
- Error messages can be precise — each function knows exactly what it
  expected
- No parser generator dependency

### AST Node Design

Every AST node is allocated from `ast_arena`. Nodes are tagged unions:

```c
typedef enum {
    NODE_MODULE,
    NODE_IMPORT,
    NODE_FUNCTION,
    NODE_PROCEDURE,
    NODE_PARAM,
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_CONSTEXPR,
    NODE_ASSIGN,
    NODE_RETURN,
    NODE_IF,
    NODE_LOOP,
    NODE_WHILE,
    NODE_FOR,
    NODE_MATCH,
    NODE_MATCH_ARM,
    NODE_DEFER,
    NODE_DEFER_ERR,
    NODE_CALL,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CAST,
    NODE_INDEX,
    NODE_FIELD,
    NODE_ADDR_OF,
    NODE_DEREF,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_CHAR_LIT,
    NODE_BOOL_LIT,
    NODE_IDENT,
    NODE_STRUCT_DECL,
    NODE_STRUCT_LIT,
    NODE_UNION_DECL,
    NODE_ENUM_DECL,
    NODE_IMPL,
    NODE_METHOD,
    NODE_ASM,
    NODE_COMPTIME,
    NODE_COMPTIME_IF,
} NodeKind;

typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeKind    kind;
    int         line;
    int         col;
    /* kind-specific fields follow */
    union {
        /* NODE_FUNCTION, NODE_PROCEDURE */
        struct {
            char*      name;
            ASTNode**  params;
            int        param_count;
            ASTNode*   body;
            ASTNode**  attrs;
            int        attr_count;
            /* return type — NULL for procedures */
            ASTNode*   return_type;
        } func;

        /* NODE_VAR_DECL */
        struct {
            char*    name;
            ASTNode* type;
            ASTNode* init;     /* NULL if zero-initialized */
            int      is_mut;
            int      is_uninit;
        } var;

        /* NODE_BINARY */
        struct {
            TokenType op;
            ASTNode*  left;
            ASTNode*  right;
        } binary;

        /* NODE_CALL */
        struct {
            ASTNode*  callee;
            ASTNode** args;
            int       arg_count;
        } call;

        /* NODE_INT_LIT */
        struct {
            uint64_t  value;
            IRType    type;    /* uint8, uint32, usize, etc. */
        } int_lit;

        /* NODE_IDENT */
        struct {
            char* name;
        } ident;

        /* ... other variants */
    };
};
```

### Parser Structure

```c
typedef struct {
    LexerState* lexer;
    Arena*      arena;
    Token       current;    /* current token */
    Token       previous;   /* previously consumed token */
} ParseState;
```

The parser maintains one token of lookahead (`current`). `advance()`
consumes the current token and fetches the next. `expect(type)` calls
`advance()` and calls `error_at()` if the token type doesn't match.

### Operator Precedence

Forge follows C operator precedence. The parser implements this with
the standard technique of separate functions for each precedence level:

```
parse_expr()          — entry point
parse_assignment()    — =  +=  -=  etc.
parse_logical_or()    — or
parse_logical_and()   — and
parse_comparison()    — ==  !=  <  >  <=  >=
parse_bitwise_or()    — |
parse_bitwise_xor()   — ^
parse_bitwise_and()   — &
parse_shift()         — <<  >>
parse_additive()      — +  -
parse_multiplicative() — *  /  %
parse_unary()         — not  -  ~  *  &
parse_postfix()       — .  []  ()
parse_primary()       — literals, identifiers, ( expr )
```

Each function handles its level and calls the next level for higher-
precedence subexpressions. This naturally produces left-associative
operators.

---

## 8. Phase 3 — Semantic Analysis

**Input:** AST from the parser
**Output:** Decorated AST (types resolved, scopes annotated)
**File:** `src/implementation/sema.c`

### What Sema Checks

1. **Name resolution** — every identifier refers to a declared name
2. **Type checking** — operand types are compatible, casts are valid
3. **Purity enforcement** — pure functions do not call procedures,
   write globals, or perform I/O
4. **Return checking** — every function has a `return` on every path
5. **Mutability checking** — immutable variables are not assigned
6. **Uninit tracking** — `@uninit` variables are written before read
7. **Error code enforcement** — error return values are not discarded
8. **`@noreturn` verification** — `@noreturn` functions have no
   reachable `return`

### Symbol Table

The symbol table is a stack of scopes. Each scope is a flat array of
`(name, type, attributes)` entries. Entering a block pushes a new
scope. Leaving a block pops it. Name lookup walks the stack from top
(innermost) to bottom (module scope).

```c
typedef struct {
    char*   name;
    IRType  type;
    int     is_mut;
    int     is_procedure;
    int     is_noreturn;
    VReg    vreg;           /* virtual register assigned during IR gen */
} Symbol;

typedef struct {
    Symbol* entries;
    int     count;
    int     cap;
} Scope;

typedef struct {
    Scope*  scopes;
    int     depth;
    int     cap;
    Arena*  arena;
} SymbolTable;
```

### Type Representation

Types are represented as small integers during sema and IR generation.
There is no heap-allocated type tree for basic types. Pointer types and
array types are represented with a small struct:

```c
typedef struct {
    IRType  base;       /* uint32, uint8, etc. */
    int     is_ptr;     /* 1 if this is a pointer */
    int     is_const;   /* @const pointee */
    int     is_volatile;/* @volatile pointee */
    int     array_size; /* 0 if not an array */
} TypeInfo;
```

Struct and enum types are looked up by name in a separate type table.

---

## 9. Phase 4 — IR Generation

**Input:** Decorated AST
**Output:** Flat three-address IR organized into basic blocks
**File:** `src/implementation/ir.c`

### IR Design Principles

The Forge IR is a flat, non-SSA, three-address intermediate
representation. Design decisions:

**Non-SSA:** SSA (Static Single Assignment) enables powerful
optimizations but requires phi node insertion and dominance analysis
to construct correctly. For the bootstrap compiler, non-SSA is
sufficient. Each virtual register can be assigned multiple times.

**Three-address:** Each instruction has at most two source operands
and one destination. This maps cleanly to x86-64 instructions which
also have at most two operands.

**Integer virtual registers:** A virtual register is a plain `int`.
The register allocator maps these integers to physical x86-64 registers
or stack slots. There are no heap-allocated value nodes.

**Basic blocks:** Instructions are grouped into basic blocks. A basic
block has exactly one entry (the first instruction) and exactly one
exit (the last instruction, which must be a branch or return). This
makes control flow explicit and simplifies code generation.

### IR Instruction Set

See `src/headers/ir.h` for the complete `IROp` enumeration.
Key instruction categories:

| Category | Instructions |
|----------|-------------|
| Constants | `IR_CONST` |
| Arithmetic | `IR_ADD IR_SUB IR_MUL IR_DIV IR_MOD` |
| Bitwise | `IR_AND IR_OR IR_XOR IR_NOT IR_NEG IR_SHL IR_SHR` |
| Comparison | `IR_CMP_EQ IR_CMP_NEQ IR_CMP_LT IR_CMP_LTE IR_CMP_GT IR_CMP_GTE` |
| Memory | `IR_LOAD IR_STORE IR_ADDR IR_COPY IR_ALLOCA` |
| Conversion | `IR_CAST IR_BITCAST` |
| Control flow | `IR_JUMP IR_JUMP_IF IR_RET` |
| Calls | `IR_CALL IR_CALL_INDIRECT` |
| Hardware | `IR_PORT_WRITE IR_PORT_READ IR_ASM` |

### Virtual Register Convention

Virtual registers are allocated sequentially from a counter in
`IRFunction.vreg_next`. Register 0 is invalid (VREG_INVALID = -1).
Function parameters receive the first N virtual registers where N is
the parameter count.

```c
/* Allocate a new virtual register */
VReg ir_new_vreg(IRFunction* fn) {
    return fn->vreg_next++;
}
```

### Control Flow Lowering

`if/else` statements:

```
; if condition { then_body } else { else_body }

    %cond = ... evaluate condition ...
    jump_if %cond, block.then, block.else

block.then:
    ... then_body instructions ...
    jump block.merge

block.else:
    ... else_body instructions ...
    jump block.merge

block.merge:
    ... continues here ...
```

`loop` statements:

```
; loop { body }

    jump block.loop_head

block.loop_head:
    ... body instructions ...
    jump block.loop_head   ; back edge

block.loop_exit:           ; break jumps here
    ...
```

`for i in lo..hi`:

```
    %i = copy uint32 %lo
    jump block.for_check

block.for_check:
    %cond = cmp_lt uint32 %i, %hi
    jump_if %cond, block.for_body, block.for_exit

block.for_body:
    ... body, %i is the loop variable ...
    %i = add uint32 %i, 1    ; increment
    jump block.for_check

block.for_exit:
    ...
```

### Multiple Return Values

Functions that return `(T1, T2)` tuples produce two virtual registers.
The IR lowerer generates two `IR_RET` metadata annotations. The
register allocator places the first return value in `rax` and the
second in `rdx`, consistent with the System V AMD64 ABI struct-return
convention.

---

## 10. Phase 5 — Optimization Passes

**Input:** IR from Phase 4
**Output:** Optimized IR
**Files:** Part of `src/implementation/ir.c`

Optimization philosophy: implement only optimizations that are simple
enough to be provably correct, valuable enough to matter for kernel
code, and bounded in size. The total optimization code targets under
1000 lines.

### Pass 1 — Constant Folding (during IR generation)

Performed inline during IR generation, not as a separate pass. When
emitting a binary or unary instruction where all operands are
`IR_CONST`, compute the result at compile time and emit `IR_CONST`
directly.

```c
/* During emit_add(): */
if (ir_is_const(fn, src1) && ir_is_const(fn, src2)) {
    return ir_emit_const(fn, block, type,
                         ir_const_val(fn, src1) +
                         ir_const_val(fn, src2));
}
```

Handles: add, sub, mul, div, mod, and, or, xor, not, neg, shl, shr,
all comparisons, and cast of constant to compatible type.

**Invariant:** never fold instructions with side effects.

### Pass 2 — Strength Reduction (during IR generation)

Also performed inline. Replaces expensive operations with cheaper
equivalents when one operand is a known constant:

- `x * (2^n)` → `x << n`
- `x / (2^n)` → `x >> n` (unsigned only)
- `x * 0` → `const 0`
- `x * 1` → `copy x`
- `x + 0` → `copy x`
- `x | 0` → `copy x`
- `x & ~0` → `copy x`

### Pass 3 — Copy Propagation

One forward pass per function. Maintains a mapping from virtual
register to the virtual register it is a copy of. Replaces all uses
of a copy with the original source.

**Invariant:** never propagate through instructions with side effects.
A `copy` of the result of an `IR_LOAD` cannot be propagated past
another `IR_LOAD` or `IR_STORE` that may alias.

### Pass 4 — Dead Code Elimination

One backward pass per function. Maintains a bitset of live virtual
registers. Processes instructions in reverse order. An instruction is
dead if its destination register is not in the live set and the
instruction has no side effects.

**Side-effectful instructions that are NEVER eliminated:**
`IR_STORE`, `IR_PORT_WRITE`, `IR_PORT_READ`, `IR_ASM`, `IR_CALL`
(to procedures), `IR_CALL_INDIRECT`.

### Pass 5 — Common Subexpression Elimination (per block)

One forward pass per basic block. Maintains a hash table mapping
`(op, type, src1, src2)` to the virtual register that holds the result.
Before emitting an instruction, check the table. If a match is found,
emit `IR_COPY` from the existing result instead.

CSE is limited to a single basic block. Cross-block CSE requires
dominance analysis and is not implemented.

### Pass 6 — Inlining

Performed before passes 3-5. Functions marked `@inline` are always
inlined. Small functions (under 10 IR instructions) called exactly once
may be inlined at the compiler's discretion.

Inlining copies the callee's IR into the caller with fresh virtual
registers. Parameters are replaced with `IR_COPY` from the call's
argument registers. The callee's return value becomes the call's result.

### Pass 7 — Peephole Optimization

Performed after code generation, on the final machine code instruction
stream. A sliding window of 1-3 instructions scans for known bad
patterns and replaces them with better sequences.

Patterns:
- `mov rX, rX` → deleted
- `add rX, 0` → deleted
- `mov rX, imm; push rX` → `push imm`
- `cmp; setcc rX; test rX, rX; jcc` → `cmp; jcc`

### Purity Verifier (not an optimization — a correctness check)

After IR generation, before optimization, every pure function is
verified. The verifier scans all instructions in all basic blocks and
reports an error if any of the following appear in a pure function:

- `IR_STORE` to a global variable
- `IR_PORT_WRITE` or `IR_PORT_READ`
- `IR_ASM` with the volatile flag set
- `IR_CALL` to a function marked as a procedure

This catches purity violations that the semantic analysis phase
may have missed due to function pointers or indirect calls.

---

## 11. Phase 6 — Code Generation

**Input:** Optimized IR
**Output:** x86-64 machine code bytes in a buffer
**File:** `src/implementation/codegen.c`

### Register Allocation — Linear Scan

forgec uses linear scan register allocation. This algorithm was chosen
because it produces good-quality register assignments with a simple,
bounded implementation.

**Algorithm:**

1. Compute the live range of each virtual register: the instruction
   index where it is first defined to the instruction index where it
   is last used.

2. Sort virtual registers by start of live range.

3. Walk through virtual registers in order. For each:
   - Expire any physical registers whose associated virtual register's
     live range has ended (add them back to the free pool).
   - If a free physical register is available, assign it.
   - If no physical register is available, spill the virtual register
     with the longest remaining live range to the stack.

4. Spilled registers are accessed via `[rbp - offset]` memory operands.

**Available physical registers:**

Caller-saved (prefer these — no save/restore needed in leaf functions):
`rax rdi rsi rdx rcx r8 r9 r10 r11`

Callee-saved (use when caller-saved are exhausted):
`rbx r12 r13 r14 r15`

`rbp` is reserved for the frame pointer. `rsp` is reserved for the
stack pointer.

**Reference:** Poletto and Sarkar, "Linear Scan Register Allocation",
ACM TOPLAS 1999. 8 pages. Search for the PDF — freely available.

### Instruction Selection

For each IR instruction, the code generator emits one or more x86-64
instructions. The mapping is mostly one-to-one:

| IR Instruction | x86-64 Output |
|---------------|---------------|
| `IR_CONST` | `mov reg, imm` |
| `IR_ADD` | `add dst, src` |
| `IR_SUB` | `sub dst, src` |
| `IR_MUL` | `imul dst, src` |
| `IR_AND` | `and dst, src` |
| `IR_OR` | `or dst, src` |
| `IR_XOR` | `xor dst, src` |
| `IR_SHL` | `shl dst, cl` (count in CL) |
| `IR_SHR` | `shr dst, cl` or `sar dst, cl` |
| `IR_LOAD` | `mov dst, [src]` |
| `IR_STORE` | `mov [dst], src` |
| `IR_ADDR` | `lea dst, [symbol]` |
| `IR_CALL` | `call symbol` |
| `IR_RET` | `mov rax, src; ret` |
| `IR_JUMP` | `jmp label` |
| `IR_JUMP_IF` | `test src, src; jnz label` |
| `IR_CMP_EQ` | `cmp a, b; sete dst` |
| `IR_PORT_WRITE` | `out dx, al` / `out dx, ax` / `out dx, eax` |
| `IR_PORT_READ` | `in al, dx` / `in ax, dx` / `in eax, dx` |
| `IR_ASM` | verbatim passthrough to output |

Division (`IR_DIV`, `IR_MOD`) requires special handling: the dividend
must be in `rdx:rax`, the result lands in `rax` (quotient) or `rdx`
(remainder). The code generator saves and restores `rdx` around
division instructions.

Shifts require the count in `CL`. The code generator moves the shift
count to `RCX` before emitting the shift instruction.

### Function Prologue and Epilogue

Standard prologue (emitted for all functions except `@naked`):

```asm
push rbp
mov  rbp, rsp
sub  rsp, N     ; N = total stack frame size (aligned to 16 bytes)
```

Standard epilogue:

```asm
mov  rsp, rbp
pop  rbp
ret
```

`@naked` functions: no prologue or epilogue is emitted. The programmer
is fully responsible for stack management via inline assembly.

`@interrupt` functions: prologue saves all caller-saved registers in
addition to the standard frame setup. Epilogue restores them in reverse
order. Returns with `iretq` instead of `ret`.

### x86-64 Instruction Encoding

The code generator emits raw machine code bytes directly. Each
instruction family has a corresponding emit function:

```c
void emit_mov_reg_imm64(Buffer* b, Reg dst, uint64_t imm);
void emit_mov_reg_reg(Buffer* b, Reg dst, Reg src);
void emit_mov_reg_mem(Buffer* b, Reg dst, Reg base, int32_t disp);
void emit_mov_mem_reg(Buffer* b, Reg base, int32_t disp, Reg src);
void emit_add_reg_reg(Buffer* b, Reg dst, Reg src);
void emit_add_reg_imm32(Buffer* b, Reg dst, int32_t imm);
void emit_call_rel32(Buffer* b, int32_t rel);
/* ... approximately 40 emit functions total */
```

The Intel Software Developer Manual Volume 2 is the authoritative
reference for instruction encodings. The ModRM byte, REX prefix, and
SIB byte are the main encoding concepts. Learn these three and you
can encode any x86-64 instruction.

**REX prefix:** Required for 64-bit operands and registers R8-R15.
Format: `0100WRXB` where W=64-bit, R=reg field extension,
X=SIB index extension, B=rm/base extension.

**ModRM byte:** Encodes operand addressing. Format: `mod(2) reg(3) rm(3)`.
- `mod=11` — register operand
- `mod=01` — memory with 8-bit displacement
- `mod=10` — memory with 32-bit displacement
- `mod=00` — memory with no displacement (or RIP-relative)

---

## 12. Phase 7 — ELF Emission

**Input:** Machine code bytes, symbol table, relocation list
**Output:** ELF64 object file
**File:** `src/implementation/elf.c`

### ELF64 Structure

An ELF64 object file contains:

```
ELF Header          (64 bytes)
Section data:
  .text             — machine code
  .rodata           — string literals, read-only data
  .data             — initialized mutable globals
  .bss              — zero-initialized globals (size only)
  .symtab           — symbol table
  .strtab           — string table for symbol names
  .shstrtab         — string table for section names
  .rela.text        — relocation entries for .text
Section Header Table
```

### Relocations

When the code generator encounters a call to an external function or
a reference to a global variable, it cannot know the final address.
It emits a relocation entry:

```c
typedef struct {
    uint64_t offset;    /* offset in .text where the fix-up goes */
    uint64_t symbol;    /* index into .symtab */
    uint64_t type;      /* relocation type (R_X86_64_PC32, etc.) */
    int64_t  addend;    /* added to the symbol value */
} Rela;
```

The system linker (`ld`) resolves relocations when linking object files
into a final binary.

Common relocation types used by forgec:
- `R_X86_64_PC32` — 32-bit PC-relative (for `call` and `jmp`)
- `R_X86_64_64` — 64-bit absolute (for pointer constants)
- `R_X86_64_32S` — 32-bit sign-extended absolute

**References:**
- ELF-64 Object File Format: uclibc.org/docs/elf-64-gen.pdf
- "Linkers and Loaders" by John Levine: iecc.com/linker (free)

---

## 13. The IR Design

### Why Non-SSA

SSA form requires that every virtual register is assigned exactly once.
When a variable is assigned in two different branches of an `if`, SSA
inserts a "phi node" at the join point to select the correct value.

Phi nodes require dominance analysis to place correctly. Dominance
analysis requires building the full control flow graph and computing
dominator trees. This is correct and powerful but adds significant
implementation complexity.

For the bootstrap compiler, the benefit (enabling global value
numbering and more precise DCE) does not justify the cost. Non-SSA IR
with per-block optimizations is sufficient to produce good kernel code.

SSA can be added in a later version when the compiler is self-hosting
and the implementation complexity is less critical.

### Why Flat (Not Tree IR)

A tree IR mirrors the AST structure and is easy to generate but hard
to optimize — you cannot easily look at a sequence of operations and
eliminate redundancies when they're spread across a tree. A flat IR
linearizes execution order and makes data flow visible.

### Why Basic Blocks

Pure flat IR (no block structure) is simpler but loses information
about control flow. Without basic blocks you cannot compute live ranges
for register allocation, cannot perform DCE correctly across branches,
and cannot implement inlining correctly.

Basic blocks add minimal complexity — a basic block is just an array
of instructions with a label — but enable all the optimizations and
the register allocator.

---

## 14. Register Allocation

See Phase 6 above for the linear scan algorithm description.

### Calling Convention Integration

The register allocator must respect the System V AMD64 calling
convention. At function entry, parameters arrive in specific registers:

```
Parameter 1: rdi
Parameter 2: rsi
Parameter 3: rdx
Parameter 4: rcx
Parameter 5: r8
Parameter 6: r9
Parameters 7+: stack (right to left)
```

Return values:
```
First return value:  rax
Second return value: rdx  (for multiple return values ≤ 16 bytes)
```

The register allocator pre-assigns these virtual registers to their
physical homes at function entry. The linear scan algorithm then
allocates remaining virtual registers from the pool of caller-saved
registers, falling back to callee-saved registers, then spilling.

### Stack Frame Layout

```
[rbp + 16]   parameter 7 (if any)
[rbp +  8]   return address (pushed by call)
[rbp +  0]   saved rbp (pushed by prologue)
[rbp -  8]   first spilled virtual register
[rbp - 16]   second spilled virtual register
...
[rbp - N]    last spilled virtual register
[rsp]        (stack pointer — 16-byte aligned)
```

---

## 15. The x86-64 Backend

### Implemented Instructions

forgec implements a subset of x86-64 sufficient to compile Forge.
The full set is approximately 40 instruction families:

**Data movement:** `mov` (register-register, register-immediate,
register-memory, memory-register), `movzx`, `movsx`, `lea`, `push`,
`pop`, `xchg`

**Arithmetic:** `add`, `sub`, `imul`, `idiv`, `div`, `neg`, `inc`,
`dec`

**Bitwise:** `and`, `or`, `xor`, `not`, `shl`, `shr`, `sar`

**Comparison:** `cmp`, `test`, `sete`, `setne`, `setl`, `setg`,
`setle`, `setge`, `setb`, `setae`

**Control flow:** `jmp`, `je`, `jne`, `jl`, `jg`, `jle`, `jge`,
`jb`, `jae`, `call`, `ret`, `iretq`

**Stack:** `enter`, `leave` (or equivalent push/sub sequences)

**System:** `hlt`, `sti`, `cli`, `nop`, `cpuid`, `rdmsr`, `wrmsr`,
`invlpg`

**I/O:** `in` (byte/word/dword), `out` (byte/word/dword)

### Reference Documents

1. **Intel Software Developer Manual Volume 2A and 2B** — the
   authoritative encoding reference. Free PDF from intel.com.
   Download and keep offline. You will refer to it constantly
   while writing codegen.c.

2. **System V AMD64 ABI** — gitlab.com/x86-psABIs/x86-64-ABI
   Read sections 3.2 (calling sequence) and 3.4 (initialization).

3. **OSDev Wiki x86-64 instruction reference** — wiki.osdev.org
   Useful for quick lookups during development.

---

## 16. Purity Enforcement

Forge's purity system — functions are pure by default, procedures are
`@mut` — is enforced at two points:

**During semantic analysis:** The type checker tracks whether each
called function is a procedure. A call to a procedure from a pure
function is flagged as an error.

**During IR verification:** After IR generation, the verifier scans
all instructions in pure functions for disallowed operations:
global stores, port I/O, volatile asm, procedure calls.

The two-pass approach catches errors that fall through the AST-level
check (function pointers, indirect calls) and provides a correctness
double-check on the IR generator itself.

### What Counts as a Side Effect

| Operation | Side effect? | Allowed in pure? |
|-----------|-------------|-----------------|
| Read local variable | No | Yes |
| Write local variable | No (local) | Yes |
| Read global variable | No | Yes |
| Write global variable | Yes | No |
| Write through pointer param | No (explicit) | Yes |
| Write through global pointer | Yes | No |
| Port read (`.read()`) | Yes | No |
| Port write (`<-`) | Yes | No |
| Non-volatile asm (no output) | No | Yes |
| Volatile asm | Yes | No |
| Atomic load | No | Yes |
| Atomic store / RMW | Yes | No |
| Call to pure function | No | Yes |
| Call to procedure | Yes | No |

---

## 17. What forgec Deliberately Does Not Do

These are features common in other compilers that forgec intentionally
omits. Each omission keeps the implementation smaller and simpler.

| Feature | Reason omitted |
|---------|---------------|
| **SSA form** | Adds significant complexity for optimization gains not needed in a bootstrap compiler |
| **Multiple error recovery** | Stops at first error — simpler, avoids cascading false errors |
| **Parallel compilation** | Module system eliminates the need — each module compiles once |
| **Link-time optimization** | Complex infrastructure, limited benefit for kernel code |
| **Debug info (DWARF)** | Useful but not required for bootstrap — add later |
| **Profile-guided optimization** | Requires runtime infrastructure — not freestanding |
| **Auto-vectorization** | Kernel code doesn't benefit — use inline asm when needed |
| **Loop optimizations** | Kernel hot loops are few and can use `@inline` + manual optimization |
| **Global register allocation** | Linear scan per function is sufficient |
| **Interprocedural analysis** | Module-at-a-time compilation is sufficient |

---

## 18. Bootstrapping

The bootstrap process for forgec:

**Stage 0 — C bootstrap compiler**
The current `forgec` written in C99. Compiles a subset of Forge
sufficient to compile the stage 1 compiler. Does not need to implement
the full language — only the features the stage 1 compiler uses.

**Stage 1 — Forge compiler written in Forge**
`forgec` rewritten in Forge, compiled by the stage 0 compiler.
This is the self-hosting compiler. Once stage 1 compiles cleanly and
passes the test suite, the C compiler is retired.

**Stage 2 — Forge compiler compiled by stage 1**
The stage 1 compiler compiles itself. If the output is identical to
the stage 1 binary (modulo timestamps and addresses), the compiler is
verified correct — this is the "bootstrap test."

The stage 0 compiler is designed with self-hosting in mind:
- No features that cannot be expressed in Forge are used in the
  implementation
- Data structures are simple (arenas, arrays, not complex C idioms)
- The IR and codegen are designed to be straightforward to rewrite
  in Forge

### Subset for Stage 0

The stage 0 compiler only needs to compile the features used by the
stage 1 compiler source. Expected subset:

- Structs, enums, unions
- Procedures and pure functions
- Multiple return values
- Pointers and arrays
- Basic control flow (if, loop, while, for, match)
- Inline assembly (passthrough)
- Module imports
- All primitive types

Not required for stage 0:
- Closures
- Comptime evaluation beyond constants
- All attributes (only `@mut`, `@pub`, `@noreturn` needed)

---

## 19. Testing Strategy

### Phase-Level Testing

Each phase has a `--dump-*` flag that halts the pipeline and prints
the current representation. Tests compare this output against expected
output files.

```sh
bin/forgec --dump-tokens tests/lexer/basic.fg > got.txt
diff tests/lexer/basic.fg.expected got.txt
```

### End-to-End Testing

Compile a Forge file to an object file, link it, run it, check the
output. For freestanding code, boot in QEMU and check serial output.

```sh
bin/forgec tests/e2e/arith.fg -o tests/e2e/arith.o
ld -o tests/e2e/arith tests/e2e/arith.o
./tests/e2e/arith
echo $?   # check exit code
```

### The Correctness Bar

The bootstrap compiler must produce correct code. "Correct" means:
- Every pure function that should be rejected is rejected
- Every valid program produces output matching the specification
- Multiple return values land in the correct registers
- `@volatile` accesses are never eliminated
- `@noreturn` functions are verified
- Integer overflow wraps, not traps (unless the programmer requested
  trapping behaviour)

The compiler does not need to produce optimal code. It needs to produce
correct code.

---

## 20. Further Reading

### Compiler Theory

- **"Crafting Interpreters" — Robert Nystrom** (craftinginterpreters.com)
  Free online. Chapters 14-17 cover bytecode IR and virtual machines.
  The clearest explanation of going from AST to IR.

- **"Engineering a Compiler" — Cooper and Torczon**
  Chapters 5 (IR), 6 (code shape), 13 (register allocation).
  The standard graduate textbook. Dense but precise.

- **"Modern Compiler Implementation in C" — Andrew Appel**
  Alternative to Cooper/Torczon. More implementation-focused.

### Reference Implementations

- **chibicc** (github.com/rui314/chibicc) — Complete C compiler in
  ~10k lines of clean C. Written to be readable. The closest
  reference to what forgec will look like.

- **8cc** (github.com/rui314/8cc) — Earlier, simpler C compiler by
  the same author. Read this first.

- **tcc** (bellard.org/tcc) — Fabrice Bellard's Tiny C Compiler.
  Demonstrates what a complete, fast, minimal C compiler looks like.

### x86-64 and ELF

- **Intel Software Developer Manual Vol. 2** — intel.com/sdm
  The authoritative instruction encoding reference. Download the PDF.

- **System V AMD64 ABI** — gitlab.com/x86-psABIs/x86-64-ABI
  Calling conventions, register usage, struct layout.

- **"Linkers and Loaders" — John Levine** (iecc.com/linker)
  Free online. Chapters 3-4 cover ELF format.

- **ELF-64 Object File Format** — uclibc.org/docs/elf-64-gen.pdf
  The 50-page specification. Read before writing elf.c.

### Register Allocation

- **"Linear Scan Register Allocation" — Poletto and Sarkar (1999)**
  The original paper. 8 pages. Search for the PDF. This is the exact
  algorithm forgec implements.

### Philosophy

- **"The Rise of Worse is Better" — Richard Gabriel (1991)**
  jwz.org/doc/worse-is-better.html
  The design philosophy underlying every decision in forgec.

- **"Unix and Beyond: An Interview with Ken Thompson" (1999)**
  The source of the "C is a good systems language" tradition —
  and a reminder of what that tradition's limitations are.

---

*forgec Internals Reference, Version 1.0*
*Part of the Forge Programming Language project*
*GNU General Public License v3.0*
