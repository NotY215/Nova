# Vayu Language Syntax

::: {align="center"}
# 🌌 Vayu Syntax

### A practical reference for the currently implemented Vayu language.

**Python-inspired syntax · Static type checking · Native-language
foundations**
:::

------------------------------------------------------------------------

## 📌 About This Document

This document describes the syntax currently demonstrated by the Vayu
examples and compiler behavior.

It focuses on the language features that are currently implemented and
tested, rather than describing planned or future features.

For other parts of the Vayu project:

-   [📦 Package System](package.md)
-   [🤖 AI & ML](ai.md)
-   [🔭 Future Vision](vision.md)

------------------------------------------------------------------------

# 1. Source Files

Vayu source files use the:

``` text
.Vayu
```

extension.

Example:

``` text
hello.Vayu
control.Vayu
fib.Vayu
classes.Vayu
```

Vayu source code is indentation-based, using a Python-inspired block
syntax.

------------------------------------------------------------------------

# 2. Comments

Single-line comments begin with `#`.

``` Vayu
# This is a comment

name = "NotY"  # This is also a comment
```

Comments are ignored by the compiler.

------------------------------------------------------------------------

# 3. Variables

Variables are created using assignment.

``` Vayu
name = "NotY"
age = 19
score = 100
```

Vayu supports type inference:

``` Vayu
x = 5
y = 3.14
name = "Vayu"
flag = true
```

The compiler determines the type from the assigned value.

------------------------------------------------------------------------

# 4. Explicit Type Annotations

Variables can optionally specify their type.

``` Vayu
x: int = 10
y: float = 2.5
name: str = "Vayu"
flag: bool = true
```

The type checker verifies that the assigned value is compatible with the
declared type.

For example:

``` Vayu
x: int = "hello"
```

produces a type error because a `str` cannot be assigned to an `int`.

------------------------------------------------------------------------

# 5. Built-in Types

The currently demonstrated type system includes:

  Type                     Example
  ------------------------ ----------------------
  `int`                    `42`
  `float`                  `3.14`
  `bool`                   `true`, `false`
  `str`                    `"Hello"`
  `list<T>`                `[1, 2, 3]`
  Struct/Class types       `Player`
  `None` / `None` return   `return` / `-> None`

Generic collection types can be written using angle brackets:

``` Vayu
list<int>
```

Example:

``` Vayu
def total_of(lst: list<int>) -> int:
    ...
```

------------------------------------------------------------------------

# 6. Boolean Values

Vayu uses:

``` Vayu
true
false
```

Example:

``` Vayu
active = true
finished = false
```

Boolean expressions can be created using comparisons and logical
operators.

``` Vayu
check = age >= 18
ok = active and check
```

------------------------------------------------------------------------

# 7. Strings

Strings use double quotes.

``` Vayu
name = "Vayu"
message = "Hello, World"
```

Strings can be concatenated with `+`.

``` Vayu
greeting = "Hello, " + name
```

Conversion to a string can be performed with `str()`.

``` Vayu
age = 19
print("Age: " + str(age))
```

------------------------------------------------------------------------

# 8. Printing

The built-in `print()` function outputs values.

``` Vayu
print("Hello, Vayu!")
print(42)
print(name)
```

Multiple values can be passed:

``` Vayu
print("sum:", total)
```

------------------------------------------------------------------------

# 9. Functions

Functions use the `def` keyword.

``` Vayu
def greet(name: str) -> str:
    return "Hello, " + name
```

Functions can be called normally:

``` Vayu
message = greet("Vayu")
print(message)
```

------------------------------------------------------------------------

## 9.1 Function Parameters

Parameters can have explicit types.

``` Vayu
def add(a: int, b: int) -> int:
    return a + b
```

Multiple parameters are separated by commas.

``` Vayu
def scale(value: float, factor: float) -> float:
    return value * factor
```

------------------------------------------------------------------------

## 9.2 Return Types

Return types are specified after `->`.

``` Vayu
def square(x: int) -> int:
    return x * x
```

Functions that do not return a value can use `None`:

``` Vayu
def greet(name: str) -> None:
    print("Hello, " + name)
```

A bare `return` is also supported:

``` Vayu
def noop():
    return
```

------------------------------------------------------------------------

## 9.3 Recursion

Functions can call themselves.

