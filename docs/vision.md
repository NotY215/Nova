# Vayu Vision & Roadmap

Vayu is an independent programming-language project aiming to combine readable Python-inspired syntax with native performance, low-level control, modern application development, portability, and an AI-ready ecosystem.

> **Write simple. Run native. Control everything.**

## Current Foundation

The current compiler demonstrates a growing language foundation including:

- variables and type inference
- explicit type annotations
- primitive and collection types
- functions and recursion
- conditions and loops
- arithmetic, comparison, and logical expressions
- lists and maps
- structs
- classes and inheritance
- lambdas and closures
- exceptions
- modules and imports
- math functionality
- type checking
- AST generation
- VM-oriented execution

The project is being developed incrementally; completed functionality should remain stable while new compiler phases are added.

## Roadmap

### 1. Compiler & Language Stabilization

- strengthen lexer and parser behavior
- expand semantic analysis
- improve type checking
- improve diagnostics and error reporting
- expand regression tests
- stabilize AST and intermediate representations

### 2. Runtime, VM & Native Code Generation

- strengthen bytecode and VM execution
- introduce/expand IR optimization
- native machine-code generation
- linking and executable generation
- runtime performance improvements
- platform-specific backends

### 3. Memory & Resource Management

- ownership/resource-management model
- deterministic cleanup
- smart-pointer concepts such as `unique<T>`, `shared<T>`, and `weak<T>`
- low-level memory operations where appropriate
- safe and explicit escape hatches for systems programming

### 4. Standard Library

- collections
- strings and utilities
- filesystem
- processes
- time/date
- networking
- concurrency primitives
- serialization
- platform abstractions

### 5. Package Ecosystem

- `nva` package workflow
- dependency resolution
- versioning and lock files
- publishing
- native dependencies
- build integration

### 6. Interoperability

- C interoperability
- C++ interoperability
- Python ecosystem/runtime interoperability
- future JVM integration
- future .NET integration

### 7. Concurrency & Networking

- threads/tasks
- synchronization primitives
- asynchronous execution
- networking APIs
- scalable server/application foundations

### 8. Application, GUI & Graphics

- desktop GUI framework
- cross-platform application APIs
- OpenGL/Vulkan/DirectX integrations
- rendering and graphics utilities
- audio/input/physics foundations for game development

### 9. AI, ML & Scientific Computing

- numerical primitives
- tensor APIs
- GPU acceleration
- CUDA/ROCm integration
- ONNX support
- Python AI ecosystem interoperability
- scientific/data-processing libraries

### 10. Developer Tooling

- formatter
- package/project tooling
- debugger
- language server / LSP
- IDE integrations
- profiling and diagnostics
- documentation tooling

### 11. Metaprogramming & Advanced Language Features

- compile-time capabilities
- metaprogramming
- stronger generics
- advanced type-system features
- safer low-level abstractions

### 12. Self-Hosting

A long-term goal is a mature Vayu compiler that can be substantially implemented in Vayu itself, reducing dependence on the bootstrap implementation and demonstrating the language's own capabilities.

## Benchmarking

Benchmarking is part of the development process. Performance measurements should track representative compiler/runtime workloads as native execution and optimization improve.

Published benchmark information can change as the implementation and test environment change, so the [official Vayu website](https://vayu.gt.tc) is the preferred location for current benchmark information.

## Long-Term Goal

The ultimate goal is not simply "Python but faster" or "C++ with Python syntax." Vayu aims to become a unified ecosystem capable of covering:

**simple programs → applications → AI → games → systems → native software**

while keeping the language approachable and giving developers progressively more control when they need it.
