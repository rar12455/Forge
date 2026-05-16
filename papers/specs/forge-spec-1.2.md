# The Forge Programming Language Specification

**Version:** 1.2
**Status:** Normative Draft
**License:** GNU General Public License v3.0
**Compiler:** `forgec` (bootstrap implementation in C99)
**File Extension:** `.fg`
**Target:** x86-64, freestanding (no libc, no OS assumed)

---

## Changelog

| Version | Changes |
|---------|---------|
| 1.2 | Removed implicit function return — `return` always required. Replaced `never` type and trailing `->` syntax with `@noreturn` attribute. Literal type suffixes are optional for local variable declarations when the type is unambiguous from the declared type. Port read syntax unified to `.read()` method. Import syntax standardized with parentheses on function names. Closures no longer use trailing arrow syntax. `panic()` and `memset()` moved to standard library. Purity rules for `defer` documented. §8.5 prose filled in. All audit fixes applied. |
| 1.1 | Removed `fn` keyword. Replaced `u8`/`i8` with `uint8`/`int8`. Procedures declared with `@mut`. Purity by default. Removed generics, pattern matching, error handling (deferred). |
| 1.0 | Initial normative draft. |

---

## Table of Contents

1. [Design Principles](#1-design-principles)
2. [Standard Library Relationship](#2-standard-library-relationship)
3. [Lexical Structure](#3-lexical-structure)
4. [Type System](#4-type-system)
5. [Variables and Constants](#5-variables-and-constants)
6. [Functions and Procedures](#6-functions-and-procedures)
7. [Control Flow](#7-control-flow)
8. [Pointers and Memory](#8-pointers-and-memory)
9. [Structs](#9-structs)
10. [Unions](#10-unions)
11. [Enums](#11-enums)
12. [Modules](#12-modules)
13. [Attributes](#13-attributes)
14. [Inline Assembly](#14-inline-assembly)
15. [Comptime](#15-comptime)
16. [OS and Hardware Primitives](#16-os-and-hardware-primitives)
17. [Memory Model](#17-memory-model)
18. [Undefined Behaviour](#18-undefined-behaviour)
19. [ABI and Calling Conventions](#19-abi-and-calling-conventions)
20. [Object Files and Linking](#20-object-files-and-linking)
21. [Compiler Attributes Reference](#21-compiler-attributes-reference)
22. [What Forge Deliberately Omits](#22-what-forge-deliberately-omits)
23. [Appendix A — Kernel Entry Point Example](#23-appendix-a--kernel-entry-point-example)
24. [Appendix B — Physical Frame Allocator Example](#24-appendix-b--physical-frame-allocator-example)
25. [Appendix C — Compile-Time Guarantees](#25-appendix-c--compile-time-guarantees)

---

## 1. Design Principles

These principles are ordered by priority. When two principles conflict, the
higher-numbered one yields to the lower-numbered one.

### 1.1 No Undefined Behaviour

Every operation in Forge has a defined result. The compiler never uses the
absence of a definition as an optimization license. There is no concept of
undefined behaviour in this specification. Every edge case is specified.

Signed integer overflow wraps by default. Division by zero traps.
Out-of-bounds access traps. Uninitialized reads are a compile error unless
explicitly opted out of with `@uninit`. Null pointer dereferences cannot
occur because null pointers do not exist as plain values.

### 1.2 Explicit Over Implicit

If an operation costs cycles, allocates memory, changes a type, copies a
value, or has a side effect — it must be written explicitly in the source.

- No type inference on expressions
- No implicit integer promotion
- No implicit conversions between any types
- No hidden copies of values
- No hidden allocations
- No hidden control flow
- No implicit return — every function exit must be an explicit `return`

The one deliberate exception: integer literal suffixes are optional when
declaring a local variable, because the type is already written explicitly
on the left side of the declaration. The literal's type is inferred from
the declared type, not from the value. See §5.3.

### 1.3 Zero-Cost Abstractions

Every abstraction in Forge compiles to machine code identical to what a
competent programmer would write by hand in C. If an abstraction cannot
be zero-cost, it is not in the language.

### 1.4 Purity by Default

All functions in Forge are **pure by default**. A pure function may not
write to global variables, perform I/O, or call procedures. The compiler
enforces this statically.

A function that has side effects must be declared with `@mut`. These are
called **procedures**. The `@mut` attribute is both the parser's signal
that this is a procedure declaration and the permission to have side
effects.

Writing through pointer parameters is permitted in pure functions because
the effect is explicit in the signature — the caller passed the pointer
knowing it may be written.

### 1.5 Freestanding First

Forge does not assume the existence of an operating system, a C runtime,
a heap, or a standard library. Every language feature works at interrupt
level, in a bootloader, or on bare metal with no prior initialization.

The Forge standard library (see §2) is an optional companion that provides
common utilities. It makes no assumption of an OS either — it is designed
to be used on bare metal.

### 1.6 Predictable Code Generation

The programmer should be able to predict the assembly output of any Forge
function without running the compiler. There is no hidden virtual dispatch,
no hidden state machine generation, and no compiler transformations that
change observable behaviour of a correct program beyond reordering and
eliminating redundant pure operations.

---

## 2. Standard Library Relationship

Forge has no implicit standard library. Nothing is available without an
explicit import. The Forge standard library (`libforge`) is an optional
collection of modules that a programmer may import as needed. It is not
linked by default.

`libforge` is written in Forge. It assumes no OS, no libc, and no heap
unless the programmer provides one. It is organized as ordinary Forge
modules and is subject to the same language rules as any other code.

### 2.1 Key Standard Library Modules

The following modules are part of `libforge` and are referenced in this
specification:

| Module | Purpose |
|--------|---------|
| `forge.panic` | Kernel panic and assertion support |
| `forge.mem` | `memset`, `memcpy`, `memcmp` and related utilities |
| `forge.fmt` | Formatted output (requires a writer procedure) |
| `forge.io` | Serial and port I/O helpers |
| `forge.atomic` | Atomic operation wrappers |

### 2.2 `panic()`

`panic()` is a procedure provided by `forge.panic`. It is not a language
built-in. It must be imported before use.

```forge
import forge.panic.{panic};
```

`panic()` is declared as:

```forge
@pub @noreturn @mut panic([]uint8 msg) {
    // implementation-defined: write message to serial, halt all CPUs
}
```

On bare metal with no OS, the default implementation writes the message to
the first serial port (0x3F8) and executes `hlt` in a loop. The programmer
may replace it by providing their own implementation.

Because `panic()` is `@noreturn`, the compiler knows any code after a
`panic()` call is unreachable and may omit it.

### 2.3 `memset()` and `memcpy()`

`memset()` and `memcpy()` are provided by `forge.mem`. They are not
language built-ins.

```forge
import forge.mem.{memset, memcpy};
```

They are declared as:

```forge
@pub @mut memset(@mut uint8* dst, uint8 val, usize len) {
    for i in 0..len { dst[i] = val; }
}

@pub @mut memcpy(@mut @restrict uint8* dst,
                 @const @restrict uint8* src,
                 usize len) {
    for i in 0..len { dst[i] = src[i]; }
}
```

The compiler may replace calls to `memset` and `memcpy` with optimized
inline sequences or hardware-specific instructions when it can prove the
semantics are preserved.

---

## 3. Lexical Structure

### 3.1 Source Files

Forge source files are UTF-8 encoded text with the `.fg` extension. Line
endings may be `LF` or `CRLF`. The compiler normalizes all line endings
to `LF` before processing.

### 3.2 Comments

```forge
// Single-line comment — extends to end of line

/*
   Block comment.
   Block comments nest: /* this is valid */ still in comment
*/

/// Documentation comment.
/// Attached to the immediately following declaration.
/// Content is treated as Markdown by documentation tooling.
uint32 add(uint32 a, uint32 b) {
    return a + b;
}
```

Comments are stripped during lexing and have no effect on the program.

### 3.3 Keywords

The following identifiers are reserved and may not be used as
user-defined names:

```
as          asm         break       comptime    constexpr
continue    defer       defer_err   else        enum
extern      false       for         if          import
in          loop        match       mod         not
or          and         return      struct      true
union       while
```

The following words are **not** keywords. They have no special meaning
to the parser and may be used as identifiers, though doing so is strongly
discouraged:

```
mut     uninit
```

### 3.4 Identifiers

```
identifier ::= [a-zA-Z_][a-zA-Z0-9_]*
```

Identifiers are case-sensitive. Identifiers beginning with two underscores
(`__`) are reserved for the linker and platform ABI.

### 3.5 Integer Literals

Integer literal suffixes are **required** in all contexts except local
variable declarations, where the type may be inferred from the declared
type (see §5.3). Underscores may appear anywhere in a digit sequence for
readability.

```forge
// In local variable declarations — suffix optional
uint32 a = 1_000_000;
uint32 b = 0xFF_EC_00_01;   // hex
uint8  c = 0b1010_1100;     // binary
uint16 d = 0o755;           // octal

// In all other contexts — suffix required
uint32 e = a + 1uint32;
uint8  f = some_array[2uint8];

// Explicit suffixes always accepted
uint32 g = 42uint32;
uptr   h = 0xFFFF_FFFF_8000_0000uptr;
int32  i = -1int32;
```

Valid suffixes: `uint8` `uint16` `uint32` `uint64` `uint128`
`int8` `int16` `int32` `int64` `int128` `usize` `isize` `uptr`

### 3.6 Float Literals

Float literals require an explicit suffix and must contain a decimal
point. In local variable declarations, the suffix may be omitted if the
declared type is unambiguous.

```forge
float32 a = 3.14;           // suffix inferred from declared type
float64 b = 2.718_281_828;
float32 c = 42.0float32;    // explicit suffix — always valid
```

Valid suffixes: `float32` `float64`

### 3.7 Boolean Literals

`true` and `false` are keywords and the only values of type `bool`.
`bool` is not an integer. There is no implicit conversion between `bool`
and any integer type.

### 3.8 Character Literals

A character literal produces a value of type `uint32` containing the
Unicode codepoint of the character.

```forge
uint32 a = 'A';             // 65
uint32 b = '\n';            // 10
uint32 c = '\t';            // 9
uint32 d = '\\';            // 92
uint32 e = '\u{1F600}';     // Unicode codepoint
uint32 f = '\x41';          // hex byte — 65
```

### 3.9 String Literals

A string literal produces a value of type `[]uint8`. The string is
UTF-8 encoded with no null terminator. Length is available via `.len`.

```forge
[]uint8 a = "hello, kernel";
[]uint8 b = "line one\nline two";
[]uint8 c = r"raw string — no \n processing";
```

String literals are stored in `.rodata` and are immutable. Writing
through a string literal pointer is a compile error.

### 3.10 Operators

```
Arithmetic:    +   -   *   /   %
Bitwise:       &   |   ^   ~   <<   >>
Comparison:    ==  !=  <   >   <=   >=
Logical:       and   or   not
Assignment:    =   +=  -=  *=  /=  &=  |=  ^=  <<=  >>=
Range:         ..   ..=
Pointer:       *  (dereference)    &  (address-of)
Cast:          as
Port write:    <-
Member:        .
Index:         [ ]
```

Operator precedence follows C conventions with these differences:
- `not` binds tighter than `and` and `or`
- `and` and `or` are keywords, not symbols
- There is no comma operator

---

## 4. Type System

Forge is statically typed. Every expression has a type known at compile
time. There is no type inference on expressions — all declarations require
an explicit type. There are no implicit conversions between any types.
Every type change must be written explicitly with `as`.

### 4.1 Integer Types

| Type | Width | Signedness | Use |
|------|-------|------------|-----|
| `uint8` | 8 bits | unsigned | byte, port value |
| `uint16` | 16 bits | unsigned | port address, short |
| `uint32` | 32 bits | unsigned | general integer |
| `uint64` | 64 bits | unsigned | general integer |
| `uint128` | 128 bits | unsigned | wide integer |
| `int8` | 8 bits | signed | signed byte |
| `int16` | 16 bits | signed | signed short |
| `int32` | 32 bits | signed | signed integer |
| `int64` | 64 bits | signed | signed long |
| `int128` | 128 bits | signed | signed wide integer |
| `usize` | 64 bits on x86-64 | unsigned | lengths, indices |
| `isize` | 64 bits on x86-64 | signed | signed size |
| `uptr` | 64 bits on x86-64 | unsigned | raw hardware addresses |

`usize` is for array lengths, loop indices, and memory sizes.
`uptr` is for raw hardware addresses and address arithmetic.
They are distinct types with no implicit conversion between them.

**Integer overflow** is defined in Forge. Both signed and unsigned
integers wrap on overflow by default (two's complement). Explicit
behaviour can be requested at the call site:

```forge
uint32 a = x.wrapping_add(y);    // defined wrap
uint32 b = x.saturating_add(y);  // clamps to type maximum
```

### 4.2 Float Types

| Type | Width | Standard |
|------|-------|----------|
| `float32` | 32 bits | IEEE 754 single precision |
| `float64` | 64 bits | IEEE 754 double precision |

Float operations follow IEEE 754 strictly. The compiler does not
reorder or fuse float operations in ways that change the result.

### 4.3 Boolean Type

`bool` is a 1-byte type. Its only values are `true` and `false`.
It is not an integer. There is no implicit conversion between `bool`
and any integer type.

### 4.4 Procedures and the Absence of `void`

There is no `void` keyword in Forge. A function that returns no value
is a **procedure** and is declared with `@mut` and no return type.
The absence of a return type, combined with `@mut`, unambiguously
identifies a procedure declaration. See §6.

### 4.5 The `@noreturn` Attribute

Functions or procedures that never return — because they loop forever,
halt the CPU, or call `panic()` — are annotated with `@noreturn`. This
is an attribute, not a type. It carries no syntax other than the
attribute itself.

```forge
@noreturn @mut halt() {
    loop { asm("hlt"); }
}

@noreturn @mut panic([]uint8 msg) {
    // write msg to serial, halt
}
```

The compiler uses `@noreturn` to:
- Mark all code after a call to a `@noreturn` function as unreachable
- Allow a `@noreturn` call to satisfy any type in a branch
- Warn if a `@noreturn` function contains a reachable `return`

### 4.6 Pointer Types

```forge
uint32*           // raw pointer to mutable uint32
@const uint32*    // raw pointer to immutable uint32
@volatile uint32* // MMIO pointer — accesses never optimized away
```

**Pointers in Forge are always non-null.** A nullable pointer is
expressed as `Option<*T>` (to be specified in a future version).
`NULL` does not exist as a value in Forge.

Pointer arithmetic is performed in units of `sizeof(T)`. For byte-level
arithmetic, cast to `uint8*` first.

### 4.7 Slice Types

A slice is a fat pointer: `(ptr: *T, len: usize)`. All slice accesses
are bounds-checked — at compile time where possible, trapping at runtime
otherwise.

```forge
[]uint8         // mutable slice of uint8
[]@const uint8  // immutable slice of uint8
```

Fields: `.ptr` — raw pointer to first element. `.len` — element count.

### 4.8 Array Types

Arrays are fixed-size and stack-allocated. The size must be a
`constexpr` value known at compile time.

```forge
[uint8; 512]   buf;
[uint32; 1024] table;
[GDTEntry; 8]  gdt;
```

An array of type `[T; N]` may be coerced to a slice `[]T` implicitly
in slice contexts.

### 4.9 Tuple Types

```forge
(uint32, bool, uint8) t  = (42, true, 0xFF);
uint32                lo = t.0;
bool                  b  = t.1;
uint8                 hi = t.2;
```

### 4.10 Function Pointer Types

Pure function pointers and procedure pointers are distinct types.

```forge
(uint32, uint32) -> uint32   // pure function: two uint32 → uint32
@mut (uint32) -> uint32      // procedure: uint32 → uint32 with side effects
@mut ()                      // procedure pointer: no return value
```

### 4.11 Bitfield Integers

Inside `@packed` or `@repr("C")` structs, integers of arbitrary bit
width between 1 and 64 may be used. The syntax is `uintN` or `intN`
where N is the bit width.

```forge
@packed
struct PageTableEntry {
    uint1  present,
    uint1  writable,
    uint1  user_access,
    uint1  write_through,
    uint1  cache_disable,
    uint1  accessed,
    uint1  dirty,
    uint1  huge_page,
    uint1  global,
    uint3  _available,
    uint52 frame_number,
}
```

Bitfield integers are valid only inside struct declarations.
They cannot be used as standalone variable types.

### 4.12 Atomic Types

`atomic<T>` wraps an integer or pointer type and provides indivisible
read-modify-write operations with explicit memory ordering. See §16.5.

### 4.13 Port Type

`port<T>` represents an x86 I/O port. `T` must be `uint8`, `uint16`,
or `uint32`. See §16.1.

### 4.14 Type Casting

All type conversions are explicit and use `as`. Every cast has a
defined result.

```forge
uint32  a = 0xFFFFFFFFuint32;
uint8   b = a as uint8;           // truncates — 0xFF
int32   c = a as int32;           // reinterpret bits — -1
uint64  d = a as uint64;          // zero-extends — 0x00000000FFFFFFFF
float32 e = 42uint32 as float32;  // converts — 42.0
uptr    f = ptr as uptr;          // pointer to integer
uint32* g = f as uint32*;         // integer to pointer
```

---

## 5. Variables and Constants

### 5.1 Declaration Syntax

Variables are declared with the type first, then the name — identical
to C. There is no type inference on expressions. The type is always
explicitly declared.

```
[@attributes] type name [= expression] ;
```

```forge
uint32      x;        // immutable, zero-initialized
uint32*     ptr;      // immutable pointer variable, zero-initialized
[uint8;512] buf;      // immutable array, all bytes zero
GDTEntry    entry;    // immutable struct, all fields zero
```

### 5.2 Mutability — `@mut`

All variables are **immutable by default**. `@mut` makes a variable
mutable after initialization.

```forge
uint32 x = 10;
x = 20;              // COMPILE ERROR: x is immutable

@mut uint32 y = 10;
y = 20;              // OK
y += 5;              // OK
```

### 5.3 Integer Literal Suffix Rules

Integer literal suffixes follow these rules:

**Local variable declarations** — the suffix is optional when the
declared type on the left makes the type of the literal unambiguous.
The compiler infers the literal's type from the declared type.

```forge
uint32 x  = 255;      // OK — 255 inferred as uint32
uint8  y  = 255;      // OK — 255 inferred as uint8, fits
uint8  z  = 256;      // COMPILE ERROR — 256 does not fit uint8
int32  w  = -1;       // OK — -1 inferred as int32
uint32 bad = -1;      // COMPILE ERROR — -1 is not valid for uint32
```

**All other contexts** — suffix is required. This includes:
- Integer literals in expressions (`a + 1uint32`)
- Integer literals in function call arguments (`f(4096usize)`)
- Integer literals in struct field initializers
- Integer literals in array size positions
- Integer literals in `constexpr` declarations
- Integer literals in `comptime` blocks

```forge
constexpr PAGE_SIZE: usize = 4096usize;   // suffix required
uint32 result = x + 1uint32;              // suffix required
```

### 5.4 Zero Initialization

All variables are zero-initialized by default unless `@uninit` is
specified:

- Integer types → `0`
- Float types → `0.0`
- Boolean → `false`
- Pointer types → all-zero bits
- Array types → all elements zeroed
- Struct types → all fields zeroed
- Slice types → `.ptr` zeroed, `.len` = 0

The compiler always emits zero initialization explicitly. It does not
rely on BSS segment zeroing for stack variables.

### 5.5 Explicit Initialization

```forge
uint32    x     = 42;
bool      flag  = true;
[uint8;4] magic = { 0x7F, 0x45, 0x4C, 0x46 };   // ELF magic
Point     p     = Point { x: 10, y: 20 };
```

### 5.6 Uninitialized Variables — `@uninit`

`@uninit` opts out of zero initialization. The compiler tracks reads
of `@uninit` variables and emits a compile error if a variable may be
read before being written.

```forge
@uninit uint32        x;
@uninit [uint8; 4096] page_buf;

page_buf[0] = 0;      // OK — first write
uint8 v = page_buf[0]; // OK — read after write

@uninit uint32 z;
uint32 w = z;          // COMPILE ERROR: z may be uninitialized
```

### 5.7 Attribute Composition

All attributes are commutative. Order never matters:

```forge
@mut @uninit uint32 x;    // mutable, uninitialized
@uninit @mut uint32 x;    // identical — attribute order irrelevant
```

### 5.8 Compile-Time Constants — `constexpr`

`constexpr` declares a compile-time constant. Always immutable. Always
evaluated at compile time. Type suffix required (literals in `constexpr`
are not local variable declarations — §5.3 does not apply). Cannot be
passed by pointer.

```forge
constexpr PAGE_SIZE:   usize  = 4096usize;
constexpr PAGE_SHIFT:  uint32 = 12uint32;
constexpr KERNEL_BASE: uptr   = 0xFFFF_FFFF_8000_0000uptr;
constexpr MAX_CPUS:    uint32 = 256uint32;
constexpr PAGE_MASK:   uptr   = ~(PAGE_SIZE as uptr - 1uptr);
```

### 5.9 Variable Scope

Variables are scoped to the block (`{ }`) in which they are declared.
An inner declaration of the same name shadows the outer one. Shadowing
produces a compiler warning.

---

## 6. Functions and Procedures

Forge distinguishes two kinds of callable units:

- **Functions** — pure by default. May not write to global state,
  perform I/O, or call procedures. Writing through pointer parameters
  is allowed because the side effect is explicit in the signature.
- **Procedures** — declared with `@mut`. May write to global state,
  perform port I/O, call other procedures, and use `@volatile` assembly.

The compiler enforces purity statically. Calling a procedure from a
pure function is a compile error.

### 6.1 Function Declaration

The return type appears first, then the name, then parameters.
Parameter declarations use `type name` order.

```
[@attributes] return_type name ( [parameters] ) block
```

```forge
uint32 add(uint32 a, uint32 b) {
    return a + b;
}

uint8 checksum([]uint8 data) {
    @mut uint8 sum = 0;
    for byte in data {
        sum ^= byte;
    }
    return sum;
}
```

Every function must have an explicit `return` on every exit path. A
function body that reaches the end without a `return` is a compile
error.

### 6.2 Procedure Declaration

A procedure has no return type. The `@mut` attribute is required. It
serves as both the purity marker and the parser's signal that this is
a procedure declaration rather than a call.

```
@mut [other_attributes] name ( [parameters] ) block
```

```forge
@mut setup_gdt() {
    load_gdt(&gdt_descriptor);
}

@mut write_serial(uint8 byte) {
    uart_data <- byte;
}
```

The parser distinguishes a procedure declaration from a procedure call
by the presence of `@mut` before the identifier and a block body `{ }`.
A procedure call has neither.

### 6.3 Noreturn Functions and Procedures

`@noreturn` marks a function or procedure that never returns. It is
an attribute — it does not change the declaration syntax.

```forge
// Noreturn procedure (most common — halt has side effects)
@noreturn @mut halt() {
    @cpu.disable_interrupts();
    loop { asm("hlt"); }
}

// Noreturn pure function (rare — e.g. an infinite computation)
@noreturn spin() {
    loop {}
}
```

A `@noreturn` function or procedure must not contain a reachable
`return` statement. Violating this is a compile error.

### 6.4 Purity Rules

A pure function (no `@mut`) **may**:
- Read global variables and `constexpr` constants
- Write through pointer parameters
- Call other pure functions
- Execute pure inline assembly (no I/O, no `@volatile`)
- Allocate on the stack
- Use `defer` to call pure functions

A pure function **may not**:
- Write to global variables
- Perform port I/O (use `<-` or `.read()`)
- Call procedures (`@mut` functions)
- Execute `@volatile` assembly
- Use `defer` or `defer_err` to call procedures

A procedure (`@mut`) may do anything.

Violation of purity rules is a compile error.

```forge
@mut uint32 global_counter = 0;

// COMPILE ERROR: pure function writes global
uint32 bad_compute(uint32 x) {
    global_counter += 1uint32;
    return x * 2;
}

// OK: procedure
@mut tick() {
    global_counter += 1uint32;
}
```

### 6.5 Return Statement

`return` is required on every exit path of a function. There is no
implicit return. A function body that may reach its end without
executing a `return` is a compile error.

```forge
uint32 abs(int32 x) {
    if x < 0 {
        return (-x) as uint32;
    }
    return x as uint32;
}

// COMPILE ERROR: missing return on the else path
uint32 bad(int32 x) {
    if x < 0 {
        return 0;
    }
    // falls off end — compile error
}
```

Procedures do not return a value. They exit by reaching the end of
the block or via an explicit bare `return;`.

```forge
@mut reset_counter() {
    global_counter = 0uint32;
    return;   // explicit — valid but optional for procedures
}
```

### 6.6 Mutable Parameters

Parameters are immutable by default. `@mut` allows mutation of the
local copy inside the function body. This does not affect the caller.

```forge
// Modifies local copy only
uint32 increment(@mut uint32 x) {
    x += 1;
    return x;
}

// Modifies caller's variable via pointer
@mut increment_ptr(@mut uint32* x) {
    *x += 1uint32;
}
```

### 6.7 First-Class Functions

Functions and procedures are first-class values. The type encodes
purity.

```forge
// Pure function pointer
(uint32, uint32) -> uint32 fn_ptr = add;
uint32 result = fn_ptr(1, 2);

// Procedure pointer
@mut () proc_ptr = setup_gdt;
proc_ptr();
```

### 6.8 Closures

Closures capture variables from their enclosing scope. They use
`|parameters| { body }` syntax. Closures do not use trailing arrow
syntax for the return type — if a return type is needed it is inferred
from the body.

```forge
uint32 threshold = 128;

// Pure closure — captures threshold by copy
(uint32) -> bool above = |uint32 x| { return x > threshold; };

// Procedure closure — captures and modifies
@mut uint32 count = 0;
@mut () increment_count = @mut || { count += 1uint32; };
```

Closures capturing by copy are always permitted. Closures that would
need to outlive their enclosing scope are a compile error unless an
explicit allocator is provided.

### 6.9 Calling Conventions

The default calling convention is the System V AMD64 ABI.

```forge
uint32 default_abi(uint32 x) { return x; }

@cc("cdecl")    int32 cdecl_fn(int32 x) { return x; }
@cc("stdcall")  int32 stdcall_fn(int32 x) { return x; }
@cc("sysv")     int32 sysv_fn(int32 x) { return x; }
@cc("win64")    int32 win64_fn(int32 x) { return x; }
```

### 6.10 Extern Declarations

`extern` declares a symbol defined in another translation unit:

```forge
extern uint32 asm_helper(uint32 a, uint32 b);

@cc("cdecl")
extern int32 c_compat_fn(int32 x);
```

### 6.11 Visibility — `@pub`

All declarations are private to their module by default. `@pub` makes
a declaration visible to importers.

```forge
uint32 internal_helper(uptr addr) {   // private
    return addr as uint32 & 0xFFF;
}

@pub uint32 page_offset(uptr addr) {  // public
    return internal_helper(addr);
}

@pub @mut init_paging() { ... }       // public procedure
```

`@pub` is commutative with all other attributes:

```forge
@pub @inline uint32 fast(uint32 x) { return x; }
@inline @pub uint32 fast(uint32 x) { return x; }   // identical
```

---

## 7. Control Flow

### 7.1 If / Else

```forge
if condition {
    ...
} else if other {
    ...
} else {
    ...
}
```

The condition must be of type `bool`. There is no implicit conversion
from integer to `bool`. `if x` where `x` is an integer is a compile
error.

**If as expression.** Both branches must yield the same type. Each
branch must be a single expression — not a block with a `return`.

```forge
uint32 abs_x = if x >= 0 { x as uint32 } else { (-x) as uint32 };
```

If-expressions do not require `return`. The value of the selected
branch is the value of the expression. This is the only context where
a value is produced without `return` — it is an expression, not a
statement.

### 7.2 Loop

`loop` is an infinite loop. It must be exited with `break` or a call
to a `@noreturn` function.

```forge
loop {
    if done { break; }
    if skip { continue; }
    do_work();
}
```

**Loop as expression.** `break` may carry a value:

```forge
uint32 result = loop {
    uint32 v = compute();
    if v > 100 { break v; }
};
```

**Labeled loops:**

```forge
'outer: loop {
    loop {
        if cond { break 'outer; }
    }
}
```

### 7.3 While

```forge
while condition {
    do_work();
}
```

### 7.4 For

```forge
// Exclusive range
for i in 0..256uint32 {
    table[i] = 0;
}

// Inclusive range
for i in 0..=255uint32 {
    table[i] = 0;
}

// Slice iteration
for byte in buffer {
    checksum ^= byte;
}

// Slice iteration with index
for i, byte in buffer {
    if byte == 0xFF { return i; }
}
```

The loop variable is immutable by default.

### 7.5 Match

`match` performs exhaustive value matching. Every possible value of
the scrutinee must be covered by some arm. There is no fall-through
between arms.

```forge
match fault_vector {
    0x00 => divide_error(),
    0x01 => debug_exception(),
    0x02 => nmi_interrupt(),
    0x06 => invalid_opcode(),
    0x08 => double_fault(),
    0x0D => general_protection_fault(),
    0x0E => page_fault(),
    _    => unhandled_exception(fault_vector),
}
```

Note: match arm literal values are inferred from the scrutinee's type.
Explicit suffixes are not required in match arms.

**Match as expression:**

```forge
[]uint8 name = match code {
    0 => "OK",
    1 => "Not Found",
    _ => "Unknown",
};
```

**Guards:**

```forge
match value {
    n if n < 0  => negative(n),
    n if n == 0 => zero(),
    n           => positive(n),
}
```

**Range arms:**

```forge
match port {
    0x20..0x21 => pic(port),
    0x40..0x43 => pit(port),
    _          => unknown(port),
}
```

**Multiple values per arm:**

```forge
match code {
    1 | 2 | 3 => retry(),
    _         => abort(),
}
```

### 7.6 Defer

`defer` schedules a statement to execute when the current scope exits,
regardless of how it exits. Multiple `defer` statements in the same
scope execute in reverse order (LIFO).

`defer` in a pure function may only call pure functions. `defer` in
a procedure may call anything.

```forge
Fd fd = open_file("/kernel.fg");
defer close_file(fd);   // runs when this scope exits
```

`defer_err` executes only when the scope exits via an error path.
It follows the same purity rules as `defer`.

```forge
Fd fd = create_file("/output");
defer_err delete_file("/output");   // only on error
defer close_file(fd);               // always
```

---

## 8. Pointers and Memory

### 8.1 Address-Of and Dereference

```forge
@mut uint32 x = 42;
uint32*      p = &x;
uint32       v = *p;        // read
*p = 100uint32;             // write
```

Taking the address of an immutable variable produces `@const T*`.
Writing through `@const T*` is a compile error.

### 8.2 Pointer Arithmetic

```forge
uint32* base = get_base();
uint32* next   = base + 1usize;              // advances 4 bytes
uint8*  offset = (base as uint8*) + 3usize;  // byte arithmetic
```

### 8.3 Volatile Pointers

`@volatile` on the pointee prevents the compiler from reordering,
merging, or eliminating the access. Required for memory-mapped I/O.

```forge
// VGA text buffer at physical address 0xB8000
@volatile uint16* vga = 0xB8000uptr as @volatile uint16*;
*vga = 0x0F41uint16;   // white 'A' on black — write guaranteed

// APIC EOI register
constexpr APIC_EOI: uptr = 0xFEE0_00B0uptr;
@volatile uint32* eoi = APIC_EOI as @volatile uint32*;
*eoi = 0uint32;
```

### 8.4 Restrict Pointers

`@restrict` asserts that the pointer does not alias any other pointer
in scope. It is an opt-in programmer assertion — the compiler uses it
for better code generation but does not verify it.

```forge
@mut memcpy(@mut @restrict uint8* dst,
            @const @restrict uint8* src,
            usize len) {
    for i in 0..len { dst[i] = src[i]; }
}
```

### 8.5 Slices

```forge
[uint8; 512] buf;
[]uint8 s  = buf[0..];
[]uint8 s2 = buf[64usize..128usize];

uint8  first  = s[0usize];
uint8  last   = s[s.len - 1usize];
usize  length = s.len;
uint8* raw    = s.ptr;
```

### 8.6 No Global Allocator

Forge has no global allocator. All allocation requires an explicit
allocator argument passed to the function that needs it.

```forge
uint8* page = page_allocator.alloc(PAGE_SIZE);
defer page_allocator.free(page);
```

### 8.7 Stack Allocation

Local arrays and structs are stack-allocated. There is no implicit heap.

```forge
[uint8; 4096] stack_page;
GDTEntry      entries[8];
```

---

## 9. Structs

### 9.1 Declaration

Fields use `type name` order, consistent with variable declarations.
Fields are comma-separated. A trailing comma is permitted.

```forge
struct Point {
    int32 x,
    int32 y,
}

struct GDTDescriptor {
    uint16 size,
    uint64 offset,
}
```

### 9.2 Instantiation

All fields must be provided at instantiation, or the struct must be
explicitly zero-initialized. Fields may be given in any order.

```forge
Point p  = Point { x: 10, y: 20 };
Point q  = Point { y: 5, x: -3 };   // order irrelevant
Point z  = Point {};                  // zero-initialized
```

**Struct update syntax.** Copy all fields from an existing value
except those explicitly overridden:

```forge
Point p2 = Point { y: 99, ..p };    // x copied from p
```

### 9.3 Field Access

```forge
int32 px = p.x;
p.y = 20;          // COMPILE ERROR if p is immutable
```

### 9.4 Methods

Methods are defined in an `impl` block. The receiver is the first
parameter, using the same `type name` convention.

```forge
struct PhysAddr {
    uptr value,
}

impl PhysAddr {
    PhysAddr new(uptr addr) {
        return PhysAddr { value: addr };
    }

    bool page_aligned(PhysAddr self) {
        return self.value & (PAGE_SIZE - 1uptr) == 0uptr;
    }

    @mut set(@mut *PhysAddr self, uptr addr) {
        self.value = addr;
    }
}

PhysAddr pa = PhysAddr.new(0x1000uptr);
bool     ok = pa.page_aligned();
```

A pure method takes `T self` (by copy) or `*T self` (by pointer,
read-only). A method that mutates the receiver must be `@mut` and
take `@mut *T self`.

### 9.5 Layout Attributes

Struct layout is controlled through attributes. By default the compiler
may insert padding between fields to satisfy alignment requirements. The
following attributes override this behaviour:

**`@packed`** — removes all compiler-inserted padding. Every field is
placed immediately after the previous one with no gap. The resulting
struct may have an alignment of 1 byte. Use this for hardware register
layouts and network/protocol headers where every byte position is
specified by an external document.

**`@align(N)`** — forces the struct's minimum alignment to N bytes.
N must be a power of 2. Individual fields still use their natural
alignment within the struct unless `@packed` is also present.

**`@repr("C")`** — instructs the compiler to lay out the struct using
the same rules as a C compiler targeting the same platform. Field order,
padding, and alignment will match what a conforming C compiler would
produce for an equivalent C struct. Use this for structs that must be
passed to or received from C code, system calls, or hardware interfaces
that document their layout in C terms.

Attributes are commutative and may be combined:

```forge
// Hardware register — no padding, every bit is specified
@packed
struct GDTEntry {
    uint16 limit_low,
    uint16 base_low,
    uint8  base_mid,
    uint8  access,
    uint8  granularity,
    uint8  base_high,
}

// Cache-line aligned — prevents false sharing
@align(64)
struct CacheLine {
    [uint8; 64] data,
}

// Passed to C code — must match C layout
@repr("C")
struct CCompatible {
    int32 x,
    int32 y,
}

// Packed and C-compatible — e.g. a protocol header
@packed @repr("C")
struct EthernetHeader {
    [uint8; 6] dst_mac,
    [uint8; 6] src_mac,
    uint16     ethertype,
}
```

Compile-time intrinsics are available to inspect layout:

```forge
usize sz  = @sizeof(GDTEntry);           // size in bytes
usize al  = @alignof(GDTEntry);          // alignment in bytes
usize off = @offsetof(GDTEntry, access); // byte offset of field
```

---

## 10. Unions

Unions allocate a single memory region shared by all fields. The size
equals the size of the largest field. Reading any field is defined —
the result is the current bit pattern of the memory reinterpreted as
the requested type. The programmer is responsible for tracking which
field is currently valid.

```forge
union FloatBits {
    float32 as_float,
    uint32  as_int,
}

FloatBits fb   = FloatBits { as_float: 1.0 };
uint32    bits = fb.as_int;   // reads float bit pattern as uint32
```

There is no undefined behaviour from reading any union field. Unions
are used for explicit type-punning and hardware register access.

---

## 11. Enums

### 11.1 Simple Enums

A simple enum is an integer type with named values. The backing type
must be specified and must be an integer type.

```forge
enum Color: uint8 {
    Red   = 0,
    Green = 1,
    Blue  = 2,
}

Color  c   = Color.Red;
uint8  raw = c as uint8;
```

The backing type determines the size and alignment. All discriminant
values must fit. The compiler enforces this.

### 11.2 Algebraic Enums (Tagged Unions)

Each variant may carry different data. The compiler generates the
discriminant field automatically. The size is the largest variant plus
the discriminant.

```forge
enum MemRegion {
    Free     { uptr base, usize size },
    Kernel   { uptr base, usize size, uint32 flags },
    MMIO     { uptr base, usize size, uint32 device_id },
    Reserved,
}
```

Algebraic enums must be matched exhaustively. Accessing variant fields
without first matching on the enum is a compile error.

```forge
@mut describe(MemRegion r) {
    match r {
        Free     { base, size }      => log("free {} at {:x}", size, base),
        Kernel   { base, flags, .. } => log("kernel at {:x}", base),
        MMIO     { device_id, .. }   => log("mmio device {}", device_id),
        Reserved                     => log("reserved"),
    }
}
```

The discriminant backing type may be controlled with `@repr`:

```forge
@repr("uint8")
enum Status {
    Ok    = 0,
    Error { uint32 code },
    Pending,
}
```

---

## 12. Modules

### 12.1 Module Declaration

Each `.fg` file declares its module name at the top. The name maps
to the file path relative to the source root.

```forge
module kernel.memory.paging;
```

There are no header files and no forward declarations. Each name is
declared exactly once.

### 12.2 Visibility

All declarations are private by default. `@pub` makes a declaration
visible to importers.

```forge
module kernel.memory.frame;

// Private — only visible within this file
uint32 scan_bitmap(usize start) { ... }

// Public — visible to importers
@pub @mut alloc() { ... }
@pub @mut free(uptr phys_addr) { ... }
@pub struct AllocError { uint8 code, }
```

### 12.3 Importing

Every imported name must be listed explicitly. There is no wildcard
import. This keeps dependencies visible and prevents name collision.

```forge
// Import a single function
import kernel.memory.paging.map_page();

// Import multiple names
import kernel.memory.paging.{map_page(), unmap_page()};

// Import a type (no parentheses — types are not callable)
import kernel.memory.frame.AllocError;

// Access with full qualified name after module import
import kernel.memory.paging;
kernel.memory.paging.map_page(virt, phys, flags);
```

Function imports use parentheses to distinguish them from type imports.
Type imports (structs, enums, unions) use the bare name without
parentheses.

### 12.4 Module Resolution

`kernel.memory.paging` resolves to `kernel/memory/paging.fg` relative
to the source root. `forge build` compiles all transitively imported
modules. Each module is compiled exactly once regardless of how many
others import it.

---

## 13. Attributes

Attributes modify the declaration that immediately follows them.
They use `@name` or `@name(args)` syntax. Attributes always appear
to the **left** of the declaration they modify. Multiple attributes
on one declaration are **commutative** — order never has semantic
meaning.

```forge
@pub @inline uint32 fast(uint32 x) { return x; }
@inline @pub uint32 fast(uint32 x) { return x; }   // identical
```

See §21 for the complete attribute reference table.

---

## 14. Inline Assembly

Inline assembly is a first-class language feature.

### 14.1 Single-Instruction Form

```forge
@mut enable_interrupts()  { asm("sti"); }
@mut disable_interrupts() { asm("cli"); }

@noreturn @mut halt() {
    loop { asm("hlt"); }
}
```

### 14.2 Full Form with Constraints

```
asm( "template" : outputs : inputs : clobbers )
```

```forge
uint64 read_cr0() {
    @uninit uint64 val;
    asm("mov %cr0, %rax" : "=a"(val) : : );
    return val;
}

@mut write_cr3(uptr pml4) {
    asm("mov %rax, %cr3" : : "a"(pml4) : "memory");
}

@mut out_byte(uint16 port, uint8 val) {
    asm("outb %al, %dx" : : "d"(port), "a"(val) : );
}

uint8 in_byte(uint16 port) {
    @uninit uint8 val;
    asm("inb %dx, %al" : "=a"(val) : "d"(port) : );
    return val;
}
```

### 14.3 Multi-Line Block Form

```forge
@mut load_gdt(@const *GDTDescriptor desc) {
    asm {
        lgdt [rdi]
        mov  ax, 0x10
        mov  ds, ax
        mov  es, ax
        mov  fs, ax
        mov  gs, ax
        mov  ss, ax
        push 0x08
        lea  rax, [rip + .reload_cs]
        push rax
        retfq
    .reload_cs:
    }
}
```

### 14.4 Volatile Assembly

`@volatile` prevents the compiler from reordering or eliminating an
`asm` block. Required for memory barriers and any asm with observable
side effects not captured by clobbers.

```forge
@mut memory_barrier() {
    @volatile asm("mfence" : : : "memory");
}
```

`@volatile asm` is always a side effect and may only appear in
procedures (`@mut` context). Non-volatile asm in a pure function is
permitted only if it has no outputs and no side effects on memory.

### 14.5 Constraint Reference

| Constraint | Register |
|-----------|---------|
| `"a"` | `rax` / `eax` / `ax` / `al` |
| `"b"` | `rbx` / `ebx` |
| `"c"` | `rcx` / `ecx` |
| `"d"` | `rdx` / `edx` |
| `"D"` | `rdi` |
| `"S"` | `rsi` |
| `"r"` | any general-purpose register |
| `"m"` | memory operand |
| `"i"` | immediate integer |
| `"="` | output operand (write-only) |
| `"+"` | output operand (read-write) |

Clobbers: `"memory"` — asm may read/write arbitrary memory.
`"cc"` — asm modifies the flags register.

---

## 15. Comptime

`comptime` evaluation runs entirely at compile time. It is typed,
scoped, and debuggable — not a text preprocessor.

### 15.1 Constexpr Blocks

```forge
comptime {
    constexpr PAGE_SHIFT: uint32 = 12uint32;
    constexpr PAGE_SIZE:  usize  = 1usize << PAGE_SHIFT;
    constexpr PAGE_MASK:  uptr   = ~(PAGE_SIZE as uptr - 1uptr);

    // Compile-time assertions — failure is a compile error
    assert(@sizeof(GDTEntry) == 8usize, "GDT entry must be 8 bytes");
    assert(PAGE_SIZE == 4096usize,      "PAGE_SIZE must be 4096");
}
```

### 15.2 Comptime If — Conditional Compilation

`comptime if` replaces `#ifdef` entirely. Typed, scoped, no text
substitution.

```forge
comptime if (TARGET_ARCH == .x86_64) {

    constexpr PHYS_ADDR_BITS: uint32 = 52uint32;

    @mut flush_tlb(uptr vaddr) {
        asm("invlpg [rdi]" : : "D"(vaddr) : );
    }

} else {
    @error("Forge targets x86-64 only");
}
```

### 15.3 Comptime Functions

A function declared `comptime` is evaluated at compile time when called
with compile-time-known arguments.

```forge
comptime usize kib(usize n) { return n * 1024usize; }
comptime usize mib(usize n) { return n * 1024usize * 1024usize; }

constexpr HEAP_SIZE:  usize = mib(16usize);
constexpr STACK_SIZE: usize = kib(64usize);
```

### 15.4 Built-In Comptime Intrinsics

| Intrinsic | Result | Description |
|-----------|--------|-------------|
| `@sizeof(T)` | `usize` | Size of type T in bytes |
| `@alignof(T)` | `usize` | Alignment of type T in bytes |
| `@offsetof(T, field)` | `usize` | Byte offset of field in struct T |
| `@bitcast<T>(val)` | `T` | Reinterpret bit pattern of val as T |
| `@error("msg")` | — | Emit a compile error |
| `@warning("msg")` | — | Emit a compile warning |

---

## 16. OS and Hardware Primitives

These features are always available without importing anything.

### 16.1 Port I/O

`port<T>` is the type of an x86 I/O port. `T` must be `uint8`,
`uint16`, or `uint32`. Port accesses translate directly to `in`/`out`
instructions.

Port writes use the `<-` operator. Port reads use the `.read()` method.
Both are side effects and may only be used in procedures (`@mut` context).

```forge
port<uint8> pic_master_cmd  = 0x20uint16;
port<uint8> pic_master_data = 0x21uint16;
port<uint8> pic_slave_cmd   = 0xA0uint16;
port<uint8> pic_slave_data  = 0xA1uint16;

// Write to port (outb) — procedure only
@mut send_eoi() {
    pic_master_cmd <- 0x20uint8;
}

// Read from port (inb) — procedure only
@mut uint8 read_pic_status() {
    return pic_master_cmd.read();
}
```

### 16.2 Interrupt Service Routines

`@interrupt` generates a complete ISR prologue and epilogue: saves all
caller-saved registers, calls the handler body, restores registers,
and executes `iretq`. ISR handlers are always procedures.

```forge
@interrupt @mut isr_divide_error(@const *InterruptFrame frame) {
    panic("Divide by zero at rip={:x}", frame.rip);
}

// Vectors that push an error code — second parameter receives it
@interrupt @mut isr_page_fault(@const *InterruptFrame frame,
                                uint64 error_code) {
    uint64 cr2 = @cpu.read_cr2();
    handle_page_fault(cr2, error_code, frame);
}

// @naked — no generated prologue or epilogue
@naked @noreturn @mut isr_double_fault() {
    asm {
        call double_fault_handler
    .hang:
        hlt
        jmp .hang
    }
}
```

The `InterruptFrame` struct matches the layout pushed by the CPU on
interrupt entry:

```forge
@repr("C")
struct InterruptFrame {
    uint64 rip,
    uint64 cs,
    uint64 rflags,
    uint64 rsp,
    uint64 ss,
}
```

### 16.3 Linker Symbol Integration

Symbols defined in a linker script are declared with `extern`. Taking
their address gives a pointer to their location in the binary image.

```forge
extern uint8 __kernel_start;
extern uint8 __kernel_end;
extern uint8 __bss_start;
extern uint8 __bss_end;
extern uint8 __stack_top;

@mut clear_bss() {
    uptr   start = &__bss_start as uptr;
    uptr   end   = &__bss_end   as uptr;
    usize  len   = (end - start) as usize;
    @mut uint8* p = start as @mut uint8*;
    for i in 0..len { p[i] = 0; }
}
```

### 16.4 CPU Register Access

Built-in `@cpu` intrinsics provide access to control registers and
MSRs. Reads are pure. Writes are side effects and require `@mut` context.

```forge
// Pure reads
uint64 cr0 = @cpu.read_cr0();
uint64 cr2 = @cpu.read_cr2();   // page fault linear address
uint64 cr3 = @cpu.read_cr3();   // PML4 physical base
uint64 cr4 = @cpu.read_cr4();
uint64 efer = @cpu.rdmsr(0xC000_0080uint32);

CpuidResult info     = @cpu.cpuid(1uint32, 0uint32);
bool        has_sse2 = (info.edx >> 26uint32) & 1uint32 == 1uint32;
bool        ie       = @cpu.interrupts_enabled();

// Writes — @mut context required
@mut init_paging() {
    @cpu.write_cr3(pml4_phys);
    @cpu.write_cr4(@cpu.read_cr4() | (1uint64 << 5uint64));
}

@mut enable_long_mode() {
    @cpu.wrmsr(0xC000_0080uint32,
               @cpu.rdmsr(0xC000_0080uint32) | (1uint64 << 8uint64));
}

@mut enable_interrupts()  { @cpu.enable_interrupts(); }
@mut disable_interrupts() { @cpu.disable_interrupts(); }
```

`CpuidResult` is a built-in struct:

```forge
struct CpuidResult {
    uint32 eax,
    uint32 ebx,
    uint32 ecx,
    uint32 edx,
}
```

### 16.5 Atomic Operations

`atomic<T>` provides indivisible operations with explicit memory
ordering. There is no implicit synchronization anywhere else in Forge.

Atomic loads are pure. Atomic stores and read-modify-write operations
are side effects and may only appear in `@mut` context.

```forge
atomic<uint64> counter;   // zero-initialized

// Pure read
uint64 read_counter() {
    return counter.load(.Acquire);
}

// Writes — procedure
@mut tick() {
    counter.fetch_add(1uint64, .SeqCst);
}

@mut reset() {
    counter.store(0uint64, .Release);
}

@mut bool try_acquire() {
    return counter.compare_exchange(0uint64, 1uint64, .SeqCst, .Relaxed);
}
```

**Memory orderings:**

| Ordering | Meaning |
|---------|---------|
| `.Relaxed` | Atomic only — no ordering |
| `.Acquire` | This load sees all stores before matching Release |
| `.Release` | All prior stores visible to matching Acquire |
| `.AcqRel` | Both Acquire and Release |
| `.SeqCst` | Total sequential consistency |

---

## 17. Memory Model

### 17.1 Execution Model

Forge targets a single-threaded execution model at the language level.
There is no thread primitive. Concurrency is expressed through atomic
operations (§16.5), memory-mapped registers, and interrupt handlers.

### 17.2 Address Space

The compiler generates code for a flat 64-bit virtual address space.
Physical address management is the programmer's responsibility. `uptr`
is the type for raw addresses. `@volatile` handles MMIO.

### 17.3 Stack

The stack grows downward. Frames follow the System V AMD64 ABI (§19).
`@naked` functions have no generated frame.

### 17.4 Object Lifetime

A local variable's lifetime begins at its declaration and ends at the
end of the enclosing block. Using a pointer to a local variable after
its lifetime ends is a compile error where detectable, and produces
incorrect behaviour otherwise. The compiler performs conservative
escape analysis and emits an error when it can prove a violation.

---

## 18. Undefined Behaviour

**Forge has no undefined behaviour.**

Every operation has a defined result. The following are UB in C and
defined in Forge:

| Operation | C | Forge |
|-----------|---|-------|
| Signed integer overflow | Undefined | Wraps (two's complement) |
| Unsigned integer overflow | Wraps | Wraps |
| Division by zero | Undefined | Traps |
| Null pointer dereference | Undefined | Cannot occur — no null values |
| Out-of-bounds array access | Undefined | Traps |
| Uninitialized read | Undefined | Compile error |
| Shift by >= type width | Undefined | Result is 0 |
| Left shift of negative | Undefined | Defined by two's complement |
| Pointer aliasing violation | Undefined | No strict aliasing rule |
| Reading wrong union field | Undefined | Returns current bit pattern |

---

## 19. ABI and Calling Conventions

### 19.1 Default — System V AMD64

**Integer/pointer arguments:** `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.
Additional arguments on the stack right-to-left.

**Float arguments:** `xmm0`–`xmm7`.

**Return values:**
- ≤ 64-bit integer/pointer: `rax`
- 65–128-bit integer: `rax:rdx`
- Float: `xmm0`

**Caller-saved:** `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`–`r11`,
`xmm0`–`xmm15`

**Callee-saved:** `rbx`, `rbp`, `r12`–`r15`

**Stack alignment:** 16-byte aligned before `call`.

### 19.2 Struct Passing

Structs ≤ 16 bytes are passed in registers per the AMD64 classification
algorithm. Structs > 16 bytes are passed on the stack by value (the
caller copies the struct and passes a pointer transparently).

### 19.3 Interrupt Frame Layout

On hardware interrupt entry the CPU pushes onto the kernel stack:

```
[rsp+32]  ss
[rsp+24]  rsp (user)
[rsp+16]  rflags
[rsp+ 8]  cs
[rsp+ 0]  rip
```

For error-code vectors (e.g. #PF, #GP, #DF):

```
[rsp+40]  ss
[rsp+32]  rsp (user)
[rsp+24]  rflags
[rsp+16]  cs
[rsp+ 8]  rip
[rsp+ 0]  error_code
```

---

## 20. Object Files and Linking

### 20.1 Output Format

`forgec` produces ELF64 object files. These are linked by `ld` or
`lld`. The compiler does not include a linker.

### 20.2 Sections

| Section | Contents |
|---------|----------|
| `.text` | Compiled function and procedure bodies |
| `.rodata` | String literals, read-only data |
| `.data` | Initialized mutable globals |
| `.bss` | Zero-initialized globals (no bytes in file) |
| `.note.forge` | Compiler version and target information |

Additional sections are created by `@section("name")`.

### 20.3 Symbol Visibility

Private symbols are local to the object file. `@pub` symbols are
exported with global visibility. `@export("name")` sets the exact
symbol name, bypassing name mangling — use for stable entry points.

### 20.4 Name Mangling

Forge mangles symbol names to encode module path and parameter types.
The mangling scheme is not stable across compiler versions. Use
`@export("name")` for any symbol that must be stable.

---

## 21. Compiler Attributes Reference

### 21.1 Variable Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Variable is mutable after initialization |
| `@uninit` | Not zero-initialized; compiler tracks reads |
| `@const` | Pointee is immutable |
| `@volatile` | Pointee accesses never reordered or eliminated |
| `@restrict` | No aliasing with other pointers in scope |
| `@section("name")` | Place in named ELF section |
| `@align(N)` | Minimum alignment; N must be power of 2 |

### 21.2 Function and Procedure Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Declares a procedure — side effects permitted |
| `@pub` | Visible to other modules |
| `@noreturn` | Function or procedure never returns |
| `@inline` | Always inline at call site |
| `@noinline` | Never inline |
| `@naked` | No generated prologue, epilogue, or stack frame |
| `@cc("name")` | Set calling convention |
| `@interrupt` | Generate ISR-safe prologue/epilogue + `iretq` |
| `@cold` | Rarely executed — optimizer deprioritizes |
| `@section("name")` | Place in named ELF section |
| `@export("name")` | Export with exact symbol name |
| `@deprecated("msg")` | Warn at every call site |

### 21.3 Type Attributes

| Attribute | Applies To | Effect |
|-----------|-----------|--------|
| `@pub` | struct / enum / union | Visible to importers |
| `@packed` | struct | Remove all padding |
| `@align(N)` | struct | Force minimum alignment |
| `@repr("C")` | struct / enum | C-compatible layout |
| `@repr("uintN")` | enum | Set discriminant backing type |

---

## 22. What Forge Deliberately Omits

| Omitted Feature | Reason |
|----------------|--------|
| **Garbage collector** | Unpredictable latency; incompatible with interrupt handlers |
| **Exceptions** | Hidden control flow — violates §1.2 |
| **Type inference on expressions** | You always know the type. Inference hides it. |
| **Implicit return** | Every function exit must be an explicit `return`. |
| **`fn` keyword** | The return type is sufficient. C got this right. |
| **`void` keyword** | Procedures are declared with `@mut` and no return type. |
| **`never` type / trailing `->`** | Replaced by `@noreturn` attribute — consistent with the attribute system. |
| **`pub` keyword** | Visibility is an attribute. `@pub` is consistent. |
| **`const` keyword** | Replaced by `constexpr` — unambiguously compile-time. |
| **Wildcard imports** | Every imported name must be listed. Dependencies stay visible. |
| **Implicit integer promotion** | Source of a significant fraction of C CVEs. |
| **Null pointers** | NULL does not exist. Nullable expressed in the type system. |
| **Undefined behaviour** | See §18. Every operation is defined. |
| **Text preprocessor** | `comptime if` replaces `#ifdef`. Modules replace `#include`. |
| **Header files** | Modules with `@pub` replace this entirely. |
| **Global allocator** | The kernel defines its memory model. Forge does not impose one. |
| **Hidden copies** | Large types must be passed by pointer explicitly. |
| **Implicit conversions** | Every type change requires explicit `as`. |
| **Variadic functions** | Use slices instead. |
| **`goto`** | Use `loop { break; }` or labeled breaks. |
| **Operator overloading** | `+` always means addition. |
| **Dynamic dispatch / vtables** | No virtual dispatch of any kind. |
| **RTTI** | No runtime type information. |
| **Built-in `panic` or `memset`** | These are standard library functions, not language primitives. |

---

## 23. Appendix A — Kernel Entry Point Example

```forge
module kernel.boot;

import kernel.memory.paging.{init_paging()};
import kernel.memory.frame.{init_frame_allocator()};
import kernel.cpu.gdt.{init_gdt()};
import kernel.cpu.idt.{init_idt()};
import kernel.drivers.uart.{init_uart(), uart_write()};
import forge.panic.{panic()};
import forge.mem.{memset()};

extern uint8 __bss_start;
extern uint8 __bss_end;
extern uint8 __stack_top;

/// Kernel entry point — called by the Multiboot2 bootloader.
/// multiboot_magic must equal 0x36D76289.
/// multiboot_info is the physical address of the Multiboot2 info structure.
@pub @section(".boot.text") @export("kernel_main")
@mut kernel_main(uint32 multiboot_magic, uptr multiboot_info) {
    // Clear BSS before anything else touches memory
    clear_bss();

    // Initialize CPU structures before enabling interrupts
    init_gdt();
    init_idt();

    // Early serial output — available before framebuffer
    init_uart(0x3F8uint16, 115200uint32);
    uart_write("Forge kernel starting\n");

    // Validate bootloader magic
    if multiboot_magic != 0x36D7_6289uint32 {
        panic("Invalid Multiboot2 magic");
    }

    // Initialize memory subsystem
    init_frame_allocator(multiboot_info);
    init_paging();

    uart_write("Memory OK\n");

    idle_loop();
}

@mut clear_bss() {
    uptr  start = &__bss_start as uptr;
    uptr  end   = &__bss_end   as uptr;
    usize len   = (end - start) as usize;
    @mut uint8* p = start as @mut uint8*;
    for i in 0..len { p[i] = 0; }
}

@noreturn @mut idle_loop() {
    loop {
        @cpu.disable_interrupts();
        asm("hlt");
        @cpu.enable_interrupts();
    }
}
```

---

## 24. Appendix B — Physical Frame Allocator Example

```forge
module kernel.memory.frame;

import forge.panic.{panic()};

/// One bit per 4 KiB physical frame. Supports up to 4 GiB of RAM.
constexpr MAX_FRAMES: usize = 1_048_576usize;

@uninit [uint8; MAX_FRAMES / 8usize] bitmap;
@mut usize total_frames = 0usize;
@mut usize free_frames  = 0usize;

/// Initialize from a Multiboot2 memory map.
/// Marks all frames used initially, then marks usable regions free.
@pub @mut init(uptr multiboot_info) {
    for i in 0..bitmap.len {
        bitmap[i] = 0xFF;
    }

    @const *MemoryMap map = multiboot2_memory_map(multiboot_info);

    for entry in map {
        if entry.kind == 1uint32 {   // 1 = usable conventional RAM
            usize base  = (entry.base / PAGE_SIZE as uint64) as usize;
            usize count = (entry.size / PAGE_SIZE as uint64) as usize;
            for frame in base..base + count {
                set_free(frame);
                free_frames += 1usize;
            }
            total_frames += count;
        }
    }
}

/// Allocate one physical frame. Returns its physical base address.
/// Panics if no frames are available.
@pub @mut uptr alloc() {
    for i in 0..bitmap.len {
        if bitmap[i] != 0xFFuint8 {
            for bit in 0..8uint8 {
                if (bitmap[i] >> bit) & 1uint8 == 0uint8 {
                    bitmap[i] |= 1uint8 << bit;
                    free_frames -= 1usize;
                    usize frame = i * 8usize + bit as usize;
                    return frame as uptr * PAGE_SIZE as uptr;
                }
            }
        }
    }
    panic("out of physical memory");
}

/// Free a physical frame by its base address.
@pub @mut free(uptr phys_addr) {
    usize frame = phys_addr as usize / PAGE_SIZE;
    usize byte  = frame / 8usize;
    uint8 bit   = (frame % 8usize) as uint8;
    bitmap[byte] &= ~(1uint8 << bit);
    free_frames += 1usize;
}

/// Number of free frames remaining.
@pub usize available() {
    return free_frames;
}

/// Total usable frames found during init.
@pub usize total() {
    return total_frames;
}

@mut set_free(usize frame) {
    usize byte = frame / 8usize;
    uint8 bit  = (frame % 8usize) as uint8;
    bitmap[byte] &= ~(1uint8 << bit);
}
```

---

## 25. Appendix C — Compile-Time Guarantees

A conforming `forgec` implementation guarantees:

**Type safety.** No program that compiles without errors can read a
value as a different type without an explicit `as` or `@bitcast`,
access a nonexistent struct field, or call a function with wrong
argument types.

**Purity enforcement.** No pure function writes to global state or
performs I/O. The compiler verifies this statically. A procedure call
from a pure function is a compile error.

**No implicit return.** Every value-returning function must have an
explicit `return` on every reachable exit path. A missing `return` is
a compile error.

**No silent integer conversions.** Every integer type change in the
source corresponds to an explicit `as`.

**No hidden control flow.** Every exit path from every function is
visible in the source. There are no destructors with side effects and
no implicit early returns.

**Exhaustive matching.** Every `match` on an algebraic enum covers all
variants. Adding a variant is a compile error in every file that matches
on that enum without a wildcard arm.

**Defined overflow.** Signed and unsigned integer overflow wraps in
two's complement. The compiler never uses overflow as an optimization
assumption.

**Zero initialization.** Every variable not marked `@uninit` is
zero-initialized before first read. The compiler emits this explicitly
— it does not rely on linker or loader behaviour for stack variables.

**Uninitialized read prevention.** Every `@uninit` variable is
tracked. Reading it before a provable write is a compile error.

**Noreturn verification.** Every `@noreturn` function or procedure
must not contain a reachable `return`. A reachable `return` in a
`@noreturn` declaration is a compile error.

---

*The Forge Programming Language Specification, Version 1.2*
*Copyright © 2025 — GNU General Public License v3.0*
*The Forge compiler (forgec) is licensed under GPLv3 with a runtime exception.*
*Code produced by forgec is not subject to the GPL.*
