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

`interface` should generally never be cv-qualified. `const interface` is limited to observing the underlying object through `target`, `operator bool` and equality comparisons.

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

Objects give `I` value semantics, pointers give `I` reference semantics.

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
using Cloner = INTERFACE(interface(), clone);
struct C
{
    Cloner clone() { return *this; }
};

Cloner c1 = C{};
auto c2 = c1.clone();
````

`interface` refers to the type itself within its definition, similar to `this`.

````c++
INTERFACE(interface(interface, interface), works);
INTERFACE(void(std::vector<interface>), still_works);
INTERFACE(interface&&(*(std::array<interface, 42>&, interface*(*)[42]))(), this_is_fine);
````

Can be used in arbitrarily compounded types.

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

## Example 8

````c++
using Fooer = INTERFACE(void(), foo);
struct S {
    void foo() const volatile & {}
};

Fooer f = S{};
````

Can call cvr-qualified functions.

````c++
struct Fail
{
    void foo() && {}
};
````

Only calls with a non-qualified lvalue. Note overload resolution prefers unqualified versions.

````c++
INTERFACE(void() const, fails);
````

Interface methods cannot be cvr-qualified.

## Member functions

#### `template<typename T> interface(T&& t)`
Constructs an interface from `t` that have methods similar to interface methods. Similarity follows that of `std::function`. Only participates in overload resolution if `T` isn't an interface.

#### `template<typename I> interface(I&& i)`
Constructs an interface from another interface `I` that must have a superset of methods. Only participates in overload resolution if `I` is an interface.

#### `signature method_name`
`signature` and `method_name` are arguments passed in to the interface.  
Calls the underlying object's method with the same name and sufficiently similar signature selected through overload resolution. The return type does not participate in resolution and must be convertible to the interface return type.
````c++
using I = INTERFACE(void(int), f);
struct S {
  int f(int) {};
  void f(char) {};
};

void f()
{
  I{S{}}.f(42);  // calls f(int)
}
````

#### `explicit operator bool() const noexcept`
Tests whether the interface holds anything.

#### `bool operator==(const interface&) const noexcept`
#### `bool operator!=(const interface&) const noexcept`
Two interfaces compare equal iff they are both empty or refer to the same object. Only participates in overload resolution if the argument has the same interface type.

All other special member functions all behave like they should.

## Non-member functions

#### `friend void swap(interface& x, interface& y) noexcept`
Swaps the contents of the interfaces.

#### `template<typename T> friend T* target(interface&& i) noexcept`
#### `template<typename T> friend T* target(interface& i) noexcept`
#### `template<typename T> friend const T* target(const interface& i) noexcept`
Returns a pointer to the underlying object of `i`. Returns `nullptr` if type doesn't match.  
Returned pointer is invalidated on assignment and copy to interface, but not on move.

````c++
using Bazer = INTERFACE(int(), baz);
struct Q {
  int i;
  int baz() { return i; }
};

void baz()
{
  Q q1{1}, q2{2};
  Bazer b = &q1;
  
  assert(!target<Q>(b));  // target is Q*
  assert(target<Q*>(b));
  
  assert(b.baz() == 1);
  *target<Q*>(b1) = &q2;    // result of target is Q**
  assert(b.baz() == 2);
  
  auto p = target<Q*>(b);
  Bazer b2 = std::move(b);
  assert(p == target<Q*>(b2));  // moving doesn't invalidate pointer
}
````


## Well-definedness

Invokes no undefined behaviour that I am aware of.

## Anonymous type

Actually, the type is a name appended with the line number. It is therefore advised to avoid defining `INTERFACE` in different translation units in the same namespace to avoid odr violations.