``` Vayu
def fib(n: int) -> int:
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)
```

------------------------------------------------------------------------

# 10. Conditions

Vayu uses `if`, `elif`, and `else`.

``` Vayu
if x < 0:
    print("negative")
elif x == 0:
    print("zero")
elif x < 10:
    print("small")
else:
    print("big")
```

Blocks are defined through indentation.

------------------------------------------------------------------------

# 11. While Loops

`while` repeats a block while its condition is true.

``` Vayu
x = 5

while x > 0:
    print(x)
    x = x - 1
```

Output:

``` text
5
4
3
2
1
```

------------------------------------------------------------------------

# 12. For Loops

Vayu supports iteration using `for`.

``` Vayu
for x in [10, 20, 30]:
    print(x)
```

Iteration over a variable is also supported:

``` Vayu
nums = [1, 2, 3, 4, 5]

for n in nums:
    print(n)
```

------------------------------------------------------------------------

## 12.1 Range

The built-in `range()` function can generate integer sequences.

``` Vayu
for i in range(5):
    print(i)
```

Produces:

``` text
0
1
2
3
4
```

A start and end value can be supplied:

``` Vayu
for i in range(2, 8):
    print(i)
```

A step can also be supplied:

``` Vayu
for i in range(0, 10, 2):
    print(i)
```

------------------------------------------------------------------------

# 13. Break

`break` immediately exits the current loop.

``` Vayu
for n in [1, 2, 3, 4, 5]:
    if n == 4:
        break
    print(n)
```

------------------------------------------------------------------------

# 14. Continue

`continue` skips the remainder of the current iteration.

``` Vayu
for n in [1, 2, 3, 4, 5]:
    if n % 2 == 0:
        continue
    print(n)
```

------------------------------------------------------------------------

# 15. Arithmetic Operators

Vayu currently demonstrates the following arithmetic operators:

  Operator   Meaning                  Example
  ---------- ------------------------ ----------------
  `+`        Addition                 `a + b`
  `-`        Subtraction              `a - b`
  `*`        Multiplication           `a * b`
  `/`        Division                 `a / b`
  `//`       Integer/floor division   `a // b`
  `%`        Modulo                   `a % b`
  `**`       Exponentiation           `a ** b`
  `-`        Unary negative           `-x`

Examples:

``` vyu
print(1 + 2)
print(10 - 3)
print(4 * 5)
print(7 / 2)
print(7 // 2)
print(7 % 3)
print(2 ** 10)
print(-5)
print(- -5)
```

Output:

``` text
3
7
20
3.5
3
1
1024
-5
5
```

### 15.1 Arithmetic Assignment Through Expressions

Variables can be updated by assigning a new expression to the same
variable:

``` vyu
a = 10
a = a + 5
print(a)
```

Output:

``` text
15
```

### 15.2 Unary Negative

The `-` operator can be used as a unary operator to negate a value.
Multiple unary negatives are supported:

``` vyu
print(-5)
print(- -5)
```

The second expression evaluates to `5`.

### 15.3 Exponentiation Associativity

Exponentiation is evaluated right-to-left:

``` vyu
print(2 ** 3 ** 2)
```

Output:

``` text
512
```

This corresponds to:

``` text
2 ** (3 ** 2)
```

rather than:

``` text
(2 ** 3) ** 2
```

### 15.4 Expression Examples

Arithmetic expressions can combine variables, literals, operators, and
parentheses:

``` vyu
a = 10
b = 20
c = a + b
print(c)

print(1 + 2 * 3)
print((1 + 2) * 3)
```

Output:

``` text
30
7
9
```

# 16. Comparison Operators

Vayu supports:

``` text
==
!=
<
<=
>
>=
```

Example:

``` Vayu
age = 19

print(age >= 18)
print(age == 19)
print(age != 10)
```

Comparison expressions produce boolean values.

------------------------------------------------------------------------

# 17. Logical Operators

Vayu supports:

``` text
and
or
not
```

Examples:

``` Vayu
adult = age >= 18
good_score = score > 90

if adult and good_score:
    print("accepted")
```

Other examples:

``` Vayu
result = a and b
result = a or b
result = not flag
```

------------------------------------------------------------------------

# 18. Operator Precedence

Vayu follows conventional expression precedence for the currently
implemented operators. Parentheses can be used to explicitly control
evaluation order.

For example:

