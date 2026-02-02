# Overview

## Build

From the root:

```shell
make
```

## Style and Best Practices

This defines some style and best practices to use for writing C++.

### Almost Always Auto + Brace Initialization

By example. Instead of:

```c++
int a = 10;
Foo foo;
Bar bar(1, 2, 3);
std::vector<int> baz(2, 5);
```

use:

```c++
auto a = 10;
auto foo = Foo{};
auto bar = Bar{1, 2, 3};
auto baz = std::vector<int>(2, 5);
```

The goal is to reduce repeating types, avoid most-vexing parse, and simplify code.