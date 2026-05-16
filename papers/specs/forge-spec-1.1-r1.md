# The Forge Programming Language Specification

**Version:** 1.1
**Status:** Normative Draft
**License:** GNU General Public License v3.0
**Compiler:** `forgec` (bootstrap implementation in C99)
**File Extension:** `.fg`
**Target:** x86-64, freestanding (no libc, no OS assumed)

---

## Preamble

Forge is a compiled, statically typed systems programming language designed
specifically for operating system development. It targets x86-64 exclusively.
It has no runtime, no garbage collector, no standard library that assumes an
operating system exists, and no undefined behaviour.

Forge is C without the footguns. It keeps everything C got right — explicit
memory, direct hardware access, predictable code generation, a simple mental
model — and surgically removes everything C got wrong through decades of
accumulated technical debt: undefined behaviour, implicit conversions,
uninitialized memory, the text preprocessor, missing namespaces, and no
error handling model.

The `forgec` compiler is written in C99. It has no external dependencies. It
produces ELF64 object files for x86-64 and links via the system linker (`ld`).
The entire compiler, including all optimization passes, targets approximately
20,000 lines of source code. This is a design constraint, not an accident.
A compiler you can read in a weekend is a compiler you can trust.

---

## Table of Contents

1. [Design Principles](#1-design-principles)
2. [Lexical Structure](#2-lexical-structure)
3. [Type System](#3-type-system)
4. [Variables and Constants](#4-variables-and-constants)
5. [Functions and Procedures](#5-functions-and-procedures)
6. [Control Flow](#6-control-flow)
7. [Pointers and Memory](#7-pointers-and-memory)
8. [Structs](#8-structs)
9. [Unions](#9-unions)
10. [Enums](#10-enums)
11. [Modules](#11-modules)
12. [Attributes](#12-attributes)
13. [Inline Assembly](#13-inline-assembly)
14. [Comptime](#14-comptime)
15. [OS and Hardware Primitives](#15-os-and-hardware-primitives)
16. [Memory Model](#16-memory-model)
17. [Undefined Behaviour](#17-undefined-behaviour)
18. [ABI and Calling Conventions](#18-abi-and-calling-conventions)
19. [Object Files and Linking](#19-object-files-and-linking)
20. [Compiler Attributes Reference](#20-compiler-attributes-reference)
21. [What Forge Deliberately Omits](#21-what-forge-deliberately-omits)
22. [Appendix A — Kernel Entry Point Example](#22-appendix-a--kernel-entry-point-example)
23. [Appendix B — Physical Frame Allocator Example](#23-appendix-b--physical-frame-allocator-example)
24. [Appendix C — Compile-Time Guarantees](#24-appendix-c--compile-time-guarantees)

---

## 1. Design Principles

These principles are ordered by priority. When two principles conflict, the
higher-numbered one yields to the lower-numbered one.

### 1.1 No Undefined Behaviour

Every operation in Forge has a defined result. The compiler never uses the
absence of a definition as an optimization license. There is no concept of
undefined behaviour in this specification. Every edge case is specified.

Signed integer overflow wraps by default. Division by zero traps. Out-of-bounds
access traps. Uninitialized reads are a compile error unless explicitly opted
out of with `@uninit`. Null pointer dereferences cannot occur because null
pointers do not exist as plain values.

### 1.2 Explicit Over Implicit

If an operation costs cycles, allocates memory, changes a type, copies a
value, or has a side effect — it must be written explicitly in the source.

- No type inference
- No implicit integer promotion
- No implicit conversions between any types
- No hidden copies of values
- No hidden allocations
- No hidden control flow

### 1.3 Zero-Cost Abstractions

Every abstraction in Forge compiles to machine code identical to what a
competent programmer would write by hand in C. If an abstraction cannot be
zero-cost, it is not in the language.

### 1.4 Purity by Default

All functions in Forge are **pure by default**. A pure function may not write
to global variables, perform I/O, or call impure functions. The compiler
enforces this statically.

A function that has side effects — writes to global state, performs port I/O,
calls other impure functions — must be declared with the `@mut` attribute.
These are called **procedures**. The `@mut` attribute is both the declaration
signal and the permission to have side effects.

Writing through pointer parameters is permitted in pure functions because the
effect is explicit in the function signature — the caller passed the pointer
knowing it may be written.

### 1.5 Freestanding First

Forge does not assume the existence of an operating system, a C runtime, a
heap, or a standard library. Every language feature works at interrupt level,
in a bootloader, or on bare metal with no prior initialization.

### 1.6 Predictable Code Generation

The programmer should be able to predict the assembly output of any Forge
function without running the compiler. There is no hidden virtual dispatch,
no hidden state machine generation, and no compiler transformations that
change the observable behaviour of a correct program beyond reordering and
eliminating redundant pure operations.

---

## 2. Lexical Structure

### 2.1 Source Files

Forge source files are UTF-8 encoded text with the `.fg` extension. Line
endings may be `LF` or `CRLF`. The compiler normalizes all line endings to
`LF` before processing.

### 2.2 Comments

```forge
// Single-line comment — extends to end of line

/*
   Block comment.
   Block comments nest: /* this is valid */ still in comment
*/

/// Documentation comment.
/// Attached to the immediately following declaration.
/// Content is treated as Markdown by documentation tooling.
uint32 add(uint32 a, uint32 b) { ... }
```

Comments are stripped during lexing and have no effect on the program.

### 2.3 Keywords

The following identifiers are reserved and may not be used as user-defined
names:

```
as          asm         break       comptime    continue
defer       defer_err   else        enum        extern
false       for         if          import      in
loop        match       mod         not         or
and         return      struct      true        union
constexpr   while
```

The following words are **not** keywords. They have no special meaning to the
parser and may be used as identifiers, though doing so is strongly discouraged:

```
mut     uninit     never
```

`never` is a built-in type name resolved during semantic analysis, not parsing.

### 2.4 Identifiers

```
identifier ::= [a-zA-Z_][a-zA-Z0-9_]*
```

Identifiers are case-sensitive. Identifiers beginning with two underscores
(`__`) are reserved for the linker and platform ABI.

### 2.5 Integer Literals

All integer literals require an explicit type suffix. A literal without a
suffix is a compile error. Underscores may appear anywhere within the digit
sequence for readability and are ignored by the compiler.

```forge
uint32  a = 1_000_000uint32;
uint32  b = 0xFF_EC_00_01uint32;   // hexadecimal
uint8   c = 0b1010_1100uint8;      // binary
uint16  d = 0o755uint16;           // octal
usize   e = 4096usize;
uptr    f = 0xFFFF_FFFF_8000_0000uptr;
int32   g = -1int32;
```

Valid suffixes:

```
uint8   uint16   uint32   uint64   uint128
int8    int16    int32    int64    int128
usize   isize    uptr
```

### 2.6 Float Literals

Float literals require an explicit suffix and must contain a decimal point.

```forge
float32 a = 3.14float32;
float64 b = 2.718_281_828float64;
float32 c = 0.5float32;
```

Valid suffixes: `float32` `float64`

### 2.7 Boolean Literals

`true` and `false` are keywords and the only values of type `bool`. `bool`
is not an integer and cannot be implicitly converted to one.

### 2.8 Character Literals

A character literal produces a value of type `uint32` containing the Unicode
codepoint of the character.

```forge
uint32 a = 'A';            // 65
uint32 b = '\n';           // 10
uint32 c = '\t';           // 9
uint32 d = '\\';           // 92
uint32 e = '\u{1F600}';    // Unicode codepoint
uint32 f = '\x41';         // hex byte — 65
```

### 2.9 String Literals

A string literal produces a value of type `[]uint8`. The string is UTF-8
encoded with no null terminator. Length is available via `.len`.

```forge
[]uint8 a = "hello, kernel";
[]uint8 b = "line one\nline two";
[]uint8 c = r"raw string — no \n processing";
```

String literals are stored in `.rodata` and are immutable. Writing through
a string literal pointer is a compile error.

### 2.10 Operators

```
Arithmetic:    +   -   *   /   %
Bitwise:       &   |   ^   ~   <<   >>
Comparison:    ==  !=  <   >   <=   >=
Logical:       and   or   not
Assignment:    =   +=  -=  *=  /=  &=  |=  ^=  <<=  >>=
Range:         ..   ..=
Pointer:       *  (dereference)    &  (address-of)
Cast:          as
Port I/O:      <-  (port write)    port.read()  (port read)
Member:        .
Index:         [ ]
```

Operator precedence follows C conventions with these differences:
- `not` binds tighter than `and` and `or`
- `and` and `or` are keywords, not symbols
- There is no comma operator

---

## 3. Type System

Forge is statically typed. Every expression has a type known at compile time.
There is no type inference — all declarations require an explicit type. There
are no implicit conversions between any types. Every type change must be
written explicitly with `as`.

### 3.1 Integer Types

| Type | Width | Signedness |
|------|-------|------------|
| `uint8` | 8 bits | unsigned |
| `uint16` | 16 bits | unsigned |
| `uint32` | 32 bits | unsigned |
| `uint64` | 64 bits | unsigned |
| `uint128` | 128 bits | unsigned |
| `int8` | 8 bits | signed |
| `int16` | 16 bits | signed |
| `int32` | 32 bits | signed |
| `int64` | 64 bits | signed |
| `int128` | 128 bits | signed |
| `usize` | 64 bits (x86-64) | unsigned — for lengths and indices |
| `isize` | 64 bits (x86-64) | signed |
| `uptr` | 64 bits (x86-64) | unsigned — for raw hardware addresses |

`usize` is for array lengths, loop indices, and memory sizes.
`uptr` is for raw hardware addresses and address arithmetic.
They are distinct types. There is no implicit conversion between them.

**Integer overflow** is defined behaviour in Forge. Both signed and unsigned
integers wrap on overflow by default (two's complement). Explicit behaviour
can be requested at the call site:

```forge
uint32 a = x.wrapping_add(y);    // defined wrap
uint32 b = x.saturating_add(y);  // clamps to type maximum
```

### 3.2 Float Types

| Type | Width | Standard |
|------|-------|----------|
| `float32` | 32 bits | IEEE 754 single precision |
| `float64` | 64 bits | IEEE 754 double precision |

Float operations follow IEEE 754 strictly. The compiler does not reorder or
fuse float operations in ways that change the result under IEEE 754 semantics.

### 3.3 Boolean Type

`bool` is a 1-byte type. Values are `true` and `false`. It is not an integer.
There is no implicit conversion between `bool` and any integer type.

### 3.4 Void and Never

There is no `void` keyword for function declarations. A function that has no
return value is declared without a return type — the compiler infers from
context that it returns nothing. These are called **procedures** and require
the `@mut` attribute (see §5).

`never` is the return type of functions that do not return. It is written
explicitly in the return position.

```forge
// Procedure — no return type, @mut required
@mut setup_gdt() {
    ...
}

// Diverging pure function — explicitly annotated never
halt() -> never {
    loop { asm("hlt"); }
}
```

### 3.5 Pointer Types

```forge
uint32*           // raw pointer to mutable uint32
@const uint32*    // raw pointer to immutable uint32
@volatile uint32* // MMIO pointer — accesses never optimized away
```

**Pointers in Forge are always non-null.** A nullable pointer is expressed
as `Option<*T>` (to be specified in a future version). `NULL` does not exist
as a value in Forge.

Pointer arithmetic is performed in units of `sizeof(T)`. For byte-level
arithmetic, cast to `uint8*` first.

### 3.6 Slice Types

A slice is a fat pointer: `(ptr: *T, len: usize)`. All slice accesses are
bounds-checked — at compile time where possible, trapping at runtime
otherwise.

```forge
[]uint8         // mutable slice of uint8
[]@const uint8  // immutable slice of uint8
```

Slice fields:
- `.ptr` — raw pointer to the first element
- `.len` — number of elements

### 3.7 Array Types

Arrays are fixed-size and stack-allocated. The size must be a `constexpr`
value known at compile time.

```forge
[uint8; 512]      buf;
[uint32; 1024]    table;
[GDTEntry; 8]     gdt;
```

### 3.8 Tuple Types

A tuple is an anonymous struct with positional fields.

```forge
(uint32, bool, uint8) t  = (42uint32, true, 0xFFuint8);
uint32                lo = t.0;
bool                  b  = t.1;
uint8                 hi = t.2;
```

### 3.9 Function Pointer Types

```forge
(uint32, uint32) -> uint32   // pointer to pure function
@mut () -> never              // pointer to diverging procedure
```

### 3.10 Bitfield Integers

Inside `@packed` or `@repr("C")` structs, integer types of arbitrary bit
width between 1 and 64 may be used for hardware register layouts. The syntax
is `uintN` or `intN` where N is the bit width.

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

### 3.11 Atomic Types

`atomic<T>` wraps an integer or pointer type and provides indivisible
read-modify-write operations with explicit memory ordering. See §15.5.

### 3.12 Port Type

`port<T>` represents an x86 I/O port where `T` is `uint8`, `uint16`, or
`uint32`. See §15.1.

### 3.13 Type Casting

All type conversions are explicit and use `as`. Every cast has a defined
result — there is no undefined behaviour from casting.

```forge
uint32  a = 0xFFFFFFFFuint32;
uint8   b = a as uint8;          // truncates — 0xFF
int32   c = a as int32;          // reinterpret bits — -1
uint64  d = a as uint64;         // zero-extends — 0x00000000FFFFFFFF
float32 e = 42uint32 as float32; // converts — 42.0
uptr    f = ptr as uptr;         // pointer to integer
uint32* g = f as uint32*;        // integer to pointer
```

---

## 4. Variables and Constants

### 4.1 Declaration Syntax

Variables are declared with the type first, then the name — identical to C.
There is no type inference. The type is always explicit.

```
[@attributes] type name [= expression] ;
```

```forge
uint32      x;        // immutable, zero-initialized
uint32*     ptr;      // immutable pointer, zero-initialized
[uint8;512] buf;      // immutable array, all bytes zero
GDTEntry    entry;    // immutable struct, all fields zero
```

### 4.2 Mutability — `@mut`

All variables are **immutable by default**. `@mut` makes a variable mutable.

```forge
uint32 x = 10uint32;
x = 20uint32;           // COMPILE ERROR: x is immutable

@mut uint32 y = 10uint32;
y = 20uint32;           // OK
y += 5uint32;           // OK
```

### 4.3 Zero Initialization

All variables are zero-initialized by default:

- Integer types → `0`
- Float types → `0.0`
- Boolean → `false`
- Pointer types → all-zero bits
- Array types → all elements zeroed
- Struct types → all fields zeroed
- Slice types → `ptr` zeroed, `len` = 0

The compiler always emits zero initialization explicitly. It does not rely
on BSS zeroing for stack variables.

### 4.4 Explicit Initialization

```forge
uint32   x     = 42uint32;
bool     flag  = true;
[uint8;4] magic = { 0x7Fuint8, 0x45uint8, 0x4Cuint8, 0x46uint8 };
Point    p     = Point { x: 10int32, y: 20int32 };
```

### 4.5 Uninitialized Variables — `@uninit`

`@uninit` opts out of zero initialization. The compiler tracks reads of
`@uninit` variables and emits a compile error if a variable may be read
before being written.

```forge
@uninit uint32        x;
@uninit [uint8; 4096] page_buf;

page_buf[0] = 0uint8;    // OK — first write
uint8 v = page_buf[0];   // OK — read after write

@uninit uint32 z;
uint32 w = z;            // COMPILE ERROR: z may be uninitialized
```

### 4.6 Attribute Composition

All attributes are commutative. Order never matters:

```forge
@mut @uninit uint32 x;    // mutable, uninitialized
@uninit @mut uint32 x;    // identical
```

### 4.7 Compile-Time Constants — `constexpr`

`constexpr` declares a compile-time constant. Always immutable. Always
evaluated at compile time. Type suffix required. Cannot be passed by pointer.

```forge
constexpr PAGE_SIZE:   usize  = 4096usize;
constexpr PAGE_SHIFT:  uint32 = 12uint32;
constexpr KERNEL_BASE: uptr   = 0xFFFF_FFFF_8000_0000uptr;
constexpr MAX_CPUS:    uint32 = 256uint32;
constexpr PAGE_MASK:   uptr   = ~(PAGE_SIZE as uptr - 1uptr);
```

### 4.8 Variable Scope

Variables are scoped to the block (`{ }`) in which they are declared. An
inner declaration of the same name shadows the outer one. Shadowing produces
a compiler warning.

---

## 5. Functions and Procedures

Forge distinguishes two kinds of callable units:

- **Functions** — pure by default. May not write to global state, perform
  I/O, or call procedures. Writing through pointer parameters is allowed.
- **Procedures** — declared with `@mut`. May have arbitrary side effects.
  Can write globals, perform port I/O, and call other procedures.

This distinction is enforced statically by the compiler. Calling a procedure
from a pure function is a compile error.

### 5.1 Function Declaration

A function is declared with its return type first, then its name, then its
parameter list, then its body. Parameter declarations use `type name` order,
consistent with variable declarations.

```
[@attributes] return_type name ( [parameters] ) block
```

```forge
uint32 add(uint32 a, uint32 b) {
    return a + b;
}

// Diverging function — never returns
halt() -> never {
    loop { asm("hlt"); }
}
```

### 5.2 Procedure Declaration

A procedure has no return type in its declaration. The `@mut` attribute is
required and serves as both the purity marker and the parser's signal that
this is a declaration rather than a call.

```
@mut [other_attributes] name ( [parameters] ) block
```

```forge
@mut setup_gdt() {
    // writes to GDT memory — side effect
    load_gdt(&gdt_descriptor);
}

@mut write_serial(uint8 byte) {
    uart_data <- byte;   // port I/O — side effect
}
```

The parser distinguishes a procedure declaration from a procedure call by
the presence of `@mut` before the identifier and a block body `{ }`.

### 5.3 Purity Rules

A pure function (no `@mut`) **may**:
- Read global variables and constants
- Write through pointer parameters
- Call other pure functions
- Execute pure inline assembly (no I/O, no memory-barrier side effects)
- Allocate on the stack

A pure function **may not**:
- Write to global variables
- Perform port I/O
- Call procedures (`@mut` functions)
- Execute `@volatile` assembly
- Use atomic store, fetch-modify, or compare-exchange operations

A procedure (`@mut`) **may** do anything.

Violation of purity rules is a compile error.

```forge
@mut uint32 global_counter = 0uint32;

uint32 compute(uint32 x) {
    global_counter += 1uint32;   // COMPILE ERROR: pure function writes global
    return x * 2uint32;
}

@mut tick() {
    global_counter += 1uint32;   // OK — procedure
}
```

### 5.4 Return Statement

`return` returns a value from a function. The last expression in a block,
written without a trailing semicolon, is implicitly returned:

```forge
uint32 mul(uint32 a, uint32 b) {
    a * b    // implicit return — no semicolon
}
```

A procedure that exits normally simply reaches the end of its block. An
explicit bare `return;` is also valid in a procedure.

### 5.5 Mutable Parameters

Parameters are immutable by default. `@mut` allows mutation inside the body.
This affects only the local copy — it does not affect the caller's variable.

```forge
uint32 sum_and_clear(@mut uint32* ptr) {
    uint32 val = *ptr;
    *ptr = 0uint32;    // writes through pointer — allowed in pure function
    return val;
}
```

### 5.6 First-Class Functions

Functions and procedures are first-class values. The function pointer type
includes the purity:

```forge
(uint32, uint32) -> uint32   fn_ptr = add;
fn_ptr(1uint32, 2uint32);

// Procedure pointer
@mut () handler = setup_gdt;
```

### 5.7 Closures

Closures use `|parameters| -> return_type { body }` syntax. Closures
capturing by copy are always permitted. Reference-capturing closures are
restricted to the enclosing scope's lifetime.

```forge
uint32 threshold = 128uint32;
(uint32) -> bool above = |uint32 x| -> bool { x > threshold };
```

A closure that writes to captured variables or calls procedures must itself
be declared `@mut`.

### 5.8 Calling Conventions

The default calling convention is the System V AMD64 ABI.

```forge
uint32 default_abi(uint32 x) { ... }

@cc("cdecl")   int32 cdecl_fn(int32 x) { ... }
@cc("stdcall") int32 stdcall_fn(int32 x) { ... }
@cc("sysv")    int32 sysv_fn(int32 x) { ... }
@cc("win64")   int32 win64_fn(int32 x) { ... }
```

### 5.9 Extern Declarations

`extern` declares a symbol defined in another translation unit:

```forge
extern uint32 some_asm_function(uint32 a, uint32 b);

@cc("cdecl")
extern int32 c_library_fn(int32 x);
```

### 5.10 Visibility — `@pub`

All declarations are private to their module by default. `@pub` makes a
declaration visible to importing modules.

```forge
// Private
uint32 internal_helper(uptr addr) { ... }

// Public
@pub uint32 map_page(uptr virt, uptr phys, uint32 flags) { ... }
@pub @mut init_paging() { ... }
```

`@pub` is commutative with all other attributes:

```forge
@pub @inline uint32 fast(uint32 x) { ... }
@inline @pub uint32 fast(uint32 x) { ... }   // identical
```

---

## 6. Control Flow

### 6.1 If / Else

```forge
if condition {
    ...
} else if other {
    ...
} else {
    ...
}
```

The condition must be of type `bool`. There is no implicit conversion from
integer to bool — `if x` where `x` is an integer is a compile error.

**If as expression.** Both branches must yield the same type:

```forge
uint32 abs_x = if x >= 0int32 { x as uint32 } else { (-x) as uint32 };
```

### 6.2 Loop

`loop` is an infinite loop exited with `break`. `continue` skips to the
next iteration.

```forge
loop {
    if done { break; }
    if skip { continue; }
    do_work();
}
```

**Loop as expression:**

```forge
uint32 result = loop {
    uint32 v = compute();
    if v > 100uint32 { break v; }
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

### 6.3 While

```forge
while condition {
    do_work();
}
```

### 6.4 For

```forge
// Exclusive range
for i in 0uint32..256uint32 {
    table[i] = 0uint8;
}

// Inclusive range
for i in 0uint32..=255uint32 {
    table[i] = 0uint8;
}

// Slice iteration
for byte in buffer {
    checksum ^= byte;
}

// Slice iteration with index
for i, byte in buffer {
    if byte == 0xFFuint8 { return i; }
}
```

The range bounds must be the same integer type. The loop variable is
immutable by default.

### 6.5 Match

`match` performs exhaustive value matching. Every possible value of the
scrutinee must be covered. There is no fall-through.

```forge
match fault_vector {
    0x00uint8 => divide_error(),
    0x01uint8 => debug_exception(),
    0x02uint8 => nmi_interrupt(),
    0x06uint8 => invalid_opcode(),
    0x08uint8 => double_fault(),
    0x0Duint8 => general_protection_fault(),
    0x0Euint8 => page_fault(),
    _         => unhandled_exception(fault_vector),
}
```

**Match as expression:**

```forge
[]uint8 name = match code {
    0uint32 => "OK",
    1uint32 => "Not Found",
    _       => "Unknown",
};
```

**Guards:**

```forge
match value {
    n if n < 0int32  => negative(n),
    n if n == 0int32 => zero(),
    n                => positive(n),
}
```

**Range arms:**

```forge
match port {
    0x20uint16..0x21uint16 => pic(port),
    0x40uint16..0x43uint16 => pit(port),
    _                      => unknown(port),
}
```

**Multiple values per arm:**

```forge
match code {
    1uint32 | 2uint32 | 3uint32 => retry(),
    _                           => abort(),
}
```

### 6.6 Defer

`defer` schedules a statement to execute when the current scope exits,
regardless of how it exits. Multiple `defer` statements execute in reverse
order (LIFO).

```forge
Fd fd = open_file("/kernel.fg");
defer close_file(fd);
// fd is closed no matter how this scope exits
```

`defer_err` executes only when the scope exits via an error path:

```forge
Fd fd = create_file("/output");
defer_err delete_file("/output");   // cleanup on failure only
defer close_file(fd);               // always runs
```

---

## 7. Pointers and Memory

### 7.1 Address-Of and Dereference

```forge
@mut uint32 x = 42uint32;
uint32*      p = &x;
uint32       v = *p;           // read
*p = 100uint32;                // write
```

Taking the address of an immutable variable produces `@const T*`. Writing
through `@const T*` is a compile error.

### 7.2 Pointer Arithmetic

```forge
uint32* next   = p + 1usize;                // advances by 4 bytes
uint8*  offset = (p as uint8*) + 3usize;    // byte arithmetic
```

### 7.3 Volatile Pointers

`@volatile` on the pointee prevents the compiler from reordering, merging,
or eliminating the access. Used for memory-mapped I/O registers.

```forge
// VGA text buffer
@volatile uint16* vga = 0xB8000uptr as @volatile uint16*;
*vga = 0x0F41uint16;   // guaranteed to reach hardware

// APIC EOI
constexpr APIC_EOI: uptr = 0xFEE0_00B0uptr;
@volatile uint32* eoi = APIC_EOI as @volatile uint32*;
*eoi = 0uint32;
```

### 7.4 Restrict Pointers

`@restrict` asserts that the pointer does not alias any other pointer in
scope. It is an opt-in assertion — the programmer is responsible for its
correctness.

```forge
@mut memcpy(@mut @restrict uint8* dst, @const @restrict uint8* src, usize len) {
    for i in 0usize..len { dst[i] = src[i]; }
}
```

### 7.5 Slices

```forge
[uint8; 512] buf;
[]uint8 s  = buf[0..];
[]uint8 s2 = buf[64usize..128usize];

uint8 first  = s[0usize];
uint8 last   = s[s.len - 1usize];
usize length = s.len;
uint8* raw   = s.ptr;
```

### 7.6 No Global Allocator

Forge has no global allocator. All allocation is performed explicitly by
passing an allocator object to functions that need it.

```forge
uint8* page = page_allocator.alloc(PAGE_SIZE);
defer page_allocator.free(page);
```

### 7.7 Stack Allocation

Local arrays and structs are stack-allocated. There is no implicit heap.

```forge
[uint8; 4096] stack_page;
GDTEntry      gdt_entries[8];
```

---

## 8. Structs

### 8.1 Declaration

Fields use `type name` order. Fields are comma-separated. Trailing comma
is permitted.

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

### 8.2 Instantiation

```forge
Point p  = Point { x: 10int32, y: 20int32 };
Point z  = Point {};                           // zero-initialized
Point p2 = Point { y: 99int32, ..p };          // struct update
```

### 8.3 Field Access

```forge
int32 px = p.x;
p.y = 20int32;   // COMPILE ERROR if p is immutable
```

### 8.4 Methods

Methods are defined in an `impl` block. The receiver is the first parameter.

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

A method that mutates its receiver must be `@mut` and take `*T self`.
A pure method takes `T self` (by copy) or `*T self` (by pointer, read-only).

### 8.5 Layout Attributes

```forge
@packed
struct GDTEntry {
    uint16 limit_low,
    uint16 base_low,
    uint8  base_mid,
    uint8  access,
    uint8  granularity,
    uint8  base_high,
}

@align(64)
struct CacheLine {
    [uint8; 64] data,
}

@repr("C")
struct CCompatible {
    int32 x,
    int32 y,
}
```

---

## 9. Unions

Unions allocate a single memory region shared by all fields. The size equals
the size of the largest field. Reading any field is defined — the result is
the current bit pattern of the memory reinterpreted as the requested type.

```forge
union FloatBits {
    float32 as_float,
    uint32  as_int,
}

@mut FloatBits fb   = FloatBits { as_float: 1.0float32 };
uint32         bits = fb.as_int;   // reads the float's bits as uint32
```

There is no undefined behaviour from reading any union field.

---

## 10. Enums

### 10.1 Simple Enums

A simple enum is an integer type with named values. The backing type must
be specified.

```forge
enum Color: uint8 {
    Red   = 0uint8,
    Green = 1uint8,
    Blue  = 2uint8,
}

Color  c   = Color.Red;
uint8  raw = c as uint8;
```

### 10.2 Algebraic Enums

Each variant may carry different data. The compiler generates the discriminant.

```forge
enum MemRegion {
    Free     { uptr base, usize size },
    Kernel   { uptr base, usize size, uint32 flags },
    MMIO     { uptr base, usize size, uint32 device_id },
    Reserved,
}
```

Algebraic enums must be matched exhaustively. It is a compile error to access
variant fields without first matching on the enum.

```forge
@mut describe(MemRegion r) {
    match r {
        Free     { base, size }      => log("free: {} at {:x}", size, base),
        Kernel   { base, flags, .. } => log("kernel at {:x}", base),
        MMIO     { device_id, .. }   => log("mmio device {}", device_id),
        Reserved                     => log("reserved"),
    }
}
```

The discriminant backing type may be set with `@repr`:

```forge
@repr("uint8")
enum Status {
    Ok    = 0uint8,
    Error { uint32 code },
    Pending,
}
```

---

## 11. Modules

### 11.1 Module Declaration

Each `.fg` file declares its module name at the top. The name maps to the
file path relative to the source root.

```forge
module kernel.memory.paging;
```

There are no header files and no forward declarations. Each name is declared
exactly once.

### 11.2 Visibility

All declarations are private by default. `@pub` makes a declaration visible
to importers.

```forge
module kernel.memory.frame;

uint32 internal_count() { ... }           // private

@pub uint32 alloc() { ... }               // public
@pub @mut free(uptr phys_addr) { ... }    // public procedure
@pub struct AllocError { ... }            // public type
```

### 11.3 Importing

```forge
// Import a specific name
import kernel.memory.paging.map_page;
map_page(virt, phys, flags);

// Import multiple names
import kernel.memory.paging.{map_page, unmap_page};

// Import with qualified access
import kernel.memory.paging;
kernel.memory.paging.map_page(virt, phys, flags);
```

There is no wildcard import. Every imported name must be listed explicitly.
This keeps dependencies visible and prevents name collision.

### 11.4 Module Resolution

`kernel.memory.paging` resolves to `kernel/memory/paging.fg` relative to the
source root. `forge build` compiles all transitively imported modules. Each
module is compiled exactly once.

---

## 12. Attributes

Attributes modify the declaration that immediately follows them. They use
`@name` or `@name(args)` syntax. Attributes always appear to the **left** of
the declaration. Multiple attributes on one declaration are **commutative** —
order never has semantic meaning.

```forge
@pub @inline uint32 fast(uint32 x) { ... }
@inline @pub uint32 fast(uint32 x) { ... }   // identical
```

See §20 for the complete attribute reference.

---

## 13. Inline Assembly

Inline assembly is a first-class language feature.

### 13.1 Single-Instruction Form

```forge
@mut enable_interrupts()  { asm("sti"); }
@mut disable_interrupts() { asm("cli"); }

halt() -> never { loop { asm("hlt"); } }
```

### 13.2 Full Form with Constraints

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

### 13.3 Multi-Line Block Form

```forge
@mut load_gdt(*GDTDescriptor desc) {
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

### 13.4 Volatile Assembly

`@volatile` on an `asm` block prevents the compiler from reordering or
eliminating it:

```forge
@mut memory_barrier() {
    @volatile asm("mfence" : : : "memory");
}
```

### 13.5 Constraint Reference

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
| `"="` | output (write-only) |
| `"+"` | output (read-write) |

Clobbers: `"memory"` — asm may access arbitrary memory. `"cc"` — asm
modifies the flags register.

---

## 14. Comptime

`comptime` evaluation runs entirely at compile time. It is typed, scoped,
and debuggable — not a text preprocessor.

### 14.1 Constexpr Blocks

```forge
comptime {
    constexpr PAGE_SHIFT: uint32 = 12uint32;
    constexpr PAGE_SIZE:  usize  = 1usize << PAGE_SHIFT;
    constexpr PAGE_MASK:  uptr   = ~(PAGE_SIZE as uptr - 1uptr);

    assert(@sizeof(GDTEntry) == 8usize, "GDT entry must be 8 bytes");
    assert(PAGE_SIZE == 4096usize,      "PAGE_SIZE must be 4096");
}
```

Compile-time assertions produce a compile error on failure — not a runtime
panic.

### 14.2 Comptime If — Conditional Compilation

`comptime if` replaces `#ifdef` entirely. It is typed, scoped, and produces
real declarations — not text substitution.

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

### 14.3 Comptime Functions

A function declared `comptime` is evaluated at compile time when called with
compile-time-known arguments.

```forge
comptime usize kib(usize n) { return n * 1024usize; }
comptime usize mib(usize n) { return n * 1024usize * 1024usize; }

constexpr HEAP_SIZE:  usize = mib(16usize);
constexpr STACK_SIZE: usize = kib(64usize);
```

### 14.4 Built-In Comptime Intrinsics

| Intrinsic | Type | Description |
|-----------|------|-------------|
| `@sizeof(T)` | `usize` | Size of type T in bytes |
| `@alignof(T)` | `usize` | Alignment of type T in bytes |
| `@offsetof(T, field)` | `usize` | Byte offset of field in struct T |
| `@bitcast<T>(val)` | `T` | Reinterpret bit pattern of val as T |
| `@error("msg")` | — | Emit compile error |
| `@warning("msg")` | — | Emit compile warning |

---

## 15. OS and Hardware Primitives

These features are always available without importing anything.

### 15.1 Port I/O

`port<T>` is the type of an x86 I/O port. `T` must be `uint8`, `uint16`,
or `uint32`. Port accesses translate directly to `in`/`out` instructions.

```forge
port<uint8> pic_master_cmd  = 0x20uint16;
port<uint8> pic_master_data = 0x21uint16;

// Write to port — outb/outw/outd depending on T
// Port writes are always side effects — only valid in @mut context
@mut send_eoi() {
    pic_master_cmd <- 0x20uint8;
}

// Read from port — inb/inw/ind depending on T
uint8 read_status() {
    return pic_master_cmd.read();
}
```

### 15.2 Interrupt Service Routines

`@interrupt` generates a complete ISR prologue and epilogue: saves all
caller-saved registers, calls the handler, restores registers, executes
`iretq`. ISR handlers are always procedures — they have side effects.

```forge
@interrupt @mut isr_divide_error(*InterruptFrame frame) {
    panic("Divide by zero at rip={:x}", frame.rip);
}

// Vectors that push an error code
@interrupt @mut isr_page_fault(*InterruptFrame frame, uint64 error_code) {
    uint64 cr2 = @cpu.read_cr2();
    handle_page_fault(cr2, error_code, frame);
}

// @naked — no generated prologue or epilogue
@naked @mut isr_double_fault() -> never {
    asm {
        call double_fault_handler
    .hang:
        hlt
        jmp .hang
    }
}
```

The `InterruptFrame` struct matches the layout pushed by the CPU:

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

### 15.3 Linker Symbol Integration

Symbols defined in the linker script are declared with `extern`:

```forge
extern uint8 __kernel_start;
extern uint8 __kernel_end;
extern uint8 __bss_start;
extern uint8 __bss_end;
extern uint8 __stack_top;

@mut clear_bss() {
    uptr  start = &__bss_start as uptr;
    uptr  end   = &__bss_end   as uptr;
    usize len   = (end - start) as usize;
    memset(start as *uint8, 0uint8, len);
}
```

### 15.4 CPU Register Access

Built-in `@cpu` intrinsics provide access to control registers and MSRs.
All writes are procedures (side effects).

```forge
// Control register reads — pure
uint64 cr0 = @cpu.read_cr0();
uint64 cr2 = @cpu.read_cr2();
uint64 cr3 = @cpu.read_cr3();
uint64 cr4 = @cpu.read_cr4();

// Control register writes — @mut context required
@mut init_paging() {
    @cpu.write_cr3(pml4_phys);
    @cpu.write_cr4(@cpu.read_cr4() | (1uint64 << 5uint64));  // PAE
}

// MSR access
uint64 efer = @cpu.rdmsr(0xC000_0080uint32);

@mut enable_long_mode() {
    @cpu.wrmsr(0xC000_0080uint32, efer | (1uint64 << 8uint64));
}

// CPUID — pure, reads CPU state
struct CpuidResult { uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, }
CpuidResult info     = @cpu.cpuid(1uint32, 0uint32);
bool        has_sse2 = (info.edx >> 26uint32) & 1uint32 == 1uint32;

// Interrupt flag
@mut @cpu.enable_interrupts();    // sti
@mut @cpu.disable_interrupts();   // cli
bool ie = @cpu.interrupts_enabled();
```

### 15.5 Atomic Operations

`atomic<T>` provides indivisible operations with explicit memory ordering.
There is no implicit synchronization anywhere else in the language.

```forge
atomic<uint64> counter;   // zero-initialized

// All fetch-modify operations return the old value
// All are @mut — they have side effects
@mut tick() {
    counter.fetch_add(1uint64, .SeqCst);
}

uint64 read_counter() {
    return counter.load(.Acquire);   // pure — just a read
}
```

**Memory orderings:**

| Ordering | Meaning |
|---------|---------|
| `.Relaxed` | Atomic only — no ordering relative to other operations |
| `.Acquire` | This load sees all stores before the matching Release |
| `.Release` | All prior stores visible to a matching Acquire |
| `.AcqRel` | Both Acquire and Release |
| `.SeqCst` | Total sequential consistency |

---

## 16. Memory Model

### 16.1 Execution Model

Forge targets a single-threaded execution model at the language level. There
is no thread primitive. Concurrency is expressed through atomic operations,
memory-mapped registers, and interrupt handlers.

### 16.2 Address Space

The compiler generates code for a flat 64-bit virtual address space. Physical
address management is the programmer's responsibility. `uptr` is the type
for raw addresses. `@volatile` handles MMIO.

### 16.3 Stack

The stack grows downward. Stack frames follow the System V AMD64 ABI (§18).
`@naked` functions have no generated frame — the programmer maintains the
invariants.

### 16.4 Object Lifetime

A local variable's lifetime begins at its declaration and ends at the end of
the enclosing block. Using a pointer to a local variable after its lifetime
is a compile error where detectable.

---

## 17. Undefined Behaviour

**Forge has no undefined behaviour.**

Every operation has a defined result. The following operations are UB in C
and defined in Forge:

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

## 18. ABI and Calling Conventions

### 18.1 Default — System V AMD64

**Integer/pointer arguments:** `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.
Additional arguments on the stack.

**Float arguments:** `xmm0`–`xmm7`.

**Return values:**
- ≤ 64-bit integer/pointer: `rax`
- 65–128-bit integer: `rax:rdx`
- Float: `xmm0`

**Caller-saved:** `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`–`r11`, `xmm0`–`xmm15`

**Callee-saved:** `rbx`, `rbp`, `r12`–`r15`

**Stack alignment:** 16-byte aligned before `call`.

### 18.2 Struct Passing

Structs ≤ 16 bytes are passed in registers per the AMD64 classification
algorithm. Structs > 16 bytes are passed on the stack by value.

### 18.3 Interrupt Frame Layout

On interrupt entry the CPU pushes onto the kernel stack:

```
[rsp+32]  ss
[rsp+24]  rsp
[rsp+16]  rflags
[rsp+ 8]  cs
[rsp+ 0]  rip
```

For error-code vectors:

```
[rsp+40]  ss
[rsp+32]  rsp
[rsp+24]  rflags
[rsp+16]  cs
[rsp+ 8]  rip
[rsp+ 0]  error_code
```

---

## 19. Object Files and Linking

### 19.1 Output Format

`forgec` produces ELF64 object files. These are linked by `ld` or `lld`.
The compiler does not include a linker.

### 19.2 Sections

| Section | Contents |
|---------|----------|
| `.text` | Compiled function bodies |
| `.rodata` | String literals, read-only data |
| `.data` | Initialized mutable globals |
| `.bss` | Zero-initialized globals |
| `.note.forge` | Compiler version information |

Additional sections are created by `@section("name")`.

### 19.3 Symbol Visibility

Private symbols are local to the object file. `@pub` symbols are exported
with global visibility. `@export("name")` sets the exact symbol name,
bypassing name mangling.

### 19.4 Name Mangling

Forge mangles symbol names to encode module path and parameter types. The
scheme is not stable across compiler versions. Use `@export("name")` for
stable external symbols.

---

## 20. Compiler Attributes Reference

### 20.1 Variable Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Variable is mutable after initialization |
| `@uninit` | Not zero-initialized; compiler warns on read before write |
| `@const` | Pointee is immutable |
| `@volatile` | Pointee accesses never reordered or eliminated |
| `@restrict` | No aliasing with other pointers in scope |
| `@section("name")` | Place in named ELF section |
| `@align(N)` | Minimum alignment; N must be power of 2 |

### 20.2 Function / Procedure Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Declares a procedure — side effects permitted |
| `@pub` | Visible to other modules |
| `@inline` | Always inline at call site |
| `@noinline` | Never inline |
| `@naked` | No generated prologue, epilogue, or stack frame |
| `@cc("name")` | Set calling convention |
| `@interrupt` | Generate ISR-safe prologue/epilogue + `iretq` |
| `@cold` | Rarely executed — optimizer deprioritizes |
| `@section("name")` | Place in named ELF section |
| `@export("name")` | Export with exact symbol name |
| `@deprecated("msg")` | Warn at every call site |

### 20.3 Type Attributes

| Attribute | Applies To | Effect |
|-----------|-----------|--------|
| `@pub` | struct / enum / union | Visible to importers |
| `@packed` | struct | Remove all padding |
| `@align(N)` | struct | Force minimum alignment |
| `@repr("C")` | struct / enum | C-compatible layout |
| `@repr("uintN")` | enum | Set discriminant backing type |

---

## 21. What Forge Deliberately Omits

| Omitted Feature | Reason |
|----------------|--------|
| **Garbage collector** | Unpredictable pause times; incompatible with interrupt handlers |
| **Exceptions** | Hidden control flow — violates §1.2 |
| **Type inference** | You always know the type. Inference hides it from the reader. |
| **`fn` keyword** | Redundant. The return type is sufficient. C got this right. |
| **`void` keyword** | Procedures are declared with `@mut` and no return type. |
| **`pub` keyword** | Visibility is an attribute. `@pub` is consistent with everything else. |
| **`const` keyword** | Replaced by `constexpr` — unambiguously compile-time. |
| **Wildcard imports** | Every imported name must be listed. Dependencies stay visible. |
| **Implicit integer promotion** | Source of a significant fraction of C CVEs. |
| **Null pointers** | NULL does not exist. Nullability is expressed in the type system. |
| **Undefined behaviour** | See §17. Every operation is defined. |
| **Text preprocessor** | `comptime if` replaces `#ifdef`. Modules replace `#include`. |
| **Header files** | Modules with `@pub` declarations replace this entirely. |
| **Global allocator** | The kernel defines its own memory model. Forge does not impose one. |
| **Hidden copies** | Large types must be passed by pointer explicitly. |
| **Implicit conversions** | Every type change requires explicit `as`. |
| **Variadic functions** | Use slices instead. |
| **`goto`** | Use `loop { break; }` or labeled breaks. |
| **Operator overloading** | `+` always means addition. No surprises in hot paths. |
| **Dynamic dispatch / vtables** | No `dyn`, no virtual. All dispatch is static. |
| **RTTI** | No runtime type information of any kind. |

---

## 22. Appendix A — Kernel Entry Point Example

```forge
module kernel.boot;

import kernel.memory.paging.{init_paging};
import kernel.memory.frame.{init_frame_allocator};
import kernel.cpu.gdt.{init_gdt};
import kernel.cpu.idt.{init_idt};
import kernel.drivers.uart.{init_uart, uart_write};

extern uint8 __bss_start;
extern uint8 __bss_end;
extern uint8 __stack_top;

/// Kernel entry point — called by bootloader.
/// multiboot_magic must be 0x36D76289 for Multiboot2.
/// multiboot_info is the physical address of the info structure.
@pub @section(".boot.text") @export("kernel_main")
@mut kernel_main(uint32 multiboot_magic, uptr multiboot_info) -> never {
    // Clear BSS before anything else
    clear_bss();

    // CPU structures — before any interrupts
    init_gdt();
    init_idt();

    // Early serial output
    init_uart(0x3F8uint16, 115200uint32);
    uart_write("Forge kernel starting\n");

    // Validate bootloader
    if multiboot_magic != 0x36D7_6289uint32 {
        panic("Invalid Multiboot2 magic: {:x}", multiboot_magic);
    }

    // Memory subsystem
    init_frame_allocator(multiboot_info);
    init_paging();

    uart_write("Memory OK\n");

    idle_loop();
}

@mut clear_bss() {
    uptr  start = &__bss_start as uptr;
    uptr  end   = &__bss_end   as uptr;
    usize len   = (end - start) as usize;
    @mut uint8* ptr = start as @mut uint8*;
    for i in 0usize..len { ptr[i] = 0uint8; }
}

@mut idle_loop() -> never {
    loop {
        @cpu.disable_interrupts();
        asm("hlt");
        @cpu.enable_interrupts();
    }
}
```

---

## 23. Appendix B — Physical Frame Allocator Example

```forge
module kernel.memory.frame;

import kernel.memory.frame.{AllocError};

/// One bit per 4 KiB physical frame. Supports up to 4 GiB.
constexpr MAX_FRAMES: usize = 1_048_576usize;

@uninit [uint8; MAX_FRAMES / 8usize] bitmap;
@mut usize total_frames = 0usize;
@mut usize free_frames  = 0usize;

@pub struct AllocError {
    uint8 code,
}

/// Initialize from a Multiboot2 memory map.
/// Marks all frames used, then marks usable regions free.
@pub @mut init(uptr multiboot_info) {
    // Start with everything marked used
    for i in 0usize..bitmap.len {
        bitmap[i] = 0xFFuint8;
    }

    *MemoryMap map = multiboot2_get_memory_map(multiboot_info);

    for entry in map {
        if entry.kind == 1uint32 {   // 1 = usable RAM
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
@pub @mut alloc() -> uptr {
    for i in 0usize..bitmap.len {
        if bitmap[i] != 0xFFuint8 {
            for bit in 0uint8..8uint8 {
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

/// Total usable frames discovered during init.
@pub usize total() {
    return total_frames;
}

set_free(usize frame) {
    usize byte = frame / 8usize;
    uint8 bit  = (frame % 8usize) as uint8;
    bitmap[byte] &= ~(1uint8 << bit);
}
```

---

## 24. Appendix C — Compile-Time Guarantees

A conforming `forgec` implementation guarantees:

**Type safety.** No program that compiles without errors can read a value as
a different type without an explicit `as` or `@bitcast`, access a nonexistent
struct field, or call a function with wrong argument types.

**Purity enforcement.** No pure function writes to global state or performs
I/O. The compiler verifies this statically. A procedure call from a pure
function is a compile error.

**No silent integer conversions.** Every integer type change corresponds to
an explicit `as` in the source.

**No hidden control flow.** Every exit path from every function is visible
in the source. There are no destructors with side effects, no implicit
exception unwind paths, and no implicit early returns.

**Exhaustive matching.** Every `match` on an algebraic enum covers all
variants. Adding a variant is a compile error in every file that matches
on that enum without a wildcard arm.

**Defined overflow.** Signed and unsigned integer overflow wraps in
two's complement. The compiler never uses overflow as an optimization
assumption.

**Zero initialization.** Every variable not marked `@uninit` is provably
zero-initialized before first read. The compiler emits this initialization
explicitly — it does not rely on linker or loader behaviour for stack
variables.

**Uninitialized read prevention.** Every `@uninit` variable is tracked.
Reading it before a provable write is a compile error.

---

*The Forge Programming Language Specification, Version 1.1*
*Copyright © 2026 — GNU General Public License v3.0*
*The Forge compiler (forgec) is licensed under GPLv3 with a runtime exception.*
*Code produced by forgec is not subject to the GPL.*