``` vyu
print(1 + 2 * 3)
print((1 + 2) * 3)
```

Output:

``` text
7
9
```

Multiplication is evaluated before addition, while parentheses take
priority over the normal precedence rules.

Exponentiation is right-associative:

``` vyu
print(2 ** 3 ** 2)
```

produces:

``` text
512
```

which is equivalent to `2 ** (3 ** 2)`.

Logical and comparison expressions can also be combined:

``` vyu
print(1 < 5 and 5 < 10)
print(1 > 5 or 5 < 10)
```

Output:

``` text
true
true
```

Parentheses are recommended when an expression would otherwise be
difficult to read.

# 19. Lists

Lists use square brackets.

``` Vayu
nums = [1, 2, 3, 4, 5]
```

Lists can contain strings:

``` Vayu
names = ["Alice", "Bob", "Charlie"]
```

An empty list is valid:

``` Vayu
items = []
```

------------------------------------------------------------------------

## 19.1 List Indexing

Indexing starts at zero.

``` Vayu
nums = [10, 20, 30]

print(nums[0])
print(nums[2])
```

Negative indexes are supported:

``` Vayu
print(nums[-1])
```

------------------------------------------------------------------------

## 19.2 List Assignment

Existing elements can be changed:

``` Vayu
nums[0] = 100
```

------------------------------------------------------------------------

## 19.3 List Length

Use `len()`:

``` Vayu
print(len(nums))
```

------------------------------------------------------------------------

## 19.4 Append

Add an item to the end:

``` Vayu
nums.append(6)
```

------------------------------------------------------------------------

## 19.5 Pop

Remove and return the last item:

``` Vayu
last = nums.pop()
print(last)
```

------------------------------------------------------------------------

## 19.6 Insert

Insert an item at a specified position:

``` Vayu
nums.insert(0, 100)
```

------------------------------------------------------------------------

## 19.7 List Membership

Use `in`:

``` Vayu
print(3 in nums)
```

The list also provides `contains()`:

``` Vayu
print(nums.contains(3))
```

------------------------------------------------------------------------

## 19.8 List Arithmetic

Lists can be concatenated:

``` Vayu
print([1, 2] + [3, 4])
```

Lists can be repeated:

``` Vayu
print([0] * 3)
```

------------------------------------------------------------------------

## 19.9 Nested Lists

Lists can contain other lists.

``` Vayu
grid = [[1, 2], [3, 4]]

print(grid[0][1])
```

------------------------------------------------------------------------

## 19.10 List Slicing

List slicing is **not currently implemented**.

For example, syntax such as:

``` Vayu
nums[1:4]
```

should not currently be considered part of the implemented language.

------------------------------------------------------------------------

# 20. Maps

Maps use curly braces with key/value pairs.

``` Vayu
ages = {
    "alice": 30,
    "bob": 25
}
```

Keys can be accessed using brackets:

``` Vayu
print(ages["alice"])
```

------------------------------------------------------------------------

## 20.1 Adding or Updating Map Values

``` Vayu
ages["carol"] = 40
```

------------------------------------------------------------------------

## 20.2 `put()`

A value can also be added using `put()`:

``` Vayu
ages.put("dave", 22)
```

------------------------------------------------------------------------

## 20.3 `remove()`

Remove a key:

``` Vayu
ages.remove("bob")
```

------------------------------------------------------------------------

## 20.4 Map Membership

``` Vayu
print("alice" in ages)
```

or:

``` Vayu
print(ages.contains("alice"))
```

------------------------------------------------------------------------

## 20.5 Map Keys

Retrieve the keys:

``` Vayu
keys = ages.keys()
```

------------------------------------------------------------------------

## 20.6 Map Length

``` Vayu
print(len(ages))
```

------------------------------------------------------------------------

## 20.7 Empty Maps

An empty map can be created with:

``` Vayu
empty = {}
```

------------------------------------------------------------------------

## 20.8 Iterating Over Maps

Iteration over a map produces its keys:

``` Vayu
for name in ages:
    print(name)
```

------------------------------------------------------------------------

# 21. String Indexing and Iteration

Strings can be indexed:

``` Vayu
text = "hello"

print(text[0])
print(text[-1])
```

Strings can also be iterated:

``` Vayu
for c in "abc":
    print(c)
```

------------------------------------------------------------------------

