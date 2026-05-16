# The Forge Programming Language Specification

**Version:** 1.4
**Status:** Normative Draft
**License:** GNU General Public License v3.0
**Compiler:** `forgec` (bootstrap implementation in C99)
**File Extension:** `.fg`
**Target:** x86-64, freestanding (no libc, no OS assumed)

---

## Changelog

| Version | Changes |
|---------|---------|
| 1.4 | Restricted multiple return values to a maximum of 16 bytes to ensure guaranteed register-level ABI allocation (§20.2). Simplified closures to be strictly stateless shorthand expressions to eliminate fat-pointer/trampoline complexity (§6.9). Restricted `@uninit` tracking to coarse identifier-level scopes to avoid dependent type-checking requirements (§5.5). Prohibited volatile pointer reads/writes within pure functions to eliminate transitive global side effects (§1.4). Defined inline assembly constraints for direct ELF emission (§15). |
| 1.3 | Removed generics entirely — deferred to a future version. Replaced `Result<T,E>`, `?`, and `catch` with multiple return values. Ignoring an error return is a compile error. `ErrorCode` is a standard convention. All examples updated. `RingBuffer` example removed. |
| 1.2 | Removed implicit function return. Replaced `never` / `->` with `@noreturn`. Literal suffixes optional for locals. Port read unified to `.read()`. Import syntax standardized. `panic()` and `memset()` moved to standard library. Purity of `defer` documented. §8.5 prose filled in. All audit fixes applied. |
| 1.1 | Removed `fn` keyword. Replaced `u8`/`i8` with `uint8`/`int8`. Procedures declared with `@mut`. Purity by default. |
| 1.0 | Initial normative draft. |

---

## Table of Contents

