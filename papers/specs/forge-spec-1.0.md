# The Forge Programming Language Specification

**Version:** 1.0
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
The entire compiler targets approximately 20,000 lines of source code. This is
a design constraint, not an accident. A compiler you can read in a weekend is
a compiler you can trust.

**Forge is licensed under the GNU General Public License v3.0.**
Code compiled by Forge is not subject to the GPL — see the compiler runtime
exception at the end of this document.

---

## Changelog

| Version | Changes |
|---------|---------|
| 1.0 | Removed `fn` keyword — functions use C-style return-type-first syntax. Pure functions are the default. `@mut` on a function declaration marks it as a procedure (has side effects). Removed `void` keyword — procedures have no return type keyword. Removed `i8`/`u8` etc — replaced with `int8`/`uint8` etc. Removed `use` keyword — specific imports use `import foo.bar.name()`. Removed generics, pattern matching, and error handling sections pending redesign. |

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
25. [Appendix D — Compiler Runtime Exception](#25-appendix-d--compiler-runtime-exception)

---

## 1. Design Principles

These principles are ordered by priority. When two principles conflict, the
higher-numbered one yields to the lower-numbered one.

### 1.1 No Undefined Behaviour

Every operation in Forge has a defined result. The compiler never uses the
absence of a definition as an optimization license. There is no concept of
undefined behaviour in this specification. Every edge case is explicitly
specified.

Signed integer overflow wraps. Division by zero traps. Out-of-bounds access
traps. Uninitialized reads are a compile error unless explicitly opted out of
with `@uninit`. Null pointer dereferences cannot occur because null pointers
do not exist as plain values.

### 1.2 Explicit Over Implicit

If an operation costs cycles, allocates memory, changes a type, copies a
value, or has a side effect — it must be written explicitly in the source.

- No type inference
- No implicit integer promotion
- No implicit conversions between any types
- No hidden copies of values
- No hidden allocations
- No hidden control flow

### 1.3 Pure by Default

All functions in Forge are **pure by default**. A pure function:

- Does not read or write global mutable state
- Does not perform I/O of any kind (port I/O, MMIO writes, system calls)
- Does not call any procedure (`@mut` function)

Purity is enforced by the compiler. Violating these rules in a pure function
is a compile error.

Writing through a pointer parameter is permitted in a pure function. The
side effect is explicit in the function signature — the caller passed a
pointer and knows it may be written. This is not considered a violation of
purity.

A function that has side effects — global state, I/O, or calls to other
procedures — must be declared with `@mut`. Such functions are called
**procedures**.

### 1.4 Zero-Cost Abstractions

Every abstraction in Forge compiles to machine code identical to what a
competent programmer would write by hand in C. If an abstraction cannot be
zero-cost, it does not exist in the language.

### 1.5 Freestanding First

Forge does not assume the existence of an operating system, a C runtime, a
heap, or a standard library. Every language feature works at interrupt level,
in a bootloader, or on bare metal with no prior initialization.

### 1.6 Predictable Code Generation

The programmer should be able to predict the assembly output of any Forge
function without running the compiler. There is no hidden virtual dispatch,
no hidden state machine generation, no surprise stack allocations, and no
compiler transformations that change the observable behaviour of a correct
program.

---

## 2. Lexical Structure

### 2.1 Source Files

Forge source files are UTF-8 encoded text with the `.fg` extension. Line
endings are `LF` (`\n`) or `CRLF` (`\r\n`), normalized to `LF` before
processing.

### 2.2 Comments

```forge
// Single-line comment — extends to end of line

/*
   Block comment.
   Block comments nest: /* this is valid */ still in comment
*/

/// Documentation comment — attached to the immediately following declaration.
/// Content is treated as Markdown by documentation tooling.
uint32 add(uint32 a, uint32 b) {
    return a + b;
}
```

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

The following words are **not** keywords and may be used as identifiers,
though doing so is discouraged:

```
mut   uninit   pub   const   let   var   never
```

`never` is a built-in type name resolved during semantic analysis, not
parsing.

### 2.4 Identifiers

```
identifier ::= [a-zA-Z_][a-zA-Z0-9_]*
```

Identifiers are case-sensitive. Identifiers beginning with `_` are reserved
for compiler internal use. Identifiers beginning with `__` are reserved for
the linker and platform ABI.

### 2.5 Integer Literals

All integer literals require an explicit type suffix. A literal without a
suffix is a compile error. Underscores may appear anywhere within the digit
sequence for readability.

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

`true` and `false` are keywords. They are the only values of type `bool`.
`bool` is not an integer — there is no implicit conversion.

### 2.8 Character Literals

A character literal produces a value of type `uint32` containing the Unicode
codepoint of the character.

```forge
uint32 a = 'A';           // 65
uint32 b = '\n';          // 10
uint32 c = '\t';          // 9
uint32 d = '\\';          // 92
uint32 e = '\u{1F600}';   // Unicode codepoint
uint32 f = '\x41';        // hex byte — 65
```

### 2.9 String Literals

A string literal produces a value of type `[]uint8`. The string is UTF-8
encoded with no null terminator. Length is available via `.len`.

```forge
[]uint8 a = "hello, kernel";
[]uint8 b = "line one\nline two";
[]uint8 c = r"raw string — no \n processing";
```

String literals are stored in `.rodata`. They are immutable. Writing through
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
Port I/O:      <-  (write to port)    ->  (read from port)
Member:        .
Index:         [ ]
```

---

## 3. Type System

Forge is statically typed. Every expression has a type known at compile time.
There is no type inference. All variable declarations require an explicit type.
There are no implicit conversions — every type change requires an explicit `as`.

### 3.1 Integer Types

| Type | Width | Description |
|------|-------|-------------|
| `uint8` | 8 bits | Unsigned integer |
| `uint16` | 16 bits | Unsigned integer |
| `uint32` | 32 bits | Unsigned integer |
| `uint64` | 64 bits | Unsigned integer |
| `uint128` | 128 bits | Unsigned integer |
| `int8` | 8 bits | Signed integer |
| `int16` | 16 bits | Signed integer |
| `int32` | 32 bits | Signed integer |
| `int64` | 64 bits | Signed integer |
| `int128` | 128 bits | Signed integer |
| `usize` | 64 bits (x86-64) | Pointer-sized unsigned — for lengths and indices |
| `isize` | 64 bits (x86-64) | Pointer-sized signed |
| `uptr` | 64 bits (x86-64) | Raw address — a plain integer, not a pointer |

`usize` is for array lengths, loop indices, and memory sizes.
`uptr` is for raw hardware addresses and address arithmetic.
They are distinct types and cannot be implicitly converted.

**Integer overflow** is defined in Forge. Both signed and unsigned integers
wrap on overflow. Explicit behaviour can be requested at the call site:

```forge
uint32 a = x.wrapping_add(y);    // wraps — same as default
uint32 b = x.saturating_add(y);  // clamps to MAX
```

### 3.2 Float Types

| Type | Width | Standard |
|------|-------|----------|
| `float32` | 32 bits | IEEE 754 single precision |
| `float64` | 64 bits | IEEE 754 double precision |

Float operations follow IEEE 754 strictly.

### 3.3 Boolean Type

`bool` is 1 byte. Values are `true` and `false`. Not an integer — no
implicit conversion exists in either direction.

### 3.4 Never Type

`never` is the return type of functions that do not return. A `never`-typed
expression may appear anywhere a value is expected because it is never
actually reached.

```forge
never halt() {
    loop { asm("hlt"); }
}
```

### 3.5 Pointer Types

```forge
uint32*           // raw pointer to mutable uint32
@const uint32*    // raw pointer to immutable uint32
@volatile uint32* // MMIO pointer — accesses never optimized away
```

**Pointers are always non-null.** A nullable pointer is expressed as
`Option<*T>` (to be specified in a future version). `NULL` does not exist
as a plain value.

Pointer arithmetic is in units of `sizeof(T)`. For byte arithmetic, cast
to `uint8*` first.

### 3.6 Slice Types

A slice is a fat pointer: `(ptr: *T, len: usize)`. All slice accesses are
bounds-checked — at compile time where possible, trapping at runtime
otherwise.

```forge
[]uint8         // mutable slice of uint8
[]@const uint8  // immutable slice of uint8
```

Slice fields: `.ptr` (raw pointer) and `.len` (element count).

### 3.7 Array Types

Arrays are fixed-size and stack-allocated. The size must be a `constexpr`
value.

```forge
[uint8; 512]      buf;
[uint32; 1024]    table;
[GDTEntry; 8]     gdt;
```

### 3.8 Tuple Types

An anonymous struct with positionally-named fields.

```forge
(uint32, bool, uint8) t  = (42uint32, true, 0xFFuint8);
uint32                lo = t.0;
bool                  b  = t.1;
```

### 3.9 Function Pointer Types

```forge
uint32(uint32, uint32)   // pointer to pure function: two uint32 args → uint32
@mut uint32(uint32)      // pointer to procedure: uint32 arg → uint32
@mut (uint32)            // pointer to procedure: uint32 arg → no return value
```

### 3.10 Bitfield Integers

Inside `@packed` or `@repr("C")` structs, integers of arbitrary bit width
between 1 and 64 may be used for hardware register layouts:

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

Bitfield integers (`uint1`–`uint64`, `int1`–`int64`) are valid only inside
struct declarations. They may not be used as standalone variable types.

### 3.11 Atomic Types

`atomic<T>` wraps an integer or pointer type and provides indivisible
read-modify-write operations with explicit memory ordering. See §15.5.

### 3.12 Port Type

`port<T>` is the type of an x86 I/O port. `T` must be `uint8`, `uint16`,
or `uint32`. See §15.1.

### 3.13 Type Casting

All type conversions are explicit and use `as`. Every cast has a defined
result — no cast invokes undefined behaviour.

```forge
uint32  a = 0xFFFFFFFFuint32;
uint8   b = a as uint8;      // truncates — 0xFF
int32   c = a as int32;      // reinterprets bits — -1
uint64  d = a as uint64;     // zero-extends — 0x00000000FFFFFFFF
float32 e = 42uint32 as float32;  // converts — 42.0
uptr    f = ptr as uptr;     // pointer to integer
uint32* g = f as uint32*;    // integer to pointer
```

---

## 4. Variables and Constants

### 4.1 Declaration Syntax

Variables are declared type-first, then name — identical to C. No type
inference. The type is always explicit.

```
[@attributes] type name [= expression] ;
```

```forge
uint32     x;       // immutable, zero-initialized
uint32*    ptr;     // immutable pointer, zero-initialized
[uint8;512] buf;   // immutable array, all bytes zero
GDTEntry   entry;  // immutable struct, all fields zero
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

| Type | Zero value |
|------|-----------|
| Integer types | `0` |
| Float types | `0.0` |
| `bool` | `false` |
| Pointer types | all-zero bits |
| Array types | all elements zero |
| Struct types | all fields zero |
| Slice types | `ptr = zero, len = 0` |

### 4.4 Explicit Initialization

```forge
uint32    x     = 42uint32;
bool      flag  = true;
[uint8;4] magic = { 0x7Fuint8, 0x45uint8, 0x4Cuint8, 0x46uint8 };
Point     p     = Point { x: 10int32, y: 20int32 };
```

### 4.5 Uninitialized Variables — `@uninit`

`@uninit` opts out of zero initialization. The compiler tracks reads and
emits an error if a variable may be read before being written.

```forge
@uninit uint32       x;
@uninit [uint8;4096] page_buf;

page_buf[0] = 0uint8;    // OK — write before read
uint8 v = page_buf[0];   // OK — read after write

@uninit uint32 z;
uint32 w = z;            // COMPILE ERROR: z may be uninitialized
```

`@mut` and `@uninit` are independent and **commutative** — order never
matters:

```forge
@mut @uninit uint32 x;   // mutable, uninitialized
@uninit @mut uint32 x;   // identical
```

### 4.6 Compile-Time Constants — `constexpr`

`constexpr` declares a compile-time constant. Always immutable, always
evaluated at compile time. Type suffix required. Has no runtime address.

```forge
constexpr PAGE_SIZE:   usize  = 4096usize;
constexpr PAGE_SHIFT:  uint32 = 12uint32;
constexpr KERNEL_BASE: uptr   = 0xFFFF_FFFF_8000_0000uptr;
constexpr MAX_CPUS:    uint32 = 256uint32;
constexpr PAGE_MASK:   uptr   = ~(PAGE_SIZE as uptr - 1uptr);
```

---

## 5. Functions and Procedures

Forge has two kinds of callable units:

- **Function** — pure by default. No global state, no I/O, no calls to
  procedures. Declared with a return type first, then name and parameters.
- **Procedure** — has side effects. Marked with `@mut`. May read/write
  global state, perform I/O, and call other procedures.

Both use C-style syntax: return type first, then name, then parameter list.
A procedure with no return value has no return type keyword — the `@mut`
attribute alone signals that it is a declaration.

### 5.1 Pure Function Syntax

```forge
// Return type first, then name, then parameters
uint32 add(uint32 a, uint32 b) {
    return a + b;
}

// Last expression is implicitly returned (no semicolon)
uint32 mul(uint32 a, uint32 b) {
    a * b
}

// Diverging function — return type is `never`
never halt() {
    loop { asm("hlt"); }
}
```

### 5.2 Procedure Syntax — `@mut`

A procedure is a function with side effects. `@mut` appears to the left of
the return type (or to the left of the name, for no-return-value procedures).

```forge
// Procedure with no return value — @mut, no return type keyword
@mut setup_gdt() {
    // writes to hardware, modifies global state
    gdt_entries[0] = make_null_descriptor();
    load_gdt(&gdt_descriptor);
}

// Procedure with a return value
@mut uint32 next_pid() {
    pid_counter += 1uint32;
    return pid_counter;
}
```

**Parser disambiguation.** The compiler distinguishes a procedure declaration
from a function call as follows:

- `@mut name(...)` followed by `{` → procedure declaration
- `name(...)` followed by `;` or used in an expression → function call

### 5.3 Purity Rules

A pure function (no `@mut`) is enforced by the compiler to:

| Operation | Pure function | Procedure (`@mut`) |
|-----------|--------------|-------------------|
| Read local variables | ✓ | ✓ |
| Write local variables | ✓ | ✓ |
| Read `@mut` globals | ✗ compile error | ✓ |
| Write `@mut` globals | ✗ compile error | ✓ |
| Port I/O | ✗ compile error | ✓ |
| MMIO writes | ✗ compile error | ✓ |
| Call pure functions | ✓ | ✓ |
| Call procedures (`@mut`) | ✗ compile error | ✓ |
| Write through pointer param | ✓ | ✓ |
| Inline assembly | ✗ compile error | ✓ |

```forge
// Pure — allowed: write through pointer parameter
uint32 compute(@mut uint32* out, uint32 x) {
    *out = x * 2uint32;   // OK — explicit in signature
    return x + 1uint32;
}

// Pure — NOT allowed: write to global
@mut uint32 global_counter = 0uint32;

uint32 bad() {
    global_counter += 1uint32;   // COMPILE ERROR: pure function modifies global
    return 0uint32;
}
```

### 5.4 Mutable Parameters

Parameters are immutable by default. `@mut` allows mutation within the
function body. This does not affect the calling convention — parameters are
passed by value. Mutating a `@mut` parameter does not affect the caller's
variable.

```forge
uint32 clamped(@mut uint32 x, uint32 lo, uint32 hi) {
    if x < lo { x = lo; }
    if x > hi { x = hi; }
    return x;
}
```

To modify the caller's variable, pass a pointer:

```forge
@mut increment(@mut uint32* x) {
    *x += 1uint32;
}
```

### 5.5 First-Class Functions

Functions are first-class values. The type of a pure function value is
`return_type(arg_types)`. The type of a procedure value is
`@mut return_type(arg_types)`.

```forge
// Store a function in a variable
uint32(uint32, uint32) op = add;
uint32 result = op(10uint32, 20uint32);

// Pass a function as a parameter
@mut apply([]uint32 arr, uint32(uint32) f, @mut []uint32 out) {
    for i in 0usize..arr.len {
        out[i] = f(arr[i]);
    }
}
```

### 5.6 Closures

Closures use `|parameters| -> return_type { body }` syntax. Closures that
capture by copy (trivially copyable types: integers, pointers, booleans) are
pure if they perform no side effects. Closures never heap-allocate implicitly.

```forge
uint32 threshold = 128uint32;

// Captures threshold by copy — pure closure
bool(uint32) above = |uint32 x| -> bool { x > threshold };
```

### 5.7 Calling Conventions

The default calling convention is System V AMD64. `@cc` overrides it:

```forge
uint32 default_abi(uint32 x) { ... }

@cc("cdecl")   int32 cdecl_fn(int32 x) { ... }
@cc("stdcall") int32 stdcall_fn(int32 x) { ... }
@cc("sysv")    int32 sysv_fn(int32 x) { ... }
@cc("win64")   int32 win64_fn(int32 x) { ... }
```

### 5.8 Extern Functions

```forge
extern @mut void* memset(void* ptr, int32 val, usize len);
extern @mut void* memcpy(void* dst, @const void* src, usize len);
```

### 5.9 Visibility — `@pub`

All declarations are private by default. `@pub` makes a declaration visible
to importing modules.

```forge
// Private
uint32 internal_checksum([]uint8 buf) { ... }

// Public
@pub uint32 crc32([]uint8 data) { ... }
@pub @mut setup_idt() { ... }
```

---

## 6. Control Flow

### 6.1 If / Else

The condition must be of type `bool`. There is no implicit integer-to-bool
conversion.

```forge
if x > 0uint32 {
    positive();
} else if x == 0uint32 {
    zero();
} else {
    negative();
}

// If as expression — both branches must yield the same type
uint32 abs_x = if x >= 0int32 { x as uint32 } else { (-x) as uint32 };
```

### 6.2 Loop

```forge
loop {
    if done { break; }
    if skip { continue; }
    do_work();
}

// Loop as expression — break carries a value
uint32 result = loop {
    uint32 v = compute();
    if v > 100uint32 { break v; }
};

// Labeled loops
'outer: loop {
    loop {
        if condition { break 'outer; }
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

Range bounds must be the same integer type. The loop variable is immutable
by default.

### 6.5 Match

Match is **exhaustive** — all possible values must be covered. No fall-through.

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

// Match as expression
[]uint8 name = match code {
    0uint32 => "OK",
    1uint32 => "Not Found",
    _       => "Unknown",
};

// Guards
match value {
    n if n < 0int32  => negative(n),
    n if n == 0int32 => zero(),
    n                => positive(n),
}

// Range arms
match port {
    0x20uint16..0x21uint16 => pic(port),
    0x40uint16..0x43uint16 => pit(port),
    _                      => unknown(port),
}

// Multiple values per arm
match code {
    1uint32 | 2uint32 | 3uint32 => retry(),
    _                           => fail(),
}
```

### 6.6 Defer

`defer` runs an expression when the current scope exits, regardless of how.
Multiple defers execute in reverse order (LIFO).

```forge
@mut copy_file([]uint8 src, []uint8 dst) {
    Fd in_fd  = open(src);
    defer close(in_fd);     // always runs on scope exit

    Fd out_fd = create(dst);
    defer_err delete_file(dst);  // runs only on error path
    defer close(out_fd);

    copy(in_fd, out_fd);
}
```

---

## 7. Pointers and Memory

### 7.1 Address-Of and Dereference

```forge
@mut uint32 x = 42uint32;
uint32*      p = &x;
uint32       v = *p;        // read
*p = 100uint32;             // write
```

Taking the address of an immutable variable produces `@const T*`. Writing
through `@const T*` is a compile error.

### 7.2 Pointer Arithmetic

```forge
uint32* next   = p + 1usize;               // +4 bytes
uint8*  offset = (p as uint8*) + 3usize;   // byte arithmetic — explicit cast
```

### 7.3 Volatile Pointers (MMIO)

`@volatile` on a pointee tells the compiler: never reorder, merge, or
eliminate this access.

```forge
@volatile uint16* vga = 0xB8000uptr as @volatile uint16*;
*vga = 0x0F41uint16;    // white 'A' — write is never eliminated

constexpr APIC_EOI: uptr = 0xFEE0_00B0uptr;
@volatile uint32* eoi = APIC_EOI as @volatile uint32*;
*eoi = 0uint32;
```

### 7.4 Restrict Pointers

`@restrict` asserts no aliasing. Opt-in — your responsibility.

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

Forge has no global allocator. All allocation requires an explicit allocator
argument.

```forge
@mut kernel_init() {
    void* page = page_allocator.alloc(PAGE_SIZE);
    defer page_allocator.free(page);
    // ...
}
```

---

## 8. Structs

### 8.1 Declaration

Fields use `type name` order. Fields are separated by commas. Trailing
comma permitted.

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

All fields must be provided, or the struct is zero-initialized.

```forge
Point p  = Point { x: 10int32, y: 20int32 };
Point z  = Point {};                           // all fields zero
Point p2 = Point { y: 99int32, ..p };          // struct update syntax
```

### 8.3 Field Access

```forge
int32 px = p.x;
p.y = 20int32;    // COMPILE ERROR if p is immutable
```

### 8.4 Methods

```forge
struct PhysAddr {
    uptr value,
}

impl PhysAddr {
    // Pure method — takes PhysAddr by value
    bool page_aligned(PhysAddr self) {
        return self.value & (PAGE_SIZE - 1uptr) == 0uptr;
    }

    // Pure method — takes pointer, does not write global state
    PhysAddr align_down(PhysAddr self) {
        return PhysAddr { value: self.value & ~(PAGE_SIZE - 1uptr) };
    }

    // Procedure method — writes through self pointer
    @mut set(@mut PhysAddr* self, uptr addr) {
        self.value = addr;
    }
}

PhysAddr pa = PhysAddr { value: 0x1000uptr };
bool     ok = pa.page_aligned();
```

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

@pub @packed @align(8)
struct GDTDescriptor {
    uint16 size,
    uint64 offset,
}
```

---

## 9. Unions

Unions allocate one region of memory shared by all fields. Reading any field
is always defined — the result is the current bit pattern reinterpreted as
the requested type.

```forge
union FloatBits {
    float32 as_float,
    uint32  as_int,
}

@mut FloatBits fb = FloatBits { as_float: 1.0float32 };
uint32 bits = fb.as_int;   // reads float bit pattern as uint32 — defined
```

---

## 10. Enums

### 10.1 Simple Enums

```forge
enum Color: uint8 {
    Red   = 0uint8,
    Green = 1uint8,
    Blue  = 2uint8,
}

Color  c   = Color.Red;
uint8  raw = c as uint8;
```

The backing type must be specified. All values must fit within it.

### 10.2 Algebraic Enums (Tagged Unions)

Each variant may carry different data. The compiler generates the discriminant.

```forge
enum MemRegion {
    Free     { uptr base, usize size },
    Kernel   { uptr base, usize size, uint32 flags },
    MMIO     { uptr base, usize size, uint32 device_id },
    Reserved,
}
```

Match on algebraic enums is exhaustive. It is a compile error to access
variant fields without first matching.

```forge
@mut describe(MemRegion r) {
    match r {
        Free     { base, size }      => log("free: {} bytes at {:x}", size, base),
        Kernel   { base, flags, .. } => log("kernel at {:x}", base),
        MMIO     { device_id, .. }   => log("mmio device {}", device_id),
        Reserved                     => log("reserved"),
    }
}
```

---

## 11. Modules

### 11.1 Module Declaration

Each `.fg` file declares its module at the top. The name reflects the
directory structure.

```forge
module kernel.memory.paging;
```

There are no header files and no forward declarations. Each name is declared
exactly once.

### 11.2 Visibility

All declarations are private by default. `@pub` exposes a declaration to
importers.

```forge
module kernel.memory.frame;

// Private
uint32 internal_helper(usize frame) { ... }

// Public
@pub @mut alloc() { ... }
@pub @mut free(uptr phys_addr) { ... }
```

### 11.3 Importing

```forge
// Import an entire module — access via qualified name
import kernel.memory.paging;
kernel.memory.paging.map_page(virt, phys, flags);

// Import a specific function — no qualification needed at call site
import kernel.memory.paging.map_page();
import kernel.memory.paging.unmap_page();
map_page(virt, phys, flags);

// Import a specific type
import kernel.memory.frame.AllocError;
```

The syntax `import module.path.name()` with parentheses imports a specific
function. `import module.path.Name` without parentheses imports a type or
constant.

### 11.4 Module Resolution

The compiler maps dotted module names to file paths relative to the source
root. `kernel.memory.paging` resolves to `kernel/memory/paging.fg`.

`forge build` takes a root module and compiles all transitively imported
modules. Each module is compiled exactly once.

---

## 12. Attributes

Attributes modify the declaration immediately following them. They use `@`
syntax and always appear to the **left** of the declaration. Multiple
attributes on one declaration are **commutative** — order never matters
semantically.

```forge
@inline @naked never isr() { ... }
@naked @inline never isr() { ... }   // identical
```

### 12.1 Variable Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Variable may be modified after initialization |
| `@uninit` | Not zero-initialized — compiler tracks reads |
| `@const` | Applied to pointee — target is immutable |
| `@volatile` | Applied to pointee — accesses never optimized away |
| `@restrict` | Applied to pointee — no aliasing with other pointers in scope |
| `@section("name")` | Place in named ELF section |
| `@align(N)` | Minimum alignment; N must be a power of 2 |

### 12.2 Function / Procedure Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Marks a procedure — has side effects (global state, I/O) |
| `@pub` | Visible to other modules |
| `@inline` | Always inline at call site |
| `@noinline` | Never inline |
| `@naked` | No generated prologue, epilogue, or stack frame |
| `@cc("name")` | Set calling convention |
| `@interrupt` | Generate ISR-safe prologue/epilogue + `iretq` |
| `@cold` | Rarely executed — optimizer deprioritizes |
| `@section("name")` | Place function body in named ELF section |
| `@export("name")` | Export with exact symbol name |
| `@deprecated("msg")` | Warn at every call site |

### 12.3 Type Attributes

| Attribute | Applies To | Effect |
|-----------|-----------|--------|
| `@pub` | struct / enum / union | Visible to importers |
| `@packed` | struct | Remove all padding bytes |
| `@align(N)` | struct | Force minimum alignment |
| `@repr("C")` | struct / enum | C-compatible layout |
| `@repr("uint8")` etc. | enum | Set discriminant backing type |

---

## 13. Inline Assembly

Inline assembly is a first-class language feature. It is available only
inside procedures (`@mut` functions) — it is a side effect by definition.

### 13.1 Single-Statement Form

```forge
@mut enable_interrupts()  { asm("sti"); }
@mut disable_interrupts() { asm("cli"); }

never halt() {
    loop { asm("hlt"); }   // NOTE: pure infinite loop, hlt has no side effect
                            // on machine state visible to the program
}
```

### 13.2 Full Form with Constraints

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
@mut load_gdt(@const GDTDescriptor* desc) {
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

`@volatile` prevents the compiler from reordering or eliminating an `asm`
block:

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
| `"d"` | `rdx` / `edx` (I/O port) |
| `"D"` | `rdi` |
| `"S"` | `rsi` |
| `"r"` | any GPR |
| `"m"` | memory operand |
| `"i"` | immediate integer |
| `"="` | output (write-only) |
| `"+"` | output (read-write) |

Clobbers: `"memory"` — asm may access arbitrary memory. `"cc"` — asm
modifies the flags register.

---

## 14. Comptime

`comptime` evaluation runs entirely at compile time. It is typed and scoped.
It is not a text preprocessor.

### 14.1 Constexpr Blocks

```forge
comptime {
    constexpr PAGE_SHIFT: uint32 = 12uint32;
    constexpr PAGE_SIZE: usize   = 1usize << PAGE_SHIFT;
    constexpr PAGE_MASK: uptr    = ~(PAGE_SIZE as uptr - 1uptr);

    // Compile-time assertion — failure is a compiler error
    assert(PAGE_SIZE == 4096usize, "PAGE_SIZE must be 4096");
    assert(@sizeof(GDTEntry) == 8usize, "GDT entry must be 8 bytes");
}
```

### 14.2 Comptime If — Conditional Compilation

Replaces `#ifdef` entirely. Typed, scoped, no text substitution.

```forge
comptime if (TARGET_ARCH == .x86_64) {

    @mut flush_tlb(uptr vaddr) {
        asm("invlpg [rdi]" : : "D"(vaddr) : );
    }
    constexpr PHYS_ADDR_BITS: uint32 = 52uint32;

} else {
    @error("Forge only targets x86-64");
}
```

### 14.3 Comptime Functions

Evaluated at compile time when called with comptime-known arguments:

```forge
comptime uint64 mask_for_bits(uint32 bits) {
    return (1uint64 << bits) - 1uint64;
}

comptime usize kib(usize n) { return n * 1024usize; }
comptime usize mib(usize n) { return n * 1024usize * 1024usize; }

constexpr PHYS_MASK:  uint64 = mask_for_bits(52uint32);
constexpr HEAP_SIZE:  usize  = mib(16usize);
constexpr STACK_SIZE: usize  = kib(64usize);
```

### 14.4 Built-In Comptime Intrinsics

| Intrinsic | Result | Description |
|-----------|--------|-------------|
| `@sizeof(T)` | `usize` | Size of T in bytes |
| `@alignof(T)` | `usize` | Alignment of T in bytes |
| `@offsetof(T, field)` | `usize` | Byte offset of field in struct T |
| `@bitcast<T>(val)` | `T` | Reinterpret bits of val as type T |
| `@error("msg")` | — | Emit a compile error |
| `@warning("msg")` | — | Emit a compile warning |

---

## 15. OS and Hardware Primitives

These are language-level features — always available, no import required.

### 15.1 Port I/O

`port<T>` is the type of an x86 I/O port. `T` must be `uint8`, `uint16`,
or `uint32`. Port access is only permitted inside procedures (`@mut`).

```forge
port<uint8> pic_master_cmd  = 0x20uint16;
port<uint8> pic_master_data = 0x21uint16;
port<uint8> pic_slave_cmd   = 0xA0uint16;
port<uint8> pic_slave_data  = 0xA1uint16;

@mut send_eoi_master() {
    pic_master_cmd <- 0x20uint8;   // outb
}

@mut uint8 read_isr() {
    pic_master_cmd <- 0x0Buint8;   // select ISR register
    return -> pic_master_cmd;      // inb
}
```

### 15.2 Interrupt Service Routines

`@interrupt` generates ISR prologue (save all registers) and epilogue
(restore all registers + `iretq`).

```forge
@interrupt @mut isr_divide_error(@const InterruptFrame* frame) {
    panic("Divide by zero at rip={:x}", frame.rip);
}

// Vectors that push an error code — second parameter
@interrupt @mut isr_page_fault(@const InterruptFrame* frame, uint64 error_code) {
    uint64 cr2 = @cpu.read_cr2();
    handle_page_fault(cr2, error_code, frame);
}

// @naked — full manual control
@naked @mut isr_double_fault() {
    asm {
        call double_fault_handler
    .hang:
        hlt
        jmp .hang
    }
}

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
    memset(start as void*, 0int32, len);
}
```

### 15.4 CPU Register Access

```forge
// Control registers — pure reads, procedure writes
uint64 cr0 = @cpu.read_cr0();
uint64 cr2 = @cpu.read_cr2();
uint64 cr3 = @cpu.read_cr3();
uint64 cr4 = @cpu.read_cr4();

@mut @cpu.write_cr0(uint64 val);
@mut @cpu.write_cr3(uptr pml4_phys);
@mut @cpu.write_cr4(uint64 val);

// MSRs
uint64 efer = @cpu.rdmsr(0xC000_0080uint32);
@mut @cpu.wrmsr(0xC000_0080uint32, efer | (1uint64 << 8uint64));

// CPUID
struct CpuidResult { uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, }
CpuidResult info = @cpu.cpuid(1uint32, 0uint32);
bool has_sse2    = (info.edx >> 26uint32) & 1uint32 == 1uint32;

// Interrupt flag
@mut @cpu.enable_interrupts();
@mut @cpu.disable_interrupts();
```

### 15.5 Atomic Operations

`atomic<T>` operations require explicit memory ordering. There is no implicit
synchronization anywhere else in the language.

```forge
@mut atomic<uint64> counter;   // zero-initialized

// All atomic operations are procedures — they have side effects
uint64 old = counter.fetch_add(1uint64, .SeqCst);
uint64 old = counter.fetch_sub(1uint64, .Relaxed);
uint64 old = counter.swap(0uint64, .AcqRel);
bool   ok  = counter.compare_exchange(0uint64, 1uint64, .SeqCst, .Relaxed);
uint64 val = counter.load(.Acquire);
counter.store(42uint64, .Release);
```

**Memory orderings:**

| Ordering | Meaning |
|---------|---------|
| `.Relaxed` | Atomicity only — no ordering guarantees |
| `.Acquire` | Sees all stores before the matching Release |
| `.Release` | All stores before this visible to matching Acquire |
| `.AcqRel` | Both Acquire and Release |
| `.SeqCst` | Total sequential consistency |

---

## 16. Memory Model

### 16.1 Execution Model

Forge targets a single-threaded execution model at the language level. There
are no thread primitives. Concurrency is expressed through atomic operations,
memory-mapped registers, and interrupt handlers. This matches the reality of
kernel programming.

### 16.2 Address Space

The compiler generates code for a flat 64-bit virtual address space. Physical
address management is the programmer's responsibility. `uptr` holds raw
addresses. `@volatile` handles MMIO. The language has no concept of physical
vs. virtual addresses at the type level.

### 16.3 Stack

The stack grows downward on x86-64. Stack frames follow the System V AMD64
ABI. The stack pointer is 16-byte aligned before every `call`. `@naked`
functions have no generated frame — the programmer maintains all invariants.

### 16.4 Object Lifetime

A local variable's lifetime is the enclosing block. Taking a pointer to a
local and using it after the variable's lifetime is a compile error where
statically detectable, and incorrect behaviour otherwise.

---

## 17. Undefined Behaviour

**Forge has no undefined behaviour.**

Every operation in this specification has a defined result. If the
specification does not define the result of an operation, that is a defect
in the specification — not permission for the compiler to do anything.

| Operation | C behaviour | Forge behaviour |
|-----------|-------------|-----------------|
| Signed integer overflow | Undefined | Wraps (two's complement) |
| Unsigned integer overflow | Wraps | Wraps |
| Division by zero | Undefined | Traps |
| Null pointer dereference | Undefined | Cannot occur — no null pointers |
| Out-of-bounds array access | Undefined | Traps |
| Uninitialized read | Undefined | Compile error (with `@uninit`) |
| Signed shift by ≥ width | Undefined | Result is 0 |
| Reading wrong union field | Undefined | Returns current bit pattern |
| Pointer aliasing violation | Undefined | No strict aliasing rule in Forge |

---

## 18. ABI and Calling Conventions

### 18.1 Default — System V AMD64

**Integer/pointer arguments:** `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.
Additional arguments go on the stack.

**Float arguments:** `xmm0`–`xmm7`.

**Return values:** integers/pointers ≤ 64 bits in `rax`; 65–128 bits in
`rax:rdx`; floats in `xmm0`.

**Caller-saved:** `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`–`r11`,
`xmm0`–`xmm15`.

**Callee-saved:** `rbx`, `rbp`, `r12`–`r15`.

**Stack alignment:** 16-byte aligned before `call`.

### 18.2 Struct Passing

Structs ≤ 16 bytes: passed in registers per AMD64 classification.
Structs > 16 bytes: passed on the stack by value (caller copies).

### 18.3 Interrupt Frame Layout

On interrupt entry the CPU pushes (high to low address):

```
ss
rsp
rflags
cs
rip
[error_code]   -- only for vectors 8, 10–14, 17, 21, 29, 30
```

`@interrupt` handlers receive `*InterruptFrame` and optionally `uint64`
error code as parameters.

---

## 19. Object Files and Linking

### 19.1 Output Format

`forgec` produces ELF64 object files (`.o`). Linking is performed by the
system `ld` or `lld`. The compiler does not include a linker.

### 19.2 Sections

| Section | Contents |
|---------|----------|
| `.text` | Compiled function bodies |
| `.rodata` | String literals, comptime constants with addresses |
| `.data` | Initialized mutable globals |
| `.bss` | Zero-initialized globals |
| `.note.forge` | Compiler version and build metadata |

Additional sections are created by `@section("name")`.

### 19.3 Symbol Visibility

All symbols are local by default. `@pub` makes a symbol globally visible.
`@export("name")` sets an exact symbol name, bypassing name mangling.

### 19.4 Name Mangling

Forge mangles symbol names to encode the module path and parameter types.
The mangling scheme is not stable across compiler versions. Use
`@export("name")` for stable ABI entry points.

---

## 20. Compiler Attributes Reference

All attributes: `@name` or `@name(args)`. Always left of the declaration.
Always commutative.

### 20.1 Variable Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Mutable after initialization |
| `@uninit` | Not zero-initialized; read tracking enabled |
| `@const` | Pointee is immutable |
| `@volatile` | Pointee accesses never optimized away |
| `@restrict` | Pointee not aliased by any other pointer in scope |
| `@section("name")` | ELF section placement |
| `@align(N)` | Minimum alignment in bytes |

### 20.2 Function Attributes

| Attribute | Effect |
|-----------|--------|
| `@mut` | Procedure — has side effects |
| `@pub` | Visible to other modules |
| `@inline` | Always inline |
| `@noinline` | Never inline |
| `@naked` | No prologue, epilogue, or stack frame |
| `@cc("name")` | Override calling convention |
| `@interrupt` | ISR prologue/epilogue + `iretq` |
| `@cold` | Rarely-executed path |
| `@section("name")` | ELF section placement |
| `@export("name")` | Exact symbol name |
| `@deprecated("msg")` | Warn at call sites |

### 20.3 Type Attributes

| Attribute | Applies To | Effect |
|-----------|-----------|--------|
| `@pub` | struct / enum / union | Visible to importers |
| `@packed` | struct | No padding |
| `@align(N)` | struct | Minimum alignment |
| `@repr("C")` | struct / enum | C-compatible layout |
| `@repr("uint8")` etc. | enum | Discriminant backing type |

---

## 21. What Forge Deliberately Omits

| Feature | Reason |
|---------|--------|
| **Garbage collector** | Unpredictable latency; incompatible with interrupt context |
| **Exceptions** | Hidden control flow — violates §1.2 |
| **Type inference** | Hides the type from the reader |
| **`fn` keyword** | Redundant — the return type is sufficient |
| **`void` keyword** | Procedures have no return type — `@mut` is sufficient |
| **`pub` keyword** | Visibility is an attribute — `@pub` is consistent |
| **`use` keyword** | `import module.path.name()` is explicit and unambiguous |
| **`const` keyword** | Replaced by `constexpr` — unambiguous compile-time evaluation |
| **`i8`/`u8` etc.** | Replaced by `int8`/`uint8` — more readable |
| **Generics** | To be designed in a future version |
| **Pattern matching** | To be designed in a future version |
| **Error handling** | To be designed in a future version |
| **Interfaces / traits** | To be designed in a future version |
| **Implicit integer promotion** | Source of a significant fraction of C CVEs |
| **Null pointers** | Use `Option<*T>`. `NULL` does not exist. |
| **Undefined behaviour** | See §17. Every operation is defined. |
| **Text preprocessor** | `comptime if` replaces `#ifdef`. Modules replace `#include`. |
| **Header files** | Modules with `@pub` replace this entirely |
| **Global allocator** | The kernel defines its memory model |
| **Hidden copies** | Large types passed by pointer; copies are explicit |
| **Implicit conversions** | Every type change requires `as` |
| **Variadic functions (`...`)** | Use slices |
| **`goto`** | Use `loop { break; }` or labeled breaks |
| **Operator overloading** | `+` always means addition |
| **Dynamic dispatch / vtables** | No `dyn`, no virtual |
| **RTTI** | No runtime type information |

---

## 22. Appendix A — Kernel Entry Point Example

```forge
module kernel.boot;

import kernel.memory.paging;
import kernel.memory.frame;
import kernel.cpu.gdt;
import kernel.cpu.idt;
import kernel.drivers.uart;

/// Kernel entry point — called by the Multiboot2 bootloader.
/// multiboot_magic must equal 0x36D76289.
/// multiboot_info is the physical address of the info structure.
@pub @section(".boot.text") @export("kernel_main")
@mut kernel_main(uint32 multiboot_magic, uptr multiboot_info) {
    gdt.init();
    idt.init();

    uart.init(0x3F8uint16, 115200uint32);
    uart.write("Forge kernel v1.0\n");

    if multiboot_magic != 0x36D7_6289uint32 {
        panic("Invalid Multiboot2 magic: {:x}", multiboot_magic);
    }

    frame.init(multiboot_info);
    paging.init();

    uart.write("Memory OK\n");
    kernel_loop();
}

@mut kernel_loop() {
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

/// Bitmap allocator. One bit per 4 KiB frame.
/// Supports up to 4 GiB of physical memory (1M frames).
constexpr MAX_FRAMES: usize = 1_048_576usize;

@uninit @mut [uint8; MAX_FRAMES / 8usize] bitmap;
@mut usize total_frames = 0usize;
@mut usize free_frames  = 0usize;

/// Initialize from a Multiboot2 memory map.
/// All frames start as used (0xFF). Usable regions are freed.
@pub @mut init(uptr multiboot_info) {
    for i in 0usize..bitmap.len {
        bitmap[i] = 0xFFuint8;
    }

    MemoryMap* map = multiboot2_get_memory_map(multiboot_info);

    for i in 0usize..map.entry_count {
        MemoryMapEntry entry = map.entries[i];
        if entry.kind == MemKind.Usable {
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

/// Allocate one physical frame. Returns its physical address.
/// Returns 0 on failure — caller must check.
/// Note: proper error handling to be added in a future version.
@pub @mut uptr alloc() {
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
    return 0uptr;   // out of memory
}

/// Free a physical frame by its physical address.
@pub @mut free(uptr phys_addr) {
    usize frame = phys_addr as usize / PAGE_SIZE;
    usize byte  = frame / 8usize;
    uint8 bit   = (frame % 8usize) as uint8;
    bitmap[byte] &= ~(1uint8 << bit);
    free_frames += 1usize;
}

/// Number of free frames available.
@pub usize available() {
    return free_frames;
}

/// Total usable frames discovered at init.
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

## 24. Appendix C — Compile-Time Guarantees

A conforming `forgec` implementation guarantees:

**Type safety.** No program that compiles without errors can:
- Read a value as a different type without an explicit `as` or `@bitcast`
- Access a struct field that does not exist
- Call a function with incorrect argument types

**Purity.** No pure function that compiles without errors can:
- Write to global mutable state
- Perform port I/O or MMIO writes
- Call a procedure (`@mut` function)
- Execute inline assembly

**No silent integer conversions.** Every integer type change corresponds
to an explicit `as` in the source.

**No hidden control flow.** Every execution path through a function is
visible in the source. No destructors with side effects, no exceptions.

**Exhaustive matching.** Every `match` on an algebraic enum covers all
variants. Adding a new variant is a compile error in all files that match
on that enum.

**Defined overflow.** Integer overflow wraps in two's complement. The
compiler does not use overflow as an optimization assumption.

**Uninitialized reads.** The compiler tracks reads of `@uninit` variables
and rejects programs where a variable may be read before being written.

---

## 25. Appendix D — Compiler Runtime Exception

The Forge compiler (`forgec`) is licensed under the GNU General Public
License v3.0. However, as a special exception:

> You may compile programs using `forgec` and distribute those programs
> under any license of your choosing, without the compiled output being
> considered a derivative work of `forgec` or subject to the terms of
> the GPL.
>
> This exception does not apply to modifications of `forgec` itself.
> Any modified version of `forgec` that you distribute must be licensed
> under the GPLv3.

This exception follows the precedent established by the GCC Runtime Library
Exception. It ensures that using Forge to build software does not impose any
licensing requirement on that software.

---

*The Forge Programming Language Specification, Version 1.0*
*GNU General Public License v3.0*
*Forge — C without the footguns. Built for operating systems.*