# 22. String Methods

Vayu currently demonstrates several string operations.

### `strip()`

``` Vayu
text.strip()
```

### `lower()`

``` Vayu
text.lower()
```

### `upper()`

``` Vayu
text.upper()
```

### `split()`

``` Vayu
"a,b,c".split(",")
"one two three".split()
```

### `join()`

``` Vayu
"-".join(["x", "y", "z"])
```

### `replace()`

``` Vayu
"hello".replace("l", "L")
```

### `find()`

``` Vayu
"hello world".find("world")
```

Returns `-1` when the substring is not found.

### `contains()`

``` Vayu
"hello".contains("ell")
```

### `starts_with()`

``` Vayu
"file.txt".starts_with("file")
```

### `ends_with()`

``` Vayu
"file.txt".ends_with(".txt")
```

### `is_digit()`

``` Vayu
"12345".is_digit()
```

### `is_alpha()`

``` Vayu
"abcde".is_alpha()
```

### `is_space()`

``` Vayu
"   ".is_space()
```

### `char_at()`

``` Vayu
"hello".char_at(1)
```

------------------------------------------------------------------------

# 23. Built-in Conversion Functions

Vayu currently demonstrates:

``` Vayu
str(value)
int(value)
float(value)
```

String/character utility functions include:

``` Vayu
ord("A")
chr(66)
```

Example:

``` Vayu
print(ord("A"))
print(chr(66))
print(chr(ord("A") + 1))
```

------------------------------------------------------------------------

# 24. `list()`

The `list()` function can create a list from an iterable such as a
string.

``` Vayu
print(list("abc"))
```

------------------------------------------------------------------------

# 25. Functions as Values

Functions can be stored in variables.

``` Vayu
def add(a: int, b: int) -> int:
    return a + b

operation = add
print(operation(2, 3))
```

Vayu also supports anonymous functions through `lambda`.

------------------------------------------------------------------------

# 26. Lambda Functions

A lambda is written using:

``` Vayu
lambda parameters: expression
```

Example:

``` Vayu
double = lambda x: x * 2

print(double(5))
```

Multiple parameters:

``` Vayu
add = lambda a, b: a + b

print(add(3, 4))
```

No parameters:

``` Vayu
no_args = lambda: 42

print(no_args())
```

------------------------------------------------------------------------

# 27. Closures

A lambda can capture a variable from its surrounding function.

``` Vayu
def make_adder(n: int):
    return lambda x: x + n

add5 = make_adder(5)

print(add5(1))
```

------------------------------------------------------------------------

# 28. Higher-Order Functions

Functions can receive functions as arguments and return functions.

Example:

``` Vayu
def compose(f, g):
    return lambda x: f(g(x))
```

------------------------------------------------------------------------

# 29. `map()`

`map()` applies a function to each item.

``` Vayu
nums = [1, 2, 3, 4, 5]

doubled = map(lambda x: x * 2, nums)

print(doubled)
```

------------------------------------------------------------------------

# 30. `filter()`

`filter()` keeps values for which the supplied function is true.

``` Vayu
evens = filter(lambda x: x % 2 == 0, nums)

print(evens)
```

------------------------------------------------------------------------

# 31. `reduce()`

`reduce()` combines a collection into a single value.

``` Vayu
total = reduce(
    lambda acc, x: acc + x,
    [1, 2, 3, 4, 5]
)

print(total)
```

------------------------------------------------------------------------

# 32. `sorted()`

Collections can be sorted:

``` Vayu
sorted_values = sorted([3, 1, 4, 1, 5])

print(sorted_values)
```

A key function can be supplied:

``` Vayu
people = ["Charlie", "alice", "Bob"]

result = sorted(people, lambda s: s.lower())
```

------------------------------------------------------------------------

# 33. `any()` and `all()`

`any()` returns whether at least one value is truthy.

``` Vayu
print(any([false, false, true]))
```

`all()` returns whether every value is truthy.

``` Vayu
print(all([true, true, true]))
```

------------------------------------------------------------------------

# 34. `sum()`

`sum()` adds values in a collection.

``` Vayu
print(sum([1, 2, 3, 4, 5]))
```

It also works with floating-point values:

``` Vayu
print(sum([1.5, 2.5, 3.0]))
```

------------------------------------------------------------------------

# 35. Structs

Vayu supports `struct` declarations.

