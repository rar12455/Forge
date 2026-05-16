# The Forge Programming Language

## Manifesto

Forge exists because the available options are all wrong in different ways.

C is transparent and close to the hardware but it is 50 years old and it shows. Its dangers are not features. They are scars. The undefined behaviour, the implicit conversions, the preprocessor, the missing module system — none of these are fundamental to systems programming. They are accidents of 1972 that became tradition. The kernel developers who write safe C have internalized a second type checker in their heads to compensate for the one the language doesn't provide. That skill is real and admirable. It should not be necessary.

Rust got the safety goals right and then kept going until the language became the product. The borrow checker is brilliant engineering. It is also something you fight constantly when writing OS code where the hardware doesn't follow ownership rules. The unsafe blocks proliferate. The complexity compounds. The abstraction layer between you and the machine grows.

Zig has the right instincts — no hidden control flow, explicit allocators, comptime instead of a preprocessor. But it has been "almost-stable" for years, the syntax has a learning curve that isn't proportional to what you gain, and it gets too many smaller things wrong.

Forge is not a general-purpose language. It is a language for writing operating systems on x86-64. That constraint is deliberate. It is the source of every good decision in the design. A language that tries to be good at everything is good at nothing. Forge tries to be exactly right for one thing.

The goal is simple: crystal-clear syntax that gives you what you need without polluting the screen. Verbosity is not rigour. Boilerplate is not safety. Complexity is not power. You should be able to read any Forge function and know exactly what the CPU will do. No surprises. No magic. No hidden anything.

This is what systems programming should feel like.

## Description

Forge is a highly-specialized, minimalist systems programming language designed explicitly for low-level bare-metal environments, including **bootloaders, operating system kernels, hypervisors, embedded firmware, and device drivers**. 

The design philosophy prioritizes absolute predictability of machine code, zero runtime overhead, explicit syntax over implicit behavior, and a strict absence of undefined behavior.

## Key Design Principles

1. **No Undefined Behaviour:** Every operation has a completely defined result. Signed integer overflow wraps by default; division by zero and out-of-bounds accesses trigger an immediate hardware trap. Null pointers do not exist as plain values.
2. **Explicit Over Implicit:** No type inference on expressions, no implicit integer promotion, no hidden allocations, and no implicit return paths. Discarding error codes triggers a terminal compile-time error.
3. **Purity by Default:** Functions are mathematically pure by default and checked statically. Side-effectful mutations, port I/O, and global modifications are strictly confined to procedures marked explicitly with `@mut`.
4. **Predictable Code Generation:** The programmer should be able to predict the assembly output of any Forge function without running the compiler.
5. **Freestanding First:** Forge assumes no operating system, no C runtime (`libc`), and no heap allocator. Every language primitive works reliably at an interrupt service level or before virtual memory is initialized.

## Licensing

* **Compiler (`forgec`):** Distributed under the **GNU General Public License version 3 (GPLv3)**. Any modifications to the compiler frontend or backend must remain open source under the GPLv3.
* **Runtime & Header Primitives:** Governed by the **Forge Runtime Library Exception (v3.1)**. Low-level core startup stubs, type macros, or architectural intrinsics combined or linked during the compilation process do *not* force target operating systems or applications to inherit the GPL copyleft license. Permissive or proprietary target software is fully supported.