1. [Design Principles](#1-design-principles)
2. [Standard Library Relationship](#2-standard-library-relationship)
3. [Lexical Structure](#3-lexical-structure)
4. [Type System](#4-type-system)
5. [Variables and Constants](#5-variables-and-constants)
6. [Functions and Procedures](#6-functions-and-procedures)
7. [Error Handling](#7-error-handling)
8. [Control Flow](#8-control-flow)
9. [Pointers and Memory](#9-pointers-and-memory)
10. [Structs](#10-structs)
11. [Unions](#11-unions)
12. [Enums](#12-enums)
13. [Modules](#13-modules)
14. [Attributes](#14-attributes)
15. [Inline Assembly](#15-inline-assembly)
16. [Comptime](#16-comptime)
17. [OS and Hardware Primitives](#17-os-and-hardware-primitives)
18. [Memory Model](#18-memory-model)
19. [Undefined Behaviour](#19-undefined-behaviour)
20. [ABI and Calling Conventions](#20-abi-and-calling-conventions)
21. [Object Files and Linking](#21-object-files-and-linking)
22. [Compiler Attributes Reference](#22-compiler-attributes-reference)
23. [What Forge Deliberately Omits](#23-what-forge-deliberately-omits)
24. [Appendix A — Kernel Entry Point Example](#24-appendix-a--kernel-entry-point-example)
25. [Appendix B — Physical Frame Allocator Example](#25-appendix-b--physical-frame-allocator-example)
26. [Appendix C — Compile-Time Guarantees](#26-appendix-c--compile-time-guarantees)

---

## 1. Design Principles

These principles are ordered by priority. When two principles conflict,
the higher-numbered one yields to the lower-numbered one.

### 1.1 No Undefined Behaviour

Every operation in Forge has a defined result. The compiler never uses
the absence of a definition as an optimization license. There is no
concept of undefined behaviour in this specification.

Signed integer overflow wraps by default. Division by zero traps.
Out-of-bounds access traps. Uninitialized reads are a compile error
unless explicitly opted out of with `@uninit`. Null pointer
dereferences cannot occur because null pointers do not exist as plain
values.

### 1.2 Explicit Over Implicit

If an operation costs cycles, allocates memory, changes a type, copies
a value, or has a side effect — it must be written explicitly in the
source.

- No type inference on expressions
- No implicit integer promotion
- No implicit conversions between any types
- No hidden copies of values
- No hidden allocations
- No hidden control flow
- No implicit return — every function exit must be an explicit `return`
- No ignored error returns — discarding the error component of a
  multiple return value is a compile error

The one deliberate exception: integer literal suffixes are optional
when declaring a local variable, because the type is already written
explicitly on the left. See §5.3.

### 1.3 Zero-Cost Abstractions

Every abstraction in Forge compiles to machine code identical to what
a competent programmer would write by hand in C. If an abstraction
cannot be zero-cost, it is not in the language.

### 1.4 Purity by Default

All functions in Forge are **pure by default**. A pure function may
not write to global variables, perform I/O, or call procedures. The
compiler enforces this statically.

A function that has side effects must be declared with `@mut`. These
are called **procedures**. Writing through pointer parameters is
permitted in pure functions because the effect is explicit in the
signature. However, to eliminate transitive state mutations, pure functions 
are strictly forbidden from reading or writing to `@volatile` memory locations 
or dereferencing pointers that expose mutable global structures.

### 1.5 Freestanding First

Forge does not assume the existence of an operating system, a C
runtime, a heap, or a standard library. Every language feature works
at interrupt level, in a bootloader, or on bare metal with no prior
initialization.

### 1.6 Predictable Code Generation

The programmer should be able to predict the assembly output of any
Forge function without running the compiler. There is no hidden virtual
dispatch, no hidden state machine generation, and no compiler
transformations that change observable behaviour beyond reordering and
eliminating redundant pure operations.

---

## 2. Standard Library Relationship

Forge has no implicit standard library. Nothing is available without
an explicit import. The Forge standard library (`libforge`) is an
optional collection of modules imported as needed. It is not linked
by default.

`libforge` is written in Forge. It assumes no OS, no libc, and no
heap unless the programmer provides one.

### 2.1 Key Standard Library Modules

| Module | Purpose |
|--------|---------|
| `forge.panic` | Kernel panic support |
| `forge.mem` | `memset`, `memcpy`, `memcmp` |
| `forge.fmt` | Formatted output (requires a writer procedure) |
| `forge.io` | Serial and port I/O helpers |

### 2.2 `panic()`

`panic()` is provided by `forge.panic`. It is not a language built-in.

```forge
import forge.panic.{panic()};

```

Declaration:

```forge
@pub @noreturn @mut panic([]uint8 msg) {
    // writes msg to serial port 0x3F8, then halts all CPUs
}

```

On bare metal the default implementation writes to the first serial
port and executes `hlt` in a loop. The programmer may replace it by
providing their own implementation before linking.

### 2.3 `memset()` and `memcpy()`

Provided by `forge.mem`. Not language built-ins.

```forge
import forge.mem.{memset(), memcpy()};

```

Declarations:

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

The compiler may replace calls to these with optimized inline
sequences when it can prove the semantics are preserved.

---

## 3. Lexical Structure

### 3.1 Source Files

Forge source files are UTF-8 encoded text with the `.fg` extension.
Line endings may be `LF` or `CRLF`. The compiler normalizes all line
endings to `LF` before processing.

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
to the parser and may be used as identifiers, though doing so is
strongly discouraged:

```
mut     uninit

```

### 3.4 Identifiers

```
identifier ::= [a-zA-Z_][a-zA-Z0-9_]*

```

Identifiers are case-sensitive. Identifiers beginning with two
underscores (`__`) are reserved for the linker and platform ABI.

### 3.5 Integer Literals

Integer literal suffixes are required in all contexts except local
variable declarations, where the type is inferred from the declared
type. Underscores may appear anywhere in a digit sequence.

```forge
// Local variable declarations — suffix optional
uint32 a = 1_000_000;
uint32 b = 0xFF_EC_00_01;     // hex
uint8  c = 0b1010_1100;       // binary
uint16 d = 0o755;             // octal
int32  e = -1;                // negative

// All other contexts — suffix required
uint32 f = a + 1uint32;
uint8  g = arr[2uint8];

// Explicit suffixes always accepted everywhere
uint32 h = 42uint32;
uptr   i = 0xFFFF_FFFF_8000_0000uptr;

```

Valid suffixes: `uint8` `uint16` `uint32` `uint64` `uint128`
`int8` `int16` `int32` `int64` `int128` `usize` `isize` `uptr`

A literal whose value does not fit the declared type is a compile
error even when the suffix is omitted:

```forge
uint8 x = 256;    // COMPILE ERROR — 256 does not fit uint8
uint32 y = -1;    // COMPILE ERROR — -1 is not valid for uint32

```

### 3.6 Float Literals

Float literals require a decimal point. In local variable declarations
the suffix may be omitted when the declared type is unambiguous.

```forge
float32 a = 3.14;              // suffix inferred
float64 b = 2.718_281_828;
float32 c = 42.0float32;       // explicit suffix — always valid

```

Valid suffixes: `float32` `float64`

### 3.7 Boolean Literals

`true` and `false` are keywords and the only values of type `bool`.
`bool` is not an integer. There is no implicit conversion.

### 3.8 Character Literals

A character literal produces a `uint32` containing the Unicode
codepoint of the character.

```forge
uint32 a = 'A';           // 65
uint32 b = '\n';          // 10
uint32 c = '\t';          // 9
uint32 d = '\\';          // 92
uint32 e = '\u{1F600}';   // Unicode codepoint
uint32 f = '\x41';        // hex byte — 65

```

### 3.9 String Literals

A string literal produces a `[]uint8`. UTF-8, no null terminator.
Length available via `.len`. Stored in `.rodata`. Immutable.

```forge
[]uint8 a = "hello, kernel";
[]uint8 b = "line one\nline two";
[]uint8 c = r"raw string — no \n processing";

```

### 3.10 Operators

```
Arithmetic:    +   -   * /   %
Bitwise:       &   |   ^   ~   <<   >>
Comparison:    ==  !=  <   >   <=   >=
Logical:       and   or   not
Assignment:    =   +=  -=  *=  /=  &=  |=  ^=  <<=  >>=
Range:         ..   ..=
Pointer:       * (dereference)    &  (address-of)
Cast:          as
Port write:    <-
Member:        .
Index:         [ ]

```

Operator precedence follows C conventions. `not` binds tighter than
`and` and `or`. There is no comma operator.

---

## 4. Type System

Forge is statically typed. Every expression has a type known at
compile time. There is no type inference on expressions. All
declarations require an explicit type. Every type change must be
written explicitly with `as`.

### 4.1 Integer Types

| Type | Width | Use |
| --- | --- | --- |
| `uint8` | 8 bits | byte, port value |
| `uint16` | 16 bits | port address |
| `uint32` | 32 bits | general unsigned integer |
| `uint64` | 64 bits | general unsigned integer |
| `uint128` | 128 bits | wide unsigned integer |
| `int8` | 8 bits | signed byte |
| `int16` | 16 bits | signed short |
| `int32` | 32 bits | signed integer |
| `int64` | 64 bits | signed long |
| `int128` | 128 bits | signed wide integer |
| `usize` | 64 bits (x86-64) | lengths, indices |
| `isize` | 64 bits (x86-64) | signed size |
| `uptr` | 64 bits (x86-64) | raw hardware addresses |

`usize` is for array lengths and indices. `uptr` is for hardware
addresses. They are distinct types with no implicit conversion.

**Integer overflow** is defined. Both signed and unsigned integers
wrap on overflow by default (two's complement). Explicit behaviour:

```forge
uint32 a = x.wrapping_add(y);    // defined wrap
uint32 b = x.saturating_add(y);  // clamps to maximum

```

### 4.2 Float Types

| Type | Width | Standard |
| --- | --- | --- |
| `float32` | 32 bits | IEEE 754 single precision |
| `float64` | 64 bits | IEEE 754 double precision |

Float operations follow IEEE 754 strictly.

### 4.3 Boolean Type

`bool` is 1 byte. Values: `true`, `false`. Not an integer.
No implicit conversion to or from any integer type.

### 4.4 The Absence of `void`

There is no `void` keyword. A function that returns no value is a
procedure, declared with `@mut` and no return type. See §6.

### 4.5 The `@noreturn` Attribute

Functions or procedures that never return are annotated `@noreturn`.
This is an attribute — it does not change the declaration syntax.

```forge
@noreturn @mut halt() {
    loop { asm("hlt"); }
}

```

The compiler uses `@noreturn` to mark unreachable code after such
calls and to allow them to satisfy any type in a branch. A `@noreturn`
function with a reachable `return` is a compile error.

### 4.6 Pointer Types

```forge
uint32* // raw pointer to mutable uint32
@const uint32* // raw pointer to immutable uint32
@volatile uint32* // MMIO pointer — accesses never optimized away

```

Pointers are always non-null. A nullable pointer is future work.
`NULL` does not exist as a value in Forge.

### 4.7 Slice Types

A slice is a fat pointer: `(ptr: *T, len: usize)`. All accesses are
bounds-checked — at compile time where possible, trapping at runtime
otherwise.

```forge
[]uint8         // mutable slice
[]@const uint8  // immutable slice

```

Fields: `.ptr` — raw pointer. `.len` — element count.

### 4.8 Array Types

Fixed-size, stack-allocated. Size must be a `constexpr`.

```forge
[uint8; 512]   buf;
[uint32; 1024] table;
[GDTEntry; 8]  gdt;

```

### 4.9 Tuple Types

```forge
(uint32, bool, uint8) t  = (42, true, 0xFF);
uint32                lo = t.0;
bool                  b  = t.1;
uint8                 hi = t.2;

```

Tuples are also used as multiple return values. See §7.

### 4.10 Function Pointer Types

```forge
(uint32, uint32) -> uint32    // pure function pointer
@mut (uint32) -> uint32       // procedure pointer with return value
@mut ()                       // procedure pointer, no return

```

Function pointer types represent entirely stateless execution branch addresses.
Closures that maintain or attempt to encapsulate outer lexical state blocks
are not allowed in the type system, ensuring execution addresses map 1:1 to
native structural text references.

### 4.11 Bitfield Integers

Valid only inside `@packed` or `@repr("C")` struct declarations.
Syntax: `uintN` or `intN` where N is the bit width (1–64).

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

### 4.12 Atomic Types

`atomic<T>` provides indivisible operations with explicit memory
ordering. See §17.5.

### 4.13 Port Type

`port<T>` represents an x86 I/O port. `T` must be `uint8`,
`uint16`, or `uint32`. See §17.1.

### 4.14 Type Casting

All conversions are explicit using `as`. Every cast has a defined
result — no undefined behaviour.

```forge
uint32  a = 0xFFFFFFFFuint32;
uint8   b = a as uint8;            // truncates — 0xFF
int32   c = a as int32;            // reinterpret bits — -1
uint64  d = a as uint64;           // zero-extends
float32 e = 42uint32 as float32;   // converts — 42.0
uptr    f = ptr as uptr;           // pointer to integer
uint32* g = f as uint32*;          // integer to pointer

```

---

## 5. Variables and Constants

### 5.1 Declaration Syntax

Type first, then name — identical to C. No type inference.

```
[@attributes] type name [= expression] ;

```

```forge
uint32      x;        // immutable, zero-initialized
uint32* ptr;      // immutable pointer, zero-initialized
[uint8;512] buf;      // immutable array, all bytes zero
GDTEntry    entry;    // immutable struct, all fields zero

```

### 5.2 Mutability — `@mut`

All variables are immutable by default. `@mut` makes a variable
mutable after initialization.

```forge
uint32 x = 10;
x = 20;              // COMPILE ERROR: x is immutable

@mut uint32 y = 10;
y = 20;              // OK
y += 5;              // OK — suffix inferred in local context

```

### 5.3 Integer Literal Suffix Rules

**Local variable declarations** — suffix optional when the declared
type makes the literal's type unambiguous. The compiler infers the
literal type from the declared type, not from the value.

**All other contexts** — suffix required. This includes expressions,
function call arguments, struct field initializers, array sizes,
`constexpr` declarations, and `comptime` blocks.

A literal that does not fit the declared type is always a compile
error regardless of whether a suffix is present.

### 5.4 Zero Initialization

All variables are zero-initialized by default:

* Integer types → `0`
* Float types → `0.0`
* Boolean → `false`
* Pointer types → all-zero bits
* Array types → all elements zeroed
* Struct types → all fields zeroed
* Slice types → `.ptr` zeroed, `.len` = 0

The compiler emits zero initialization explicitly. It does not rely
on BSS zeroing for stack variables.

### 5.5 Uninitialized Variables — `@uninit`

`@uninit` opts out of zero initialization. Definite assignment analysis
is strictly performed at the **identifier level**, rather than individual
structural sub-fields or dynamic element indices. Reading any member of an
aggregate structure or array before the entire aggregate identifier is assigned
or fully overwritten as a structural block is a compile error.

```forge
@uninit uint32        x;
@uninit [uint8; 4096] page_buf;

// Aggregates must be fully written before reading elements
for i in 0..page_buf.len { page_buf[i] = 0uint8; } 

uint8 v = page_buf[0usize]; // OK — aggregate identifier is fully assigned

@uninit uint32 z;
uint32 w = z;          // COMPILE ERROR: z is completely uninitialized

```

### 5.6 Attribute Composition

All attributes are commutative. Order never matters.

```forge
@mut @uninit uint32 x;    // mutable, uninitialized
@uninit @mut uint32 x;    // identical

```

### 5.7 Compile-Time Constants — `constexpr`

Always immutable. Always evaluated at compile time. Type suffix
required. Cannot be passed by pointer.

```forge
constexpr PAGE_SIZE:   usize  = 4096usize;
constexpr PAGE_SHIFT:  uint32 = 12uint32;
constexpr KERNEL_BASE: uptr   = 0xFFFF_FFFF_8000_0000uptr;
constexpr MAX_CPUS:    uint32 = 256uint32;
constexpr PAGE_MASK:   uptr   = ~(PAGE_SIZE as uptr - 1uptr);

```

### 5.8 Variable Scope

Variables are scoped to the enclosing block. An inner declaration of
the same name shadows the outer one. Shadowing produces a warning.

---

## 6. Functions and Procedures

Forge distinguishes two kinds of callable units:

* **Functions** — pure by default. May not write to global state,
perform I/O, or call procedures. Writing through pointer parameters
is allowed.
* **Procedures** — declared with `@mut`. May have arbitrary side
effects. Can write globals, perform port I/O, and call other
procedures.

The compiler enforces purity statically. Calling a procedure from a
pure function is a compile error.

### 6.1 Function Declaration

Return type first, then name, then parameter list. Parameters use
`type name` order.

```
[@attributes] return_type name ( [parameters] ) block

```

```forge
uint32 add(uint32 a, uint32 b) {
    return a + b;
}

uint8 checksum([]uint8 data) {
    @mut uint8 sum = 0uint8;
    for byte in data {
        sum ^= byte;
    }
    return sum;
}

```

Every function must have an explicit `return` on every reachable exit
path. A function that may reach its end without a `return` is a
compile error.

### 6.2 Procedure Declaration

No return type. `@mut` is required.

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

A procedure may use a bare `return;` to exit early, or simply reach
the end of the block.

### 6.3 Multiple Return Values

Functions and procedures may return multiple values using tuple syntax.
This is the primary mechanism for error handling. To eliminate stack-spill
complexity within calling conventions, the combined layout size of all
multiple return values must never exceed 16 bytes. See §20.2.

```forge
// Pure function returning two values (4 + 1 <= 16 bytes)
(uint32, bool) divide(uint32 a, uint32 b) {
    if b == 0uint32 {
        return (0uint32, false);
    }
    return (a / b, true);
}

// Procedure returning a value and an error code (8 + 4 <= 16 bytes)
@mut (uptr, ErrorCode) alloc_page() {
    if free_frames == 0usize {
        return (0uptr, ErrorCode.OutOfMemory);
    }
    // ... allocate
    return (addr, ErrorCode.Ok);
}

```

### 6.4 Noreturn

`@noreturn` marks a function or procedure that never returns.

```forge
@noreturn @mut halt() {
    @cpu.disable_interrupts();
    loop { asm("hlt"); }
}

```

### 6.5 Purity Rules

A pure function **may**:

* Read global variables and `constexpr` constants
* Write through pointer parameters
* Call other pure functions
* Execute non-volatile inline assembly with no memory side effects
* Use `defer` to call pure functions

A pure function **may not**:

* Write to global variables
* Perform port I/O
* Call procedures
* Execute `@volatile` assembly or interact with `@volatile` pointers
* Use `defer` or `defer_err` to call procedures

Violation is a compile error.

### 6.6 Return Statement

`return` is required on every exit path of a function. There is no
implicit return. The body reaching its end without `return` is a
compile error.

```forge
uint32 abs(int32 x) {
    if x < 0int32 {
        return (-x) as uint32;
    }
    return x as uint32;
}

```

If-expressions produce a value without `return` — they are
expressions, not statements. This is the only exception to the
explicit return rule. See §8.1.

### 6.7 Mutable Parameters

Parameters are immutable by default. `@mut` allows mutation of the
local copy inside the body. This does not affect the caller.

```forge
uint32 clamp(@mut uint32 x, uint32 lo, uint32 hi) {
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

### 6.8 First-Class Functions

```forge
(uint32, uint32) -> uint32 fn_ptr = add;
uint32 result = fn_ptr(1uint32, 2uint32);

@mut () proc_ptr = setup_gdt;
proc_ptr();

```

### 6.9 Closures

Closures use `|parameters| { body }` syntax and are strictly zero-overhead,
stateless structural representations. To completely circumvent fat-pointer
allocation patterns, stack alignment dependencies, or runtime trampoline requirements
on bare metal, closures are entirely prohibited from capturing environment variables
from outer scopes. They are pure inline syntactical shorthands for local functions and
can be seamlessly passed directly to plain function pointer bindings (§4.10).

```forge
// Stateless shorthand mapping is valid and maps to plain function pointers
(uint32) -> uint32 double = |uint32 x| { return x * 2uint32; };

```

### 6.10 Calling Conventions

Default is System V AMD64 ABI.

```forge
@cc("cdecl")   int32 cdecl_fn(int32 x) { return x; }
@cc("stdcall") int32 stdcall_fn(int32 x) { return x; }
@cc("sysv")    int32 sysv_fn(int32 x) { return x; }
@cc("win64")   int32 win64_fn(int32 x) { return x; }

```

### 6.11 Extern Declarations

```forge
extern uint32 asm_helper(uint32 a, uint32 b);

@cc("cdecl")
extern int32 c_compat_fn(int32 x);

```

### 6.12 Visibility — `@pub`

All declarations are private to their module by default.

```forge
uint32 internal(uptr addr) { return addr as uint32 & 0xFFFuint32; }

@pub uint32 page_offset(uptr addr) { return internal(addr); }
@pub @mut init_paging() { ... }

```

---

## 7. Error Handling

Forge has no exceptions and no hidden error propagation. Errors are
plain values returned alongside normal return values using multiple
return values. Every error must be explicitly handled at the call site.

### 7.1 ErrorCode Convention

`ErrorCode` is a simple enum defined by the programmer for each module
or subsystem. There is no global error type imposed by the language.
A common definition:

```forge
enum ErrorCode: uint32 {
    Ok              = 0,
    OutOfMemory     = 1,
    InvalidArgument = 2,
    NotFound        = 3,
    PermissionDenied = 4,
    DeviceBusy      = 5,
    // ... extend as needed
}

```

Each module defines the error codes it can produce. Using a `uint32`
backing type means error codes can be passed through the ABI with no
overhead.

### 7.2 Returning Errors

A function or procedure that can fail returns its result value and an
`ErrorCode` as a tuple. By convention the error code is always the
last return value.

```forge
// Pure function that can fail (8 + 4 <= 16 bytes)
(uptr, ErrorCode) find_free_frame() {
    if free_frames == 0usize {
        return (0uptr, ErrorCode.OutOfMemory);
    }
    return (frame_addr, ErrorCode.Ok);
}

// Procedure that can fail and returns a value (4 + 4 <= 16 bytes)
@mut (uint32, ErrorCode) read_register(uint16 port) {
    if not port_valid(port) {
        return (0uint32, ErrorCode.InvalidArgument);
    }
    return (port.read() as uint32, ErrorCode.Ok);
}

// Procedure that can fail but has no meaningful return value
// Returns only an ErrorCode (4 <= 16 bytes)
@mut ErrorCode init_paging() {
    if not cpu_has_pae() {
        return ErrorCode.DeviceBusy;
    }
    // ... set up paging
    return ErrorCode.Ok;
}

```

### 7.3 Handling Errors

Every return value from a function must be bound. Discarding any
component of a tuple return value — including the error code — is a
compile error.

```forge
// Must unpack all return values
(uptr addr, ErrorCode err) = find_free_frame();
if err != ErrorCode.Ok {
    // handle error
    return err;
}
// use addr safely here

```

Discarding the error is a compile error:

```forge
uptr addr = find_free_frame();      // COMPILE ERROR: second return value discarded
find_free_frame();                  // COMPILE ERROR: both return values discarded

```

To explicitly discard a value that is not needed, use `_`:

```forge
(uptr addr, _) = find_free_frame();   // COMPILE ERROR: cannot discard ErrorCode
(_, ErrorCode err) = find_free_frame(); // OK: addr discarded, err checked

```

The error code component of a tuple return may never be discarded with
`_`. Discarding the error code is always a compile error. All other
return values may be discarded with `_`.

### 7.4 Early Return on Error

Since there is no `?` operator, early return on error is explicit:

```forge
@mut ErrorCode setup_memory(uptr multiboot_info) {
    ErrorCode err = init_frame_allocator(multiboot_info);
    if err != ErrorCode.Ok { return err; }

    err = init_paging();
    if err != ErrorCode.Ok { return err; }

    err = map_kernel_sections();
    if err != ErrorCode.Ok { return err; }

    return ErrorCode.Ok;
}

```

This is deliberate. Every error propagation is visible in the source.
There is no hidden unwinding.

### 7.5 Defer on Error — `defer_err`

`defer_err` runs a cleanup statement only when the current scope exits
via a non-`Ok` error return. It is useful for rolling back partial
initialization.

```forge
@mut ErrorCode create_process(uptr entry) {
    (uptr stack, ErrorCode err) = alloc_page();
    if err != ErrorCode.Ok { return err; }
    defer_err free_page(stack);   // frees stack if anything below fails

    (uptr page_table, ErrorCode err2) = alloc_page();
    if err2 != ErrorCode.Ok { return err2; }
    defer_err free_page(page_table);

    // ... if we return ErrorCode.Ok, defer_err blocks do NOT run
    return ErrorCode.Ok;
}

```

`defer` (without `_err`) runs unconditionally on scope exit and
follows the same purity rules as regular calls.

### 7.6 Functions That Cannot Fail

Functions that cannot fail simply return their value directly with no
error code. This is the common case for pure computations.

```forge
uint32 page_index(uptr addr) {
    return (addr >> PAGE_SHIFT as uptr) as uint32;
}

```

There is no need to wrap infallible functions in any error type.

---

## 8. Control Flow

### 8.1 If / Else

```forge
if condition {
    ...
} else if other {
    ...
} else {
    ...
}

```

The condition must be `bool`. `if x` where `x` is an integer is a
compile error.

**If as expression.** Both branches must yield the same type. The
value of the selected branch is the value of the expression. This is
the only context in Forge where a value is produced without an
explicit `return`.

```forge
uint32 abs_x = if x >= 0int32 { x as uint32 } else { (-x) as uint32 };

```

### 8.2 Loop

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

### 8.3 While

```forge
while condition {
    do_work();
}

```

### 8.4 For

```forge
// Exclusive range
for i in 0..256uint32 {
    table[i] = 0uint32;
}

// Inclusive range
for i in 0..=255uint32 {
    table[i] = 0uint32;
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

The loop variable is immutable by default. Match arm literal values
and range bounds are inferred from the scrutinee type — suffixes not
required in these positions.

### 8.5 Match

`match` is exhaustive. Every possible value must be covered. No
fall-through.

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

### 8.6 Defer

`defer` schedules a statement to run when the current scope exits,
regardless of how. Multiple `defer` statements execute LIFO.

`defer` in a pure function may only call pure functions.
`defer` in a procedure may call anything.

```forge
Fd fd = open_file("/kernel.fg");
defer close_file(fd);

```

`defer_err` runs only on error exit. See §7.5.

---

## 9. Pointers and Memory

### 9.1 Address-Of and Dereference

```forge
@mut uint32 x = 42uint32;
uint32* p = &x;
uint32       v = *p;         // read
*p = 100uint32;              // write

```

Taking the address of an immutable variable produces `@const T*`.
Writing through `@const T*` is a compile error.

### 9.2 Pointer Arithmetic

```forge
uint32* base = get_base();
uint32* next   = base + 1usize;
uint8* offset = (base as uint8*) + 3usize;

```

### 9.3 Volatile Pointers

`@volatile` prevents the compiler from reordering, merging, or
eliminating the access. Required for MMIO registers.

```forge
@volatile uint16* vga = 0xB8000uptr as @volatile uint16*;
*vga = 0x0F41uint16;   // guaranteed to reach hardware

constexpr APIC_EOI: uptr = 0xFEE0_00B0uptr;
@volatile uint32* eoi = APIC_EOI as @volatile uint32*;
*eoi = 0uint32;

```

### 9.4 Restrict Pointers

`@restrict` asserts no aliasing with other pointers in scope.
Programmer responsibility — compiler uses it for better codegen.

```forge
@mut memcpy(@mut @restrict uint8* dst,
            @const @restrict uint8* src,
            usize len) {
    for i in 0..len { dst[i] = src[i]; }
}

```

### 9.5 Slices

```forge
[uint8; 512] buf;
[]uint8 s  = buf[0..];
[]uint8 s2 = buf[64usize..128usize];

uint8  first  = s[0usize];
uint8  last   = s[s.len - 1usize];
usize  length = s.len;
uint8* raw    = s.ptr;

```

### 9.6 No Global Allocator

All allocation requires an explicit allocator argument.

```forge
(uint8* page, ErrorCode err) = page_allocator.alloc(PAGE_SIZE);
if err != ErrorCode.Ok { return err; }
defer page_allocator.free(page);

```

### 9.7 Stack Allocation

Local arrays and structs are stack-allocated. No implicit heap.

```forge
[uint8; 4096] stack_page;
GDTEntry      entries[8];

```

---

## 10. Structs

### 10.1 Declaration

Fields use `type name` order. Comma-separated. Trailing comma
permitted.

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

### 10.2 Instantiation

```forge
Point p  = Point { x: 10int32, y: 20int32 };
Point q  = Point { y: 5int32, x: -3int32 };    // field order irrelevant
Point z  = Point {};                           // zero-initialized
Point p2 = Point { y: 99int32, ..p };          // struct update

```

### 10.3 Field Access

```forge
int32 px = p.x;
p.y = 20int32;    // COMPILE ERROR if p is immutable

```

### 10.4 Methods

Defined in `impl` blocks. Receiver is the first parameter.

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

A pure method takes `T self` or `*T self` (read-only). A mutating
method takes `@mut *T self` and must be `@mut`.

### 10.5 Layout Attributes

Struct layout is controlled by attributes. By default the compiler
inserts padding to satisfy alignment requirements.

**`@packed`** removes all compiler-inserted padding. Fields are placed
immediately after each other with no gaps. The struct may have
alignment 1. Use for hardware register layouts and protocol headers
where byte positions are specified by an external standard.

**`@align(N)`** forces the struct's minimum alignment to N bytes.
N must be a power of 2. Use for cache-line alignment and DMA buffers.

**`@repr("C")`** lays the struct out using the same rules as a C
compiler for the same target. Field order, padding, and alignment
match what a conforming C compiler would produce. Use for structs
shared with C code, system calls, or interfaces documented in C terms.

Attributes may be combined and are commutative:

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

@packed @repr("C")
struct EthernetHeader {
    [uint8; 6] dst_mac,
    [uint8; 6] src_mac,
    uint16     ethertype,
}

```

Layout intrinsics:

```forge
usize sz  = @sizeof(GDTEntry);
usize al  = @alignof(GDTEntry);
usize off = @offsetof(GDTEntry, access);

```

---

## 11. Unions

Unions allocate a single memory region shared by all fields. Size
equals the largest field. Reading any field is defined — the result
is the current bit pattern reinterpreted as the requested type.
No undefined behaviour.

```forge
union FloatBits {
    float32 as_float,
    uint32  as_int,
}

FloatBits fb   = FloatBits { as_float: 1.0float32 };
uint32    bits = fb.as_int;

```

---

## 12. Enums

### 12.1 Simple Enums

```forge
enum Color: uint8 {
    Red   = 0,
    Green = 1,
    Blue  = 2,
}

Color  c   = Color.Red;
uint8  raw = c as uint8;

```

### 12.2 Algebraic Enums (Tagged Unions)

Each variant may carry different data. Discriminant generated by
compiler. Must be matched exhaustively.

```forge
enum MemRegion {
    Free     { uptr base, usize size },
    Kernel   { uptr base, usize size, uint32 flags },
    MMIO     { uptr base, usize size, uint32 device_id },
    Reserved,
}

@mut describe(MemRegion r) {
    match r {
        Free     { base, size }      => log("free {} at {:x}", size, base),
        Kernel   { base, flags, .. } => log("kernel at {:x}", base),
        MMIO     { device_id, .. }   => log("mmio device {}", device_id),
        Reserved                     => log("reserved"),
    }
}

```

Discriminant backing type via `@repr`:

```forge
@repr("uint8")
enum Status {
    Ok    = 0,
    Error { uint32 code },
    Pending,
}

```

---

## 13. Modules

### 13.1 Module Declaration

Each `.fg` file declares its module at the top. Name maps to file path.

```forge
module kernel.memory.paging;

```

No header files. No forward declarations. Each name declared once.

### 13.2 Visibility

All declarations are private by default. `@pub` makes them visible
to importers.

```forge
module kernel.memory.frame;

uint32 scan_bitmap(usize start) { ... }   // private

@pub @mut (uptr, ErrorCode) alloc() { ... }
@pub @mut free(uptr phys_addr) { ... }
@pub struct AllocError { uint8 code, }

```

### 13.3 Importing

Every imported name must be listed explicitly. No wildcard imports.
Function imports use parentheses. Type imports use the bare name.

```forge
// Single function
import kernel.memory.paging.map_page();

// Multiple names
import kernel.memory.paging.{map_page(), unmap_page()};

// Type import — no parentheses
import kernel.memory.frame.AllocError;

// Qualified access after module import
import kernel.memory.paging;
kernel.memory.paging.map_page(virt, phys, flags);

```

### 13.4 Module Resolution

`kernel.memory.paging` → `kernel/memory/paging.fg` relative to
source root. `forge build` compiles all transitively imported modules,
each exactly once.

---

## 14. Attributes

Attributes modify the declaration immediately following them. Always
to the **left** of the declaration. Multiple attributes are
**commutative** — order never has semantic meaning.

```forge
@pub @inline uint32 fast(uint32 x) { return x; }
@inline @pub uint32 fast(uint32 x) { return x; }   // identical

```

See §22 for the full attribute reference.

---

## 15. Inline Assembly

Inline assembly is a first-class language feature. To support a direct,
self-contained ELF64 object file generation backend without requiring a
heavyweight macro assembler embedded within `forgec`, inline `asm` strings
and blocks must conform to a deterministic x86-64 instruction constraint dictionary
supported by the compiler backend's lightweight internal opcode mapper. Complex sequences
unrecognized by the mapping table are explicitly lowered via literal inline raw byte components.

### 15.1 Single-Instruction Form

```forge
@mut enable_interrupts()  { asm("sti"); }
@mut disable_interrupts() { asm("cli"); }

@noreturn @mut halt() { loop { asm("hlt"); } }

```

### 15.2 Full Form with Constraints

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

### 15.3 Multi-Line Block Form

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

### 15.4 Volatile Assembly

`@volatile` prevents reordering or elimination of the asm block.
`@volatile asm` is always a side effect — only valid in `@mut` context.

```forge
@mut memory_barrier() {
    @volatile asm("mfence" : : : "memory");
}

```

### 15.5 Constraint Reference

| Constraint | Register |
| --- | --- |
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

Clobbers: `"memory"` — accesses arbitrary memory.
`"cc"` — modifies flags register.

---

## 16. Comptime

`comptime` runs entirely at compile time. Typed, scoped, debuggable.
Not a text preprocessor.

### 16.1 Constexpr Blocks

```forge
comptime {
    constexpr PAGE_SHIFT: uint32 = 12uint32;
    constexpr PAGE_SIZE:  usize  = 1usize << PAGE_SHIFT;
    constexpr PAGE_MASK:  uptr   = ~(PAGE_SIZE as uptr - 1uptr);

    assert(@sizeof(GDTEntry) == 8usize, "GDT entry must be 8 bytes");
    assert(PAGE_SIZE == 4096usize,      "PAGE_SIZE must be 4096");
}

```

Compile-time assertion failure is a compile error, not a runtime panic.

### 16.2 Comptime If — Conditional Compilation

Replaces `#ifdef`. Typed, scoped, no text substitution.

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

### 16.3 Comptime Functions

Evaluated at compile time when called with compile-time arguments.

```forge
comptime usize kib(usize n) { return n * 1024usize; }
comptime usize mib(usize n) { return n * 1024usize * 1024usize; }

constexpr HEAP_SIZE:  usize = mib(16usize);
constexpr STACK_SIZE: usize = kib(64usize);

```

### 16.4 Built-In Comptime Intrinsics

| Intrinsic | Result | Description |
| --- | --- | --- |
| `@sizeof(T)` | `usize` | Size of T in bytes |
| `@alignof(T)` | `usize` | Alignment of T in bytes |
| `@offsetof(T, field)` | `usize` | Byte offset of field in T |
| `@bitcast<T>(val)` | `T` | Reinterpret bit pattern as T |
| `@error("msg")` | — | Emit compile error |
| `@warning("msg")` | — | Emit compile warning |

---

## 17. OS and Hardware Primitives

Always available. No import required.

### 17.1 Port I/O

`port<T>` is an x86 I/O port type. `T` must be `uint8`, `uint16`,
or `uint32`. Writes use `<-`. Reads use `.read()`. Both are side
effects and require `@mut` context.

```forge
port<uint8> pic_master_cmd  = 0x20uint16;
port<uint8> pic_master_data = 0x21uint16;

@mut send_eoi() {
    pic_master_cmd <- 0x20uint8;
}

@mut uint8 read_pic_status() {
    return pic_master_cmd.read();
}

```

### 17.2 Interrupt Service Routines

`@interrupt` generates a complete ISR prologue and epilogue. ISR
handlers are always procedures.

```forge
@interrupt @mut isr_divide_error(@const *InterruptFrame frame) {
    panic("Divide by zero at rip={:x}", frame.rip);
}

@interrupt @mut isr_page_fault(@const *InterruptFrame frame,
                                uint64 error_code) {
    uint64 cr2 = @cpu.read_cr2();
    handle_page_fault(cr2, error_code, frame);
}

@naked @noreturn @mut isr_double_fault() {
    asm {
        call double_fault_handler
    .hang:
        hlt
        jmp .hang
    }
}

```

`InterruptFrame` struct:

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

### 17.3 Linker Symbol Integration

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
    @mut uint8* p = start as @mut uint8*;
    for i in 0..len { p[i] = 0uint8; }
}

```

### 17.4 CPU Register Access

Reads are pure. Writes require `@mut` context.

```forge
// Pure reads
uint64 cr0  = @cpu.read_cr0();
uint64 cr2  = @cpu.read_cr2();
uint64 cr3  = @cpu.read_cr3();
uint64 cr4  = @cpu.read_cr4();
uint64 efer = @cpu.rdmsr(0xC000_0080uint32);
bool   ie   = @cpu.interrupts_enabled();

struct CpuidResult { uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, }
CpuidResult info = @cpu.cpuid(1uint32, 0uint32);

// Writes — @mut context required
@mut init_cr3(uptr pml4_phys) {
    @cpu.write_cr3(pml4_phys);
}

@mut enable_pae() {
    @cpu.write_cr4(@cpu.read_cr4() | (1uint64 << 5uint64));
}

@mut enable_long_mode() {
    @cpu.wrmsr(0xC000_0080uint32,
               @cpu.rdmsr(0xC000_0080uint32) | (1uint64 << 8uint64));
}

```

### 17.5 Atomic Operations

Atomic loads are pure. Stores and read-modify-write require `@mut`.

```forge
atomic<uint64> counter;   // zero-initialized

uint64 read_counter() {
    return counter.load(.Acquire);
}

@mut tick() {
    counter.fetch_add(1uint64, .SeqCst);
}

@mut reset() {
    counter.store(0uint64, .Release);
}

@mut bool try_lock() {
    return counter.compare_exchange(0uint64, 1uint64, .SeqCst, .Relaxed);
}

```

**Memory orderings:**

| Ordering | Meaning |
| --- | --- |
| `.Relaxed` | Atomic only — no ordering |
| `.Acquire` | Sees all stores before matching Release |
| `.Release` | All prior stores visible to matching Acquire |
| `.AcqRel` | Both Acquire and Release |
| `.SeqCst` | Total sequential consistency |

---

## 18. Memory Model

### 18.1 Execution Model

Single-threaded at the language level. Concurrency via atomic
operations, MMIO, and interrupt handlers.

### 18.2 Address Space

FLat 64-bit virtual address space. Physical address management is the
programmer's responsibility. `uptr` is the type for raw addresses.

### 18.3 Stack

Grows downward. Frames follow System V AMD64 ABI (§20). `@naked`
functions have no generated frame.

### 18.4 Object Lifetime

A local variable's lifetime spans its declaration to the end of its
enclosing block. Using a pointer to a local after its lifetime is a
compile error where detectable.

---

## 19. Undefined Behaviour

**Forge has no undefined behaviour.**

| Operation | C | Forge |
| --- | --- | --- |
| Signed integer overflow | Undefined | Wraps (two's complement) |
| Unsigned integer overflow | Wraps | Wraps |
| Division by zero | Undefined | Traps |
| Null pointer dereference | Undefined | Cannot occur |
| Out-of-bounds array access | Undefined | Traps |
| Uninitialized read | Undefined | Compile error |
| Shift by >= type width | Undefined | Result is 0 |
| Left shift of negative | Undefined | Defined by two's complement |
| Pointer aliasing violation | Undefined | No strict aliasing rule |
| Reading wrong union field | Undefined | Returns current bit pattern |

---

## 20. ABI and Calling Conventions

### 20.1 Default — System V AMD64

**Integer/pointer arguments:** `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.
Additional arguments on the stack.

**Float arguments:** `xmm0`–`xmm7`.

**Return values:** ≤ 64-bit → `rax`. 65–128-bit → `rax:rdx`.
Float → `xmm0`. Multiple return values follow the struct-return rules.

**Caller-saved:** `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`–`r11`,
`xmm0`–`xmm15`

**Callee-saved:** `rbx`, `rbp`, `r12`–`r15`

**Stack alignment:** 16-byte aligned before `call`.

### 20.2 Multiple Return Values and the ABI

Multiple return values are passed as if the function returns a struct
containing all the return values in declaration order. To avoid hidden
spill pointers passed via `rdi` and prevent non-deterministic stack framing
allocation, the combined size of all components of a multiple return value
**must not exceed 16 bytes**. Values $\le$ 16 bytes are guaranteed to be
passed purely inside native platform registers (`rax`, `rdx`) with zero
runtime execution overhead. Exceeding 16 bytes for a multi-return declaration
is a strict compile error.

```forge
// (uptr, ErrorCode) — 8 + 4 = 12 bytes — explicitly fits in rax:rdx
(uptr addr, ErrorCode err) = alloc_page();

```

### 20.3 Struct Passing

Structs ≤ 16 bytes are passed in registers per the AMD64 classification
algorithm. Structs > 16 bytes are passed on the stack by value.

### 20.4 Interrupt Frame Layout

```
[rsp+32]  ss
[rsp+24]  rsp (user)
[rsp+16]  rflags
[rsp+ 8]  cs
[rsp+ 0]  rip

```

For error-code vectors:

```
[rsp+40]  ss
[rsp+32]  rsp (user)
[rsp+24]  rflags
[rsp+16]  cs
[rsp+ 8]  rip
[rsp+ 0]  error_code

```

---

## 21. Object Files and Linking

### 21.1 Output Format

`forgec` directly maps compiled symbols to produce ELF64 object files natively.
Linked by `ld` or `lld`. The compiler does not include an external linker.

### 21.2 Sections

| Section | Contents |
| --- | --- |
| `.text` | Compiled function and procedure bodies |
| `.rodata` | String literals, read-only data |
| `.data` | Initialized mutable globals |
| `.bss` | Zero-initialized globals (no bytes in file) |
| `.note.forge` | Compiler version and target information |

Additional sections via `@section("name")`.

### 21.3 Symbol Visibility

Private symbols are local to the object file. `@pub` symbols are
exported. `@export("name")` sets the exact symbol name bypassing
mangling — use for stable entry points.

### 21.4 Name Mangling

Forge mangles names to encode module path and parameter types. Not
stable across compiler versions. Use `@export("name")` for stable
symbols.

---

## 22. Compiler Attributes Reference

### 22.1 Variable Attributes

| Attribute | Effect |
| --- | --- |
| `@mut` | Variable is mutable after initialization |
| `@uninit` | Not zero-initialized; compiler tracks reads |
| `@const` | Pointee is immutable |
| `@volatile` | Pointee accesses never reordered or eliminated |
| `@restrict` | No aliasing with other pointers in scope |
| `@section("name")` | Place in named ELF section |
| `@align(N)` | Minimum alignment; N must be power of 2 |

### 22.2 Function and Procedure Attributes

| Attribute | Effect |
| --- | --- |
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

### 22.3 Type Attributes

| Attribute | Applies To | Effect |
| --- | --- | --- |
| `@pub` | struct / enum / union | Visible to importers |
| `@packed` | struct | Remove all padding |
| `@align(N)` | struct | Force minimum alignment |
| `@repr("C")` | struct / enum | C-compatible layout |
| `@repr("uintN")` | enum | Set discriminant backing type |

---

## 23. What Forge Deliberately Omits

| Omitted Feature | Reason |
| --- | --- |
| **Garbage collector** | Unpredictable latency; incompatible with interrupt handlers |
| **Exceptions** | Hidden control flow — violates §1.2 |
| **Type inference on expressions** | You always know the type. Inference hides it. |
| **Implicit return** | Every function exit must be an explicit `return`. |
| **`fn` keyword** | The return type is sufficient. C got this right. |
| **`void` keyword** | Procedures use `@mut` and no return type. |
| **`never` type / trailing `->**` | Replaced by `@noreturn` attribute. |
| **`pub` keyword** | Visibility is an attribute. `@pub` is consistent. |
| **`const` keyword** | Replaced by `constexpr` — unambiguously compile-time. |
| **`Result<T,E>` / `?` / `catch**` | Replaced by multiple return values. Errors are plain values. |
| **Generics** | Deferred to a future version. |
| **Wildcard imports** | Every imported name must be listed explicitly. |
| **Implicit integer promotion** | Source of a significant fraction of C CVEs. |
| **Null pointers** | NULL does not exist. |
| **Stateful closures** | Eradicated environment capture complexity to enforce purely explicit context state passing (§6.9). |
| **Tuple return spillage (>16 bytes)** | Enforces strict compile-time register mapping restrictions to maintain predictable runtime boundaries (§20.2). |
| **Undefined behaviour** | See §19. Every operation is defined. |
| **Text preprocessor** | `comptime if` replaces `#ifdef`. Modules replace `#include`. |
| **Header files** | Modules replace this entirely. |
| **Global allocator** | The kernel defines its own memory model. |
| **Hidden copies** | Large types passed by pointer explicitly. |
| **Implicit conversions** | Every type change requires explicit `as`. |
| **Variadic functions** | Use slices instead. |
| **`goto`** | Use `loop { break; }` or labeled breaks. |
| **Operator overloading** | `+` always means addition. |
| **Dynamic dispatch / vtables** | No virtual dispatch of any kind. |
| **RTTI** | No runtime type information. |
| **Built-in `panic` or `memset**` | Standard library functions, not language primitives. |

---

## 24. Appendix A — Kernel Entry Point Example

```forge
module kernel.boot;

import kernel.memory.paging.{init_paging()};
import kernel.memory.frame.{init_frame_allocator()};
import kernel.cpu.gdt.{init_gdt()};
import kernel.cpu.idt.{init_idt()};
import kernel.drivers.uart.{init_uart(), uart_write()};
import forge.panic.{panic()};

extern uint8 __bss_start;
extern uint8 __bss_end;

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

    // Early serial output
    init_uart(0x3F8uint16, 115200uint32);
    uart_write("Forge kernel starting\n");

    // Validate bootloader
    if multiboot_magic != 0x36D7_6289uint32 {
        panic("Invalid Multiboot2 magic");
    }

    // Initialize memory subsystem
    ErrorCode err = init_frame_allocator(multiboot_info);
    if err != ErrorCode.Ok {
        panic("Frame allocator init failed");
    }

    err = init_paging();
    if err != ErrorCode.Ok {
        panic("Paging init failed");
    }

    uart_write("Memory OK\n");

    idle_loop();
}

@mut clear_bss() {
    uptr  start = &__bss_start as uptr;
    uptr  end   = &__bss_end   as uptr;
    usize len   = (end - start) as usize;
    @mut uint8* p = start as @mut uint8*;
    for i in 0..len { p[i] = 0uint8; }
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

## 25. Appendix B — Physical Frame Allocator Example

```forge
module kernel.memory.frame;

import forge.panic.{panic()};

constexpr MAX_FRAMES: usize = 1_048_576usize;   // 4 GiB / 4 KiB

@uninit [uint8; MAX_FRAMES / 8usize] bitmap;
@mut usize total_frames = 0usize;
@mut usize free_frames  = 0usize;

enum ErrorCode: uint32 {
    Ok          = 0,
    OutOfMemory = 1,
}

/// Initialize from a Multiboot2 memory map.
/// Marks all frames used, then marks usable regions free.
@pub @mut ErrorCode init(uptr multiboot_info) {
    for i in 0..bitmap.len {
        bitmap[i] = 0xFFuint8;
    }

    @const *MemoryMap map = multiboot2_memory_map(multiboot_info);

    for entry in map {
        if entry.kind == 1uint32 {
            usize base  = (entry.base / PAGE_SIZE as uint64) as usize;
            usize count = (entry.size / PAGE_SIZE as uint64) as usize;
            for frame in base..base + count {
                set_free(frame);
                free_frames += 1usize;
            }
            total_frames += count;
        }
    }

    return ErrorCode.Ok;
}

/// Allocate one physical frame.
/// Returns its physical base address and an error code.
@pub @mut (uptr, ErrorCode) alloc() {
    for i in 0..bitmap.len {
        if bitmap[i] != 0xFFuint8 {
            for bit in 0..8uint8 {
                if (bitmap[i] >> bit) & 1uint8 == 0uint8 {
                    bitmap[i] |= 1uint8 << bit;
                    free_frames -= 1usize;
                    usize frame = i * 8usize + bit as usize;
                    return (frame as uptr * PAGE_SIZE as uptr,
                            ErrorCode.Ok);
                }
            }
        }
    }
    return (0uptr, ErrorCode.OutOfMemory);
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

## 26. Appendix C — Compile-Time Guarantees

A conforming `forgec` implementation guarantees:

**Type safety.** No program that compiles without errors can read a
value as a different type without an explicit `as` or `@bitcast`,
access a nonexistent struct field, or call a function with wrong
argument types.

**Purity enforcement.** No pure function writes to global state or
performs I/O. Verified statically. A procedure call or volatile memory read/write
from a pure function is a compile error.

**No implicit return.** Every value-returning function must have an
explicit `return` on every reachable exit path. Missing `return` is
a compile error.

**No discarded error codes.** The error code component of a multiple
return value may never be discarded. Doing so is a compile error.

**Register-bound return values.** Every function or procedure returning multiple
values fields a deterministic combined alignment footprint $\le$ 16 bytes. Exceeding
this register threshold triggers a terminal compilation failure.

**Coarse variable tracking.** Every `@uninit` allocation assertion tracks data safety
exclusively at the clear identifier variable level. Partial structural reads across
unassigned aggregates are checked out as global scope errors.

**No hidden control flow.** Every exit path is visible in the source.
No destructors with side effects. No implicit early returns.

**Exhaustive matching.** Every `match` on an algebraic enum covers
all variants. Adding a variant is a compile error in every matching
file without a wildcard arm.

**Defined overflow.** Integer overflow wraps in two's complement.
The compiler never uses overflow as an optimization assumption.

**Zero initialization.** Every variable not marked `@uninit` is
zero-initialized before first read. Emitted explicitly by the
compiler — not relying on linker behaviour for stack variables.

**Uninitialized read prevention.** Every `@uninit` variable is
tracked. Reading before a provable write is a compile error.

**Noreturn verification.** A reachable `return` in a `@noreturn`
declaration is a compile error.

---

*The Forge Programming Language Specification, Version 1.4*
*Copyright © 2025 — GNU General Public License v3.0*
*The Forge compiler (forgec) is licensed under GPLv3 with a runtime exception.*
*Code produced by forgec is not subject to the GPL.*