``` Vayu
struct Player:
    name: str
    health: int
    score: int
```

A struct defines named fields with types.

------------------------------------------------------------------------

## 35.1 Creating Structs

Keyword arguments:

``` Vayu
p = Player(
    name="NotY",
    health=100,
    score=0
)
```

Positional arguments:

``` Vayu
q = Player("Ally", 80, 10)
```

------------------------------------------------------------------------

## 35.2 Accessing Fields

Use member access:

``` Vayu
print(p.name)
print(p.health)
```

------------------------------------------------------------------------

## 35.3 Modifying Fields

``` Vayu
p.health = p.health - 25
```

------------------------------------------------------------------------

## 35.4 Nested Structs

Structs can contain other struct types.

``` Vayu
struct Team:
    leader: Player
    size: int

team = Team(
    leader=p,
    size=3
)
```

Nested fields can be accessed:

``` Vayu
print(team.leader.name)
print(team.leader.health)
```

------------------------------------------------------------------------

## 35.5 Struct Identity

A nested struct field can refer to the same object.

``` Vayu
team.leader.health = 42

print(p.health)
```

------------------------------------------------------------------------

## 35.6 Printing Structs

Struct instances can be printed.

Example representation:

``` text
Player(name="NotY", health=42, score=0)
```

------------------------------------------------------------------------

# 36. Classes

Vayu supports classes with fields and methods.

``` Vayu
class Player:
    name: str
    health: int
```

Classes can define an initializer using `__init__`.

``` Vayu
class Player:
    name: str
    health: int

    def __init__(self, name: str) -> None:
        self.name = name
        self.health = 100
```

------------------------------------------------------------------------

# 37. `self`

Instance methods use `self` to access the current object.

``` Vayu
def damage(self, amount: int) -> None:
    self.health = self.health - amount
```

Fields can be accessed with:

``` Vayu
self.name
self.health
```

------------------------------------------------------------------------

# 38. Instance Methods

Classes can define methods.

``` Vayu
class Counter:
    value: int

    def __init__(self) -> None:
        self.value = 0

    def inc(self) -> None:
        self.value = self.value + 1
```

Create an instance:

``` Vayu
counter = Counter()

counter.inc()
```

------------------------------------------------------------------------

# 39. Methods Calling Other Methods

An instance method can call another method on the same object.

``` Vayu
def inc_n(self, n: int) -> None:
    i = 0

    while i < n:
        self.inc()
        i = i + 1
```

------------------------------------------------------------------------

# 40. Inheritance

A class can inherit from another class.

``` Vayu
class Warrior(Player):
    rage: int
```

The derived class receives members and behavior from the base class.

------------------------------------------------------------------------

# 41. `super()`

A derived class can call the base-class implementation using `super()`.

``` Vayu
def __init__(self, name: str) -> None:
    super().__init__(name)
    self.rage = 0
```

Methods can also call a parent implementation:

``` Vayu
def damage(self, amount: int) -> None:
    super().damage(amount)
    self.rage = self.rage + amount
```

------------------------------------------------------------------------

# 42. Method Overriding

A subclass can replace a method inherited from its parent.

``` Vayu
class Warrior(Player):

    def describe(self) -> str:
        return super().describe() + " [rage " + str(self.rage) + "]"
```

------------------------------------------------------------------------

# 43. Multi-Level Inheritance

Inheritance can continue through multiple levels.

``` Vayu
class Mage(Player):
    mana: int
```

Then:

``` Vayu
class Archmage(Mage):
    def cast(self) -> str:
        return self.name + " casts a spell"
```

------------------------------------------------------------------------

# 44. Member Access

The `.` operator accesses fields, methods and module members.

Examples:

``` Vayu
player.health
player.damage(20)

math.pi

math_helpers.square(5)

obj.method(x).field
```

------------------------------------------------------------------------

# 45. Function Calls

Function calls use parentheses.

``` Vayu
print("Hello")
square(5)
add(2, 3)
```

Nested calls are supported:

``` Vayu
print(greet(name))
```

------------------------------------------------------------------------

# 46. Keyword Arguments

Struct construction supports named arguments.

``` Vayu
player = Player(
    name="NotY",
    health=100,
    score=0
)
```

Keyword-style arguments can also be used when calling supported
functions.

------------------------------------------------------------------------

