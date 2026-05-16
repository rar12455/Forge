# The Forge Programming Language

Forge is a highly-specialized, minimalist systems programming language designed explicitly for low-level bare-metal environments, including **bootloaders, operating system kernels, hypervisors, embedded firmware, and device drivers**. 

The design philosophy prioritizes absolute predictability of machine code, zero runtime overhead, explicit syntax over implicit behavior, and a strict absence of undefined behavior.

## Key Design Principles

1. **No Undefined Behaviour:** Every operation has a completely defined result. Signed integer overflow wraps by default; division by zero and out-of-bounds accesses trigger an immediate hardware trap. Null pointers do not exist as plain values.
2. **Explicit Over Implicit:** No type inference on expressions, no implicit integer promotion, no hidden allocations, and no implicit return paths. Discarding error codes triggers a terminal compile-time error.
3. **Purity by Default:** Functions are mathematically pure by default and checked statically. Side-effectful mutations, port I/O, and global modifications are strictly confined to procedures marked explicitly with `@mut`.
4. **Register-Bound Multi-Returns:** Multiple return values are used for error handling, restricted strictly to a maximum combined footprint of 16 bytes to guarantee allocation entirely within native CPU registers (`rax:rdx`), eliminating hidden stack-spill pointer complexity.
5. **Freestanding First:** Forge assumes no operating system, no C runtime (`libc`), and no heap allocator. Every language primitive works reliably at an interrupt service level or before virtual memory is initialized.

## Repository Layout

* `bin/` - Target directory for compiled binaries.
* `examples/` - Reference Forge programs and bare-metal stubs.
* `papers/specs/` - Normative language specification drafts (v1.0 through v1.4).
* `src/` - The bootstrap compiler implementation (`forgec`), hand-rolled in ISO C99.
  * `headers/` - Component function and compiler state definitions.
  * `implementation/` - Lexer, parser, symbol table, and ELF64 code-generation passes.

## Licensing

* **Compiler (`forgec`):** Distributed under the **GNU General Public License version 3 (GPLv3)**. Any modifications to the compiler frontend or backend must remain open source under the GPLv3.
* **Runtime & Header Primitives:** Governed by the **Forge Runtime Library Exception (v3.1)**. Low-level core startup stubs, type macros, or architectural intrinsics combined or linked during the compilation process do *not* force target operating systems or applications to inherit the GPL copyleft license. Permissive or proprietary target software is fully supported.
