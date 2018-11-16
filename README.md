# interface
`interface` does the exact same thing as go's interface in c++.
Its syntax is

````c++
INTERFACE(signature 0, method name 0, signature 1, method name 1, ...)
````

which is an anonymous type that holds anything with methods matching the specified name and sufficiently similar signature.
The rules of similarity follows that of `std::function`.

`using I = INTERFACE(sig0, id0, sig1, id1, ...);` is equivalent to the go construct

````go
type I interface {
  id0 sig0
  id1 sig1
  ...
}
````

## Installation

`interface` is a header only library. Just `#include "interface.hpp"`.

## General remarks

`interface` methods may not be overloaded.

Can be defined at namespace and class scope, but not at function scope.

Must have at least one method. Use `std::any` instead for empty interfaces.

Pointers to objects give `interface` reference semantics. Otherwise, the stored type must be copy constructible.

`interface` should generally never be cv-qualified. `const interface` is limited to observing the underlying object through `target` and `operator bool`.

Requires C++17.

Has a default maximum of 8 methods in the interface. See impl/README for details.

## Example 1

````c++
using Fooer = INTERFACE(void(), foo);
struct R {
  void foo() {}
};
struct S {
  void foo() {} 
  void bar(int) {}
};

void foo()
{
  Fooer i = R{};
  i.foo();
  i = S{};
  i.foo();
}
````

The `i.foo()` calls will call the underlying object's `foo()`

````c++
using Foobarer = INTERFACE(void(), foo, void(int), bar);

void foobar()
{
  Foobarer f = S{};
  f.foo();
  f.bar(42);
}
````

`interface` supports multiple methods.

## Example 2

````c++
using Fooer = INTERFACE(void(), foo);
struct Q
{
  int foo(double = 42.0)
  {
    return 42;
  }
};

Fooer f = Q{};
````

Functions only need to be sufficiently similar.

## Example 3

````c++
using I = INTERFACE(void(), answer);
struct S {
  int n = 0;
  void answer() { n = 42; }
};

void meaning()
{
  S s;
  I i = s;
  i.answer();
  assert(s.n == 0);
  i = &s;
  i.answer();
  assert(s.n == 42);
}
````

Pointers gives `I` reference semantics.

## Example 4

````c++
INTERFACE(std::pair<int, float>(), fails);

using signature = std::pair<int, float>();
INTERFACE(signature, works);
````

The alias is necessary since macros don't respect angle brackets.

````c++
INTERFACE(void(std::pair<int, float>), also_works);
````

An alias isn't needed here because it's within parentheses.

## Example 5

````c++
INTERFACE(interface(interface, interface), works);
INTERFACE(void(std::vector<interface>), still_works);
INTERFACE(interface&&(*(std::vector<interface>&, interface*(*)[10]))(), this_is_fine);
````

`interface` refers to the type itself within its definition, similar to `this`.

````c++
using bad_signature = void(std::map<string, interface>);
INTERFACE(bad_signature, fails);

template<typename T>
using good_signature = void(std::map<string, T>);
INTERFACE(good_signature<interface>, works);
````

`interface` isn't visible outside its definition.

## Example 6

````c++
template<typename... Ts>
struct S {
  using type = INTERFACE(void(Ts&&...), just_works);
};
````

Works with templates as well.

## Example 7

````c++
using Fooer = INTERFACE(void(), foo);
using Foobarer = INTERFACE(void(), foo, void(int), bar);
struct S {
  void foo() {} 
  void bar(int) {}
};

Foobarer fb = S{};
Fooer f = fb;
````

There exists a conversion from an interface to another subset interface. The resulting `f` is the same as constructing from `S{}` directly.

## Other member functions

#### `template<typename T> interface(T&& t)`
Constructs an interface from `t` that have methods similar to interface methods. Similarity follows that of `std::function`.

#### `operator bool()`
Tests whether the interface holds anything.

All other special member functions all behave like they should.
There is no conversion between different interfaces unless one is a subset of another.

## Non-member functions

#### `friend swap(interface& x, interface& y)`
Swaps the contents of the interfaces.

#### `template<typename T> friend T* target(interface&& i)`
#### `template<typename T> friend T* target(interface& i)`
#### `template<typename T> friend const T* target(const interface& i)`
Returns a pointer to the underlying object of `i`. Returns `nullptr` if type doesn't match.  
Call `target<T*>` to retrieve the object from an interface storing a pointer to `T`, its resulting type is `T**`.  
Returns a `const` qualified pointer if `I` is `const` qualified.  
Can only be found by ADL, use `using ::target;` to enable if `target` is shadowed.

## Well-definedness

Invokes no undefined behaviour that I am aware of (including arcane pointer rules).

## Anonymous type

Actually, the type is a name appended with the line number. It is therefore advised to avoid defining `INTERFACE` in different translation units in the same namespace to avoid odr violations.