# 47. Exception Handling

Vayu supports exception handling with:

``` text
try
except
finally
raise
```

Basic example:

``` Vayu
try:
    print("before")
    raise ValueError("something went wrong")
except ValueError as e:
    print("caught:", e.message)
```

------------------------------------------------------------------------

# 48. Raising Exceptions

Exceptions can be raised using `raise`.

``` Vayu
raise ValueError("bad value")
```

Other demonstrated exception types include:

``` Vayu
ValueError
TypeError
RuntimeError
ZeroDivisionError
Exception
```

------------------------------------------------------------------------

# 49. Exception Variables

An exception can be captured with `as`.

``` Vayu
except ValueError as e:
    print(e.message)
```

The exception's message can be accessed through:

``` Vayu
e.message
```

------------------------------------------------------------------------

# 50. Multiple Exception Handlers

Multiple `except` blocks can handle different exception types.

``` Vayu
try:
    risky()
except ValueError as e:
    print("ValueError:", e.message)
except TypeError as e:
    print("TypeError:", e.message)
except RuntimeError as e:
    print("RuntimeError:", e.message)
```

------------------------------------------------------------------------

# 51. Bare `except`

A bare `except` can catch an exception without specifying a type.

``` Vayu
try:
    raise ValueError("something went wrong")
except:
    print("caught")
```

------------------------------------------------------------------------

# 52. Base Exception Handling

`Exception` can be used to catch exceptions through the common base
type.

``` Vayu
try:
    raise ValueError("specific")
except Exception as e:
    print(e.message)
```

------------------------------------------------------------------------

# 53. Finally

`finally` runs after a `try` block and its exception handling.

``` Vayu
try:
    print("doing work")
finally:
    print("cleanup")
```

It also runs when an exception is handled:

``` Vayu
try:
    print("try block")
    raise ValueError("oops")
except ValueError:
    print("handler")
finally:
    print("finally always runs")
```

------------------------------------------------------------------------

# 54. Nested Exceptions

Exception handling can be nested.

``` Vayu
try:
    try:
        raise ValueError("inner")
    finally:
        print("inner finally")
except ValueError:
    print("outer caught")
```

------------------------------------------------------------------------

# 55. Re-Raising

An exception can be re-raised from an exception handler.

``` Vayu
def reraise() -> None:
    try:
        raise ValueError("original")
    except ValueError:
        print("first catch")
        raise
```

------------------------------------------------------------------------

# 56. Raising Another Exception

An exception handler can raise another exception.

``` Vayu
try:
    try:
        raise ValueError("inner")
    except ValueError:
        raise TypeError("from inner")
except TypeError as e:
    print(e.message)
```

------------------------------------------------------------------------

# 57. Raising a String

The current implementation also demonstrates raising a plain string:

``` Vayu
try:
    raise "plain string error"
except Exception as e:
    print(e.message)
```

This behavior is part of the currently demonstrated runtime behavior.

------------------------------------------------------------------------

# 58. Modules

Vayu supports importing other `.Vayu` modules.

Example project:

``` text
examples/
└── modules/
    ├── main.Vayu
    ├── math_helpers.Vayu
    └── user.Vayu
```

------------------------------------------------------------------------

# 59. Importing a Module

A module can be imported by name:

``` Vayu
import math_helpers
```

Members can then be accessed through the module:

``` Vayu
print(math_helpers.square(5))
print(math_helpers.PI)
```

------------------------------------------------------------------------

# 60. Import Aliases

Modules can have aliases:

``` Vayu
import math_helpers as mh
```

Then:

``` Vayu
print(mh.square(6))
```

------------------------------------------------------------------------

# 61. Importing Specific Members

Specific functions or values can be imported:

``` Vayu
from math_helpers import square, cube
```

They can then be used directly:

``` Vayu
print(square(5))
print(cube(3))
```

------------------------------------------------------------------------

# 62. Import Aliases for Members

Imported members can also have aliases.

``` Vayu
from math_helpers import square, cube as cb
```

Then:

``` Vayu
print(square(5))
print(cb(3))
```

------------------------------------------------------------------------

# 63. Module Constants

Modules can contain top-level values.

``` Vayu
PI = 3.14159
```

They can be accessed through the module:

``` Vayu
print(math_helpers.PI)
```

------------------------------------------------------------------------

# 64. Module Functions

