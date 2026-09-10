# 🌌 Nova

<p align="center">
  <img src="assets/logo.svg" alt="Nova Programming Language" width="260">
</p>

<h1 align="center">Nova</h1>

<p align="center">
  <strong>Python simplicity. Native performance. Low-level control. AI-ready.</strong>
</p>

<p align="center">
  A modern general-purpose programming language designed to combine the
  simplicity of Python with the performance and control of C++, while
  incorporating the productivity and ecosystem concepts of C# and Java.
</p>

<p align="center">

![Status](https://img.shields.io/badge/Status-Experimental-orange.svg)
![Language](https://img.shields.io/badge/Language-Nova-6C5CE7.svg)
![Syntax](https://img.shields.io/badge/Syntax-Python--Inspired-3776AB.svg)
![Performance](https://img.shields.io/badge/Performance-Native--Compiled-blue.svg)
![AI](https://img.shields.io/badge/AI-First--Class-purple.svg)
![Platform](https://img.shields.io/badge/Platform-Cross--Platform-success.svg)
![License](https://img.shields.io/badge/License-TBD-lightgrey.svg)

</p>

---

## 🚀 What is Nova?

**Nova** is a modern general-purpose programming language designed around a
simple idea:

> **Make native programming as approachable as Python without giving up the
> performance and control expected from systems languages.**

Nova combines ideas inspired by:

- 🐍 **Python** — simplicity, readability, rapid development and AI ecosystem
- ⚡ **C++** — native performance, memory control and low-level programming
- ☕ **Java** — portability, structured development and large-scale software
- 💜 **C#** — modern application development, GUI capabilities and tooling

Nova is **not intended to be a direct replacement or clone** of any of these
languages.

Instead, Nova aims to create a single coherent programming experience from
the strongest ideas behind them.

---

# ✨ The Nova Philosophy

Nova follows four primary principles:

### 1. Simple by default

Writing basic programs should feel almost as easy as Python.

```nova
print("Hello, Nova!")
````

### 2. Powerful when needed

Advanced programmers should be able to access:

* pointers
* references
* manual memory management
* native libraries
* system APIs
* multithreading
* SIMD
* GPU APIs
* low-level hardware functionality

### 3. Native performance

Nova is designed as a **compiled language**, rather than relying on a
traditional interpreter for normal execution.

The goal is to allow Nova applications to achieve performance comparable to
other native compiled languages for suitable workloads.

### 4. One language, multiple levels

Nova should be usable for:

```text
Simple scripts
      ↓
Desktop applications
      ↓
Web/network applications
      ↓
AI / Machine Learning
      ↓
Game development
      ↓
High-performance applications
      ↓
Systems programming
      ↓
Low-level software
```

---

# 🧬 Why Nova?

Modern programming often forces developers to choose between:

| Language    | Major Strength                               |
| ----------- | -------------------------------------------- |
| 🐍 Python   | Simplicity, AI, data science                 |
| ⚡ C++       | Performance and low-level control            |
| ☕ Java      | Portability and large-scale software         |
| 💜 C#       | Application development and GUI              |
| 🌌 **Nova** | **A unified combination of these strengths** |

Nova attempts to reduce the gap between:

```text
Easy to write
      ↕
Fast to execute

High-level
      ↕
Low-level

Rapid development
      ↕
Hardware control
```

---

# 📝 Syntax

Nova uses a **Python-inspired syntax**.

It is intentionally not 100% identical to Python.

The goal is to keep familiar concepts while allowing Nova to introduce
features necessary for a compiled, statically typed and low-level language.

## Hello World

```nova
print("Hello, World!")
```

No:

```cpp
#include <iostream>
using namespace std;

int main() {
    cout << "Hello";
}
```

No unnecessary boilerplate.

---

# 📦 Variables

Nova supports simple variable declarations.

```nova
name = "NotY"
age = 17
score = 95.5
active = true
```

Explicit types can also be used when required:

```nova
name: str = "NotY"
age: int = 17
score: float64 = 95.5
```

Nova is designed to use **type inference** wherever possible.

---

# 🔢 Type System

Nova provides common primitive types:

```text
int
uint
int8
int16
int32
int64

uint8
uint16
uint32
uint64

float32
float64

bool
char
str
bytes
```

It also supports higher-level types such as:

```text
list
array
map
set
tuple
struct
class
enum
optional
generic
pointer
reference
```

Example:

```nova
numbers: list<int> = [10, 20, 30, 40]
```

---

# 🧠 Type Inference

Nova attempts to reduce unnecessary type declarations.

Instead of:

```nova
int number = 100;
```

or:

```python
number: int = 100
```

Nova allows:

```nova
number = 100
```

The compiler can infer:

```text
number → int
```

while still maintaining strong type information internally.

---

# 🔧 Functions

Functions use Python-inspired syntax.

```nova
def add(a: int, b: int) -> int:
    return a + b
```

Usage:

```nova
result = add(10, 20)

print(result)
```

---

# 🔀 Conditions

```nova
if score >= 90:
    print("Excellent")
elif score >= 60:
    print("Good")
else:
    print("Needs improvement")
```

Nova uses indentation-oriented blocks instead of requiring braces for
ordinary code.

---

# 🔁 Loops

```nova
for number in numbers:
    print(number)
```

And:

```nova
while running:
    update()
```

---

# 🏗️ Object-Oriented Programming

Nova supports modern object-oriented programming.

```nova
class Player:

    name: str
    health: int

    def __init__(self, name: str):
        self.name = name
        self.health = 100

    def damage(self, amount: int):
        self.health -= amount
```

Usage:

```nova
player = Player("Nova")

player.damage(20)

print(player.health)
```

Nova is designed to support:

* Classes
* Objects
* Encapsulation
* Inheritance
* Polymorphism
* Interfaces
* Abstract types
* Properties
* Static members
* Method overriding

---

# 🧱 Structs

For lightweight data structures:

```nova
struct Vector3:
    x: float32
    y: float32
    z: float32
```

Usage:

```nova
position = Vector3(
    x=10,
    y=20,
    z=30
)
```

---

# 🧮 Generics

Nova is designed to support generic programming.

```nova
def maximum<T>(a: T, b: T) -> T:

    if a > b:
        return a

    return b
```

Generic collections:

```nova
numbers: list<int>
names: list<str>

scores: map<str, int>
```

---

# 🧠 Memory Management

Memory management is one of Nova's major design goals.

Nova aims to provide both:

### High-level memory management

```nova
player = Player("Nova")
```

The programmer does not need to manually manage every allocation.

### Low-level memory control

Advanced programs can explicitly control memory when necessary.

```nova
ptr = allocate<int>()
```

and:

```nova
free(ptr)
```

Nova is intended to support concepts such as:

* Stack allocation
* Heap allocation
* References
* Pointers
* Ownership
* Resource management
* Smart pointers
* Deterministic cleanup
* Custom allocators
* Memory pools
* Unsafe operations

The objective is:

> **Easy memory management for beginners, powerful memory control for experts.**

---

# ⚠️ Unsafe Operations

Low-level operations should be explicitly identifiable.

Conceptually:

```nova
unsafe:

    ptr = allocate<int>()

    # low-level memory operations

    free(ptr)
```

This separates ordinary application programming from operations that can
directly affect memory and hardware.

---

# ⚡ Native Performance

Nova is designed around native compilation.

The language is intended for applications where execution speed matters:

* Game engines
* Graphics
* Simulations
* Scientific computing
* Servers
* Desktop applications
* AI infrastructure
* System software
* High-performance applications

Nova does **not** promise that every program will automatically be faster than
C++, Rust, Java or C#.

Performance depends on:

* Algorithms
* Memory access
* Compiler optimization
* Data structures
* Hardware
* Runtime behavior

The goal is to provide the compiler and language features necessary for
**high-performance native software**.

---

# 🧵 Concurrency

Nova is designed for modern multicore systems.

Planned concurrency capabilities include:

* Threads
* Tasks
* Parallel execution
* Futures
* Channels
* Synchronization
* Thread-safe collections
* Parallel loops

Conceptually:

```nova
task download(url):
    ...

task1 = spawn download(url1)
task2 = spawn download(url2)

wait(task1)
wait(task2)
```

---

# 🤖 AI & Machine Learning

AI is a major target of Nova.

Nova is designed to interact with the existing AI ecosystem while eventually
providing native AI functionality.

Potential ecosystem:

```text
nova.ai
nova.ml
nova.tensor
nova.nn
nova.data
nova.cuda
```

Example:

```nova
import nova.ai

model = Model()

model.train(data)
```

Nova aims to work with technologies such as:

* NumPy
* PyTorch
* TensorFlow
* ONNX
* OpenCV
* CUDA
* GPU acceleration
* Scientific computing libraries

---

# 🐍 Python Ecosystem

One of Nova's major goals is **Python interoperability**.

Existing Python libraries are extremely valuable, especially in:

* AI
* Machine learning
* Data science
* Scientific computing
* Automation

Nova therefore aims to allow compatible Python packages to be used through
an interoperability layer.

Example:

```nova
import numpy

data = numpy.array([1, 2, 3, 4])

print(data)
```

Package installation:

```text
nva install numpy
```

The long-term goal is to make Nova capable of using a large portion of the
Python ecosystem without requiring developers to rewrite every library.

> Python interoperability does not mean Python code magically becomes native
> Nova code. Python libraries may initially execute through a Python runtime
> interoperability layer.

---

# 🎨 GUI Development

Nova is designed to be suitable for desktop GUI applications.

Potential API:

```nova
import nova.gui

window = Window(
    title="Nova Application",
    width=800,
    height=600
)

button = Button("Click Me")

window.add(button)

window.show()
```

The goal is to provide:

* Windows support
* Linux support
* macOS support
* Native-looking applications
* Modern UI controls
* Events
* Layouts
* Graphics
* Custom widgets

---

# 🎮 Game Development

Nova is also designed with game development in mind.

Potential modules:

```text
nova.game
nova.graphics
nova.physics
nova.audio
nova.input
nova.math
```

Graphics API interoperability is intended to include:

```text
DirectX
Vulkan
OpenGL
```

This allows Nova to potentially be used for:

* 2D games
* 3D games
* Game tools
* Game editors
* Rendering systems
* Simulations
* Game engines

---

# 🔌 C / C++ Interoperability

C and C++ have one of the largest native software ecosystems.

Nova therefore aims to provide strong native interoperability.

Potential capabilities:

```text
C libraries
C++ libraries
DLL
Shared libraries
Static libraries
Native system APIs
Graphics APIs
Hardware APIs
```

This allows existing native software to remain useful instead of forcing
developers to rewrite mature libraries.

---

# ☕ Java Interoperability

Java provides an enormous ecosystem and excellent cross-platform
application development.

Nova's long-term interoperability goals include access to selected Java
libraries and applications.

Potential technologies include:

```text
JNI
JVM integration
Generated bindings
```

Java interoperability is intended to be optional.

A normal Nova program should **not require the JVM** merely to execute.

---

# 💜 C# / .NET Interoperability

Nova also aims to interact with the .NET ecosystem.

Potential interoperability includes:

```text
.NET libraries
C# components
Native interfaces
COM where appropriate
Generated bindings
```

This can provide access to mature application-development and GUI ecosystems.

---

# 📦 Nova Package Manager

Nova will have its own package ecosystem.

The package manager is called:

```text
nva
```

Example:

```text
nva install numpy
```

Other examples:

```text
nva install requests
nva install opencv
nva install torch
nva install nova-gui
nva install nova-ai
```

Typical package-management operations include:

```text
nva install <package>
nva remove <package>
nva update
nva upgrade
nva search <package>
nva info <package>
nva list
nva publish
```

---

# 📄 Nova Project Files

Nova source files use:

```text
.nova
```

Examples:

```text
main.nova
game.nova
ai_model.nova
application.nova
```

Project configuration uses:

```text
nova.toml
```

Dependency locking can use:

```text
nova.lock
```

---

# 📚 Standard Library

Nova aims to provide a broad standard library.

### Core

```text
nova.core
nova.string
nova.collections
nova.math
```

### System

```text
nova.io
nova.fs
nova.process
nova.system
nova.thread
nova.concurrent
```

### Networking

```text
nova.net
nova.http
nova.crypto
nova.json
```

### Application

```text
nova.gui
nova.graphics
nova.audio
```

### AI

```text
nova.ai
nova.ml
nova.tensor
nova.nn
nova.data
```

### Game Development

```text
nova.game
nova.physics
nova.input
```

---

# 🌍 Cross-Platform

Nova is designed as a cross-platform language.

Primary targets:

```text
Windows
Linux
macOS
```

Potential future targets:

```text
Android
WebAssembly
ARM
Embedded systems
```

The goal is to keep the language itself platform-independent while providing
platform-specific APIs through libraries.

---

# 🛡️ Safety

Nova aims to provide safer defaults than traditional low-level languages
without removing low-level capabilities.

The language is designed around:

```text
Safe by default
        ↓
Explicit low-level control
        ↓
Explicit unsafe operations
```

Potential safety mechanisms include:

* Strong typing
* Bounds checking where appropriate
* Null/optional handling
* Resource management
* Ownership concepts
* Safe collections
* Explicit unsafe operations

---

# 🔐 Package Security

Nova's package ecosystem is intended to support modern dependency security.

Potential features include:

* Package hashes
* Lock files
* Dependency verification
* Package signing
* Version constraints
* Reproducible dependencies
* Security metadata

---

# 🧩 Modules

Nova uses a simple import system.

```nova
import math
import nova.gui
import nova.ai
```

Specific functionality can be imported:

```nova
from nova.graphics import Renderer
```

Local modules can be organized naturally:

```text
project/
│
├── main.nova
│
├── player.nova
├── world.nova
└── physics.nova
```

---

# 🖥️ Example Nova Program

```nova
import math

class Player:

    name: str
    health: int

    def __init__(self, name: str):
        self.name = name
        self.health = 100

    def damage(self, amount: int):

        self.health -= amount

        if self.health <= 0:
            print(self.name + " defeated")


player = Player("Nova")

player.damage(25)

print("Health:", player.health)
```

The goal is for this code to remain:

* Readable like Python
* Structured like modern C#
* Strongly typed when required
* Capable of native compilation
* Extendable into low-level programming

---

# 📊 Nova vs Other Languages

| Feature            | Python |   C++ |  Java |    C# | **Nova** |
| ------------------ | -----: | ----: | ----: | ----: | -------: |
| Easy syntax        |  ⭐⭐⭐⭐⭐ |    ⭐⭐ |   ⭐⭐⭐ |  ⭐⭐⭐⭐ |    ⭐⭐⭐⭐⭐ |
| Native performance |     ⭐⭐ | ⭐⭐⭐⭐⭐ |  ⭐⭐⭐⭐ |  ⭐⭐⭐⭐ |    ⭐⭐⭐⭐⭐ |
| Low-level control  |      ⭐ | ⭐⭐⭐⭐⭐ |    ⭐⭐ |    ⭐⭐ |    ⭐⭐⭐⭐⭐ |
| Memory control     |     ⭐⭐ | ⭐⭐⭐⭐⭐ |    ⭐⭐ |   ⭐⭐⭐ |    ⭐⭐⭐⭐⭐ |
| AI ecosystem       |  ⭐⭐⭐⭐⭐ |   ⭐⭐⭐ |    ⭐⭐ |    ⭐⭐ |   ⭐⭐⭐⭐⭐* |
| GUI development    |    ⭐⭐⭐ |   ⭐⭐⭐ |  ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |   ⭐⭐⭐⭐⭐* |
| Game development   |    ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |   ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |   ⭐⭐⭐⭐⭐* |
| Cross-platform     |  ⭐⭐⭐⭐⭐ |  ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |  ⭐⭐⭐⭐ |    ⭐⭐⭐⭐⭐ |
| C/C++ interop      |    ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |   ⭐⭐⭐ |   ⭐⭐⭐ |   ⭐⭐⭐⭐⭐* |
| Python ecosystem   |  ⭐⭐⭐⭐⭐ |    ⭐⭐ |    ⭐⭐ |    ⭐⭐ |   ⭐⭐⭐⭐⭐* |
| Type safety        |     ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |    ⭐⭐⭐⭐⭐ |
| Beginner friendly  |  ⭐⭐⭐⭐⭐ |    ⭐⭐ |   ⭐⭐⭐ |  ⭐⭐⭐⭐ |    ⭐⭐⭐⭐⭐ |

`*` = long-term goal / dependent on ecosystem maturity.

---

# ✅ Advantages

## 🐍 Python-like simplicity

Nova code is designed to be clean and readable.

```nova
print("Hello")
```

instead of requiring large amounts of boilerplate.

---

## ⚡ Native performance

Nova is designed for compiled native execution.

This makes it suitable for applications where Python alone may not provide
enough performance.

---

## 🧠 Powerful memory control

Developers can remain at a high level or move down to explicit memory
management when required.

---

## 🤖 AI-friendly

Nova is designed to access the enormous Python AI ecosystem while developing
its own native AI capabilities.

---

## 🎮 Game-development friendly

Native performance, graphics APIs, memory control and multithreading make
Nova suitable for game-development workloads.

---

## 🎨 GUI-friendly

Nova aims to make modern desktop applications significantly easier to write
than traditional C++ GUI applications.

---

## 🔌 Interoperability

Nova is designed to coexist with existing ecosystems rather than isolate
developers from them.

Target ecosystems include:

```text
C
C++
Python
Java
.NET / C#
```

---

## 🌍 Cross-platform

The language is designed to target multiple operating systems without forcing
developers to rewrite the language itself for each platform.

---

# ❌ Disadvantages & Trade-offs

Nova is ambitious, and that creates real trade-offs.

## 1. Complexity

Combining high-level productivity with low-level control is difficult.

A language capable of both:

```text
print("Hello")
```

and:

```text
unsafe:
    ptr = allocate<int>()
```

must carefully define how these systems interact.

---

## 2. Smaller ecosystem

Established languages already have enormous ecosystems.

Python, C++, Java and C# have decades of libraries, documentation and
community knowledge.

Nova will initially have far fewer resources.

---

## 3. Python compatibility is not free

Supporting Python packages does not automatically make Nova equivalent to
Python.

Some packages depend heavily on:

* Python internals
* CPython behavior
* dynamic runtime features
* Python-specific extension systems

Compatibility therefore needs a dedicated interoperability layer.

---

## 4. Compiler complexity

A language that combines:

* static typing
* type inference
* generics
* memory control
* native compilation
* Python interoperability
* C++ interoperability
* concurrency

requires a significantly more sophisticated compiler than a basic scripting
language.

---

## 5. Learning advanced Nova

Basic Nova should be easy.

Advanced Nova will not necessarily be easy.

Developers who use:

* pointers
* memory allocators
* unsafe code
* concurrency
* native APIs
* GPU programming

will still need to understand advanced computer-science concepts.

---

## 6. No language can automatically be the fastest

Nova being compiled does not automatically make every Nova program faster
than C++.

Good algorithms, efficient memory access, compiler optimization and proper
architecture still matter.

---

# 🎯 Ideal Use Cases

Nova is intended to be useful across a wide range of applications.

### 🤖 Artificial Intelligence

```text
Machine Learning
Deep Learning
Computer Vision
Data Science
AI Applications
Scientific Computing
```

### 🎮 Game Development

```text
Game Engines
3D Games
2D Games
Physics
Rendering
Game Tools
Simulations
```

### 🖥️ Desktop Applications

```text
GUI Applications
Editors
Development Tools
Media Applications
Utilities
```

### ⚡ High-Performance Software

```text
Servers
Simulations
Rendering
Data Processing
Networking
Scientific Applications
```

### 🔧 Systems Programming

```text
System Utilities
Native Applications
Hardware Interfaces
Operating-System Components
Embedded Software
```

### 🌐 General Software

```text
Automation
Networking
APIs
CLI Tools
Applications
Libraries
```

---

# 🌟 Long-Term Vision

Nova's ultimate goal is not simply to become:

> "Python but faster."

Nor:

> "C++ with Python syntax."

The vision is much broader.

Nova aims to become a unified language where developers can move naturally
between abstraction levels.

```text
                 NOVA
                   │
        ┌──────────┼──────────┐
        │          │          │
     Simple      Native       AI
     Apps       Software    Computing
        │          │          │
        ├──────────┼──────────┤
        │          │          │
       GUI       Games      Systems
        │          │          │
        └──────────┼──────────┘
                   │
             Low-Level Code
```

The programmer chooses the level of control.

---

# 🌌 The Nova Goal

Nova wants to make this possible:

```nova
# Simple application

print("Hello Nova!")
```

while still allowing advanced software to reach:

```text
Native CPU
    ↓
Memory
    ↓
Threads
    ↓
GPU
    ↓
Operating System
    ↓
Hardware
```

without forcing every developer to learn low-level programming first.

---

# 🧭 Design Principles

Nova follows these principles:

1. **Readable code**
2. **Simple syntax**
3. **Native performance**
4. **Strong typing**
5. **Type inference**
6. **Memory control**
7. **Safe defaults**
8. **Low-level access when required**
9. **Python interoperability**
10. **C/C++ interoperability**
11. **Modern GUI development**
12. **First-class AI ecosystem**
13. **Game-development support**
14. **Cross-platform design**
15. **Modern concurrency**
16. **Powerful package management**
17. **Excellent developer tooling**
18. **Minimal unnecessary boilerplate**

---

# 🔭 Future Possibilities

Nova's long-term ecosystem may eventually include:

```text
Nova Compiler
Nova Runtime
Nova Standard Library
Nova Package Registry
Nova IDE
Nova Language Server
Nova Debugger
Nova Formatter
Nova Profiler
Nova AI Framework
Nova GUI Framework
Nova Game Framework
Nova Engine
```

---

# 🪐 Nova in One Sentence

> **Nova is a Python-inspired native programming language designed to combine
> Python's simplicity, C++'s performance and memory control, Java's
> portability, and C#'s application-development strengths with first-class
> AI and modern systems programming capabilities.**

---

<p align="center">

### 🌌 Write simple. Run native. Control everything.

**Nova — One language. Multiple levels of power.**

</p>

---

# 💙 Special Thanks

Nova is an independent programming-language project, but the idea and its
development journey were greatly supported by AI-assisted research,
discussion, and coding.

### 🐋 DeepSeek

<p align="center">
  <a href="https://www.deepseek.com/">
    <img src="https://www.deepseek.com/favicon.ico" alt="DeepSeek" width="80">
  </a>
</p>

A **special and major thank you to DeepSeek** for providing substantial
assistance throughout the development of the Nova concept.

DeepSeek was especially valuable during:

- 🧠 Programming and architecture discussions
- 💻 Writing and refining code
- 🔧 Debugging and problem solving
- 🏗️ Compiler and language-design discussions
- 📚 Exploring technical concepts
- 🚀 Turning ideas into practical implementations

A large part of the technical exploration and coding assistance behind Nova
was made possible with the help of DeepSeek.

**Thank you, DeepSeek, for being a major part of the journey. ❤️**

[Visit DeepSeek →](https://www.deepseek.com/)

---

### 🤖 ChatGPT

<p align="center">
  <a href="https://chatgpt.com/">
    <img src="https://upload.wikimedia.org/wikipedia/commons/0/04/ChatGPT_logo.svg" alt="OpenAI" width="80">
  </a>
</p>

A special thank you to **ChatGPT by OpenAI** for helping with:

- 💡 Confirming ideas and concepts
- 🧩 Language-design discussions
- 📝 Documentation and README refinement
- 🔍 Reviewing concepts
- 💭 Brainstorming and exploring possibilities

ChatGPT played a smaller supporting role in the development process, mainly
helping with confirmation, refinement, and additional perspectives.

**Thank you, ChatGPT, for being part of the Nova journey. ❤️**

[Visit ChatGPT →](https://chatgpt.com/)

---

<p align="center">

### 🌌 Built from an idea. Refined with AI. Created with ambition.

**Nova**

*Python simplicity · Native performance · Low-level control · AI-ready*

</p>

---

> **Disclaimer:** DeepSeek and OpenAI/ChatGPT are acknowledged as AI tools
> used during the development of this project. This acknowledgement does not
> imply sponsorship, partnership, endorsement, or affiliation with either
> organization.