Modules can contain functions:

``` Vayu
def square(x: int) -> int:
    return x * x
```

------------------------------------------------------------------------

# 65. Module Structs

Modules can contain structs:

``` Vayu
struct User:
    name: str
    age: int
```

They can be returned and used by other modules.

``` Vayu
def make_user(name: str, age: int) -> User:
    return User(name=name, age=age)
```

------------------------------------------------------------------------

# 66. Chained Module Access

Module and member access can be combined.

``` Vayu
Vayu.ai.Tensor(1, 2)
```

------------------------------------------------------------------------

# 67. Math Standard Library

The demonstrated `math` module provides mathematical functionality.

Examples:

``` Vayu
math.sqrt(16)
math.floor(3.7)
math.ceil(3.2)

math.sin(0)
math.cos(0)

math.exp(0)
math.log(math.e)
math.log10(100)
math.log2(8)
```

Constants:

``` Vayu
math.pi
math.e
```

------------------------------------------------------------------------

# 68. Absolute Value

`abs()` returns the absolute value.

``` Vayu
print(abs(-5))
```

------------------------------------------------------------------------

# 69. Expression Statements

Vayu allows expressions to appear as standalone statements.

Examples:

``` Vayu
1 + 2 * 3
foo(1, 2, 3)
obj.method(x)
```

------------------------------------------------------------------------

# 70. Attribute Chains

Member access can be chained:

``` Vayu
x.y.z
```

Method calls can be chained with member access:

``` Vayu
obj.method(x).field
```

------------------------------------------------------------------------

# 71. Indexing Expressions

Values can be indexed using square brackets:

``` Vayu
arr[0]
```

Nested indexing is possible:

``` Vayu
matrix[0][1]
```

------------------------------------------------------------------------

# 72. Type Promotion

The current type checker demonstrates integer-to-floating-point
promotion.

``` Vayu
a: int = 10
b: float = 2.5

result = a + b
```

The resulting arithmetic expression is treated as a floating-point
value.

Function arguments can also use this promotion:

``` Vayu
def scale(v: float, k: float) -> float:
    return v * k

result = scale(1, 2.0)
```

Here the integer argument can be promoted to `float`.

------------------------------------------------------------------------

# 73. Generic Collection Types

Generic type notation is supported for demonstrated collection types.

``` Vayu
list<int>
```

Example:

``` Vayu
def total_of(values: list<int>) -> int:
    total = 0

    for value in values:
        total = total + value

    return total
```

------------------------------------------------------------------------

# 74. Type Checking

Vayu provides a type-checking mode.

The compiler can check a source file without executing it:

``` text
vayuc examples/types.Vayu --check
```

A successful check reports:

``` text
OK: examples/types.Vayu type-checks successfully.
```

An invalid assignment such as:

``` Vayu
x: int = "hello"
```

is rejected by the type checker.

------------------------------------------------------------------------

# 75. AST Representation

Vayu's compiler can expose the parsed Abstract Syntax Tree.

For example, a simple program containing:

``` Vayu
name = "NotY"
age = 19

def greet(who: str) -> str:
    return "Hello, " + who

if age >= 18:
    print(greet(name))
else:
    print("Minor")
```

is represented using nodes such as:

``` text
Program
├── Assign
├── Assign
├── Def greet(who: str) -> str
└── If
```

The AST contains nodes for assignments, names, literals, functions,
returns, binary expressions, calls and conditional branches.

------------------------------------------------------------------------

# 76. Example: Hello World

A complete small Vayu program:

``` Vayu
name = "NotY"
age = 19

def greet(who: str) -> str:
    return "Hello, " + who

if age >= 18:
    print(greet(name))
else:
    print("Minor")
```

------------------------------------------------------------------------

# 77. Example: Control Flow

``` Vayu
x = 5

if x < 0:
    print("negative")
elif x == 0:
    print("zero")
elif x < 10:
    print("small")
else:
    print("big")

while x > 0:
    print(x)
    x = x - 1

print("done")
```

------------------------------------------------------------------------

# 78. Example: Recursion

``` Vayu
def fib(n: int) -> int:
    if n < 2:
        return n

    return fib(n - 1) + fib(n - 2)

i = 0

while i < 10:
    print("fib(" + str(i) + ") =", fib(i))
    i = i + 1
```

------------------------------------------------------------------------

# 79. Example: Struct + Function

``` Vayu
struct Player:
    name: str
    health: int
    score: int

def make_player(name: str) -> Player:
    return Player(
        name=name,
        health=100,
        score=0
    )

player = make_player("NotY")

print(player.name)
print(player.health)
```

------------------------------------------------------------------------

# 80. Example: Class + Inheritance

``` Vayu
class Player:
    name: str
    health: int

    def __init__(self, name: str) -> None:
        self.name = name
        self.health = 100

    def damage(self, amount: int) -> None:
        self.health = self.health - amount

    def describe(self) -> str:
        return self.name + " has " + str(self.health) + " hp"


class Warrior(Player):
    rage: int

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.rage = 0

    def damage(self, amount: int) -> None:
        super().damage(amount)
        self.rage = self.rage + amount

    def describe(self) -> str:
        return super().describe() + " [rage " + str(self.rage) + "]"


warrior = Warrior("Brutus")

warrior.damage(30)
warrior.damage(15)

print(warrior.describe())
```

------------------------------------------------------------------------

# 81. Example: Lambda + Collections

``` Vayu
nums = [1, 2, 3, 4, 5]

doubled = map(lambda x: x * 2, nums)

evens = filter(lambda x: x % 2 == 0, nums)

total = reduce(
    lambda a, b: a + b,
    nums
)

print(doubled)
print(evens)
print(total)
```

------------------------------------------------------------------------

# 82. Example: Modules

### `math_helpers.Vayu`

``` Vayu
PI = 3.14159

def square(x: int) -> int:
    return x * x

def cube(x: int) -> int:
    return x * x * x

def greet(name: str) -> str:
    return "Hello, " + name + "!"
```

### `main.Vayu`

``` Vayu
import math_helpers
import math_helpers as mh

from math_helpers import square, cube as cb, greet

print(math_helpers.square(5))
print(mh.square(6))
print(square(7))
print(cb(3))
print(greet("Vayu"))
```

------------------------------------------------------------------------

# 83. Current Syntax Summary

  Area                       Current Support
  -------------------------- ------------------------------
  Variables                  ✅
  Type inference             ✅
  Type annotations           ✅
  `int`                      ✅
  `float`                    ✅
  `bool`                     ✅
  `str`                      ✅
  Lists                      ✅
  Maps                       ✅
  Nested collections         ✅
  Indexing                   ✅
  Negative indexing          ✅
  List slicing               ❌ Not currently implemented
  Functions                  ✅
  Typed parameters           ✅
  Return types               ✅
  Recursion                  ✅
  `if / elif / else`         ✅
  `while`                    ✅
  `for`                      ✅
  `break`                    ✅
  `continue`                 ✅
  `range()`                  ✅
  Arithmetic                 ✅
  Comparisons                ✅
  Logical operators          ✅
  Structs                    ✅
  Classes                    ✅
  Constructors               ✅
  Instance methods           ✅
  Inheritance                ✅
  Method overriding          ✅
  `super()`                  ✅
  Lambdas                    ✅
  Closures                   ✅
  `map()`                    ✅
  `filter()`                 ✅
  `reduce()`                 ✅
  `sorted()`                 ✅
  `any()` / `all()`          ✅
  `sum()`                    ✅
  Exceptions                 ✅
  `try / except / finally`   ✅
  `raise`                    ✅
  Re-raise                   ✅
  Modules                    ✅
  Import aliases             ✅
  `from ... import ...`      ✅
  Math library               ✅
  Type checking              ✅
  AST dumping                ✅

------------------------------------------------------------------------

# 🌌 Vayu Syntax Philosophy

Vayu's syntax is intentionally designed to remain readable while
providing access to increasingly powerful language features.

The core style can be summarized as:

``` text
Readable blocks
      +
Python-inspired syntax
      +
Optional explicit typing
      +
Structured data
      +
Object-oriented programming
      +
Functional programming
      +
Native-language foundations
```

Vayu is intended to evolve without sacrificing the readability that
makes the language approachable.

------------------------------------------------------------------------

::: {align="center"}
# 🌌 Vayu

### Python-inspired syntax. Native ambition. One language.

**[← Back to README](../README.md)**

[📦 Packages](package.md) · [🤖 AI & ML](ai.md) · [🔭 Vision](vision.md)
:::
