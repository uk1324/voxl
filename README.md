## voxl is an embeddable scripting language



## Language features

### variables

Creating a variable.

```rust
x : 5;
```

Assigning to a variable.

```rust
x = 5;
```

Variables are scoped.

```rust
x : 0;
{
    x : 1;
    put(x);
}
put(x);
// Outputs "10".
```

### if

```rust
if x < 5 {
	put("x is smaller than 5");
} elif x < 10 {
	put("x is smaller than 10");
} else {
	put("x is bigger or equal to 10");
}
```

### loop

```rust
loop {
    put("never halts");
}
```

### while

```javascript
i : 0;
while i < 10 {
    put(i);
    i += 1;
}
// Outputs "0123456789".
```

### for ... in

`for ... in ` loops are used to iterate iterables, which are instances of classes than have a `$iter` method that returns an iterator of the type. The iterator's type has to have a `$next` method which returns the next item or throws `StopIteration.`

Example: Range iterator

```javascript
class Range {
    $init(n) {
		$.current = 0;
    	if n >= 0 {
			$.max = 0;
    	} else {
			$.max = n;    		
    	}
    }
    
    $iter() {
    	// Returns itself.
    	ret $;
    }
    
    $next() {
    	if $.current >= $.max {
    		throw StopIteration();
    	}
    	result : $.current;
    	$.current += 1;
    	ret result;
    }
}
```

```rust
for i in Range(10) {
	put(i);
}
// Outputs "0123456789".
```

### match

match can be used similarly to C's switch statement or for pattern matching.

```rust
one : 1;
match x {
	{0} => { put("x is 0"); }
    // The cases don't have to be constants.
	{one + 1} => { put("x is 2"); }
	* => { put("else / default"); }
}
```

```rust
match expr {
	Number => { put("<number>"); }
	BinaryExpr(lhs = Number, rhs = Number) => { put("<number> + <number>"); }
	BinaryExpr(lhs = BinaryExpr, rhs = Number(value = {1})) => { put("<bianry expr> + 1"); }
}
```

### exceptions

```javascript
try {
	// Exceptions can be created using the `throw` statement.
	throw Error("error");
	// Any exception throw while executing the `try` block will be caught by and the `catch` block will execute.
} catch Error { 
	// After the catch keyword there has to be a pattern. The same thing that is used in match statements. The pattern can be followed by arrow and a variable name. The variable stores the caught exception. There can be multiple catch blocks if none of them match the exception will be rethrown.
	put("caught");
} catch Int => value {
	
} finally {
	// The finally block always executes.
	// If an exception is thrown inside the finally block the program terminates.
}
```

Types that are thrown should have a `$str` method. The string returned by this function is displayed if the exception isn't caught.

### functions

```rust
fn add(a, b) {
    ret a + b;
}
```

or using an anonymous function

```rust
add : |x, y, z| a + b;
```

Multi-statement anonymous functions have to use braces

```rust
abs : |x| {
	if x < 0 {
		ret -x;
	}
	ret x;
};
```

### closures

Nested functions can capture the variables of the outer functions.

```rust
fn make_counter() {
	i : 0;
	ret || { i += 1; ret i; };
}
```

```rust
next : make_counter();
put(counter());
put(counter());
put(counter());
// Outputs "123";
```

### classes

```javascript
class Base {
	$init(name) {
		$.name = name;
	}
	
	put_name() {
		put($.name);
	}
}

class Derived < Base {
	$init() {
		// Methods are just functions that take the instance as the first argument.
		Base.$init($, "dervied");
	}
}.
```

```
base_instance : Base("name");
derived_instance : Derived();

base_instance.put_name();
derived_instance.put_name();
// Outputs "namedervied"
```

### impl

New methods can be added to existing classes using the `impl` statement.

```rust
impl Number {
	cube() {
        ret $ * $ * $;
    }
}
```

```javascript
put((0.5).cube());
// Outputs "0.125".
```

### operator overloading

Classes can overload how operators like `+` , `*` or indexing and others work on them.

```javascript
class Vec2 {
	$init(x, y) {
		$.x = x;
		$.y = y;
	}
	
	$add(other) {
		ret Vec2($.x + other.x, $.y + other.y);
	}
	
	$mul(other) {
		match other {
			Number => { ret Vec2($.x * other, $.y * other); }
			Vec2 => { ret Vec2($.x * other.x, $.y * other.y); }
			* => { throw Error("no matching overload"); }
		}
	}
	
	$get_index(i) {
		match => {
			{0} => { ret $.x; }
			{1} => { ret $.y; }
			* => throw Error("index out of range");
		}
	}
	
	$set_index(i, value) {
		match => {
			{0} => { $.x = value; }
			{1} => { $.y = value; }
			* => throw Error("index out of range");
		}
	}
}
```

### built-in types

###### String

Strings are immutable arrays of UTF-8 characters.

```
"VOXL™"
```

###### Int and Float

Int and Float are both derived from the Number class.

###### List

```rust
list : [1, 2, 3];
impl List {
	find(item) {
		for i in Range($.size()) {
			if $[i] == item {
				ret i;
			}
		}
		ret -1;
	}
}

for item in list {
	put(item);
}
```

###### Dict

Dict stores key value pairs. They key value's type has to have a `$hash` method.

```
dict : { "abc" : 3, "abc" + "efg" : 4 };
dict["abc"] += 1;
```

### modules

Modules are imported using the `use` statement. The `use` keyword has to be followed by the string containing a path to a file or the module's name. In the case a path is used the `.voxl` extension can be omitted. 

```javascript
// Crates a variable named 'test' which can be used to access the module's members.
use "file/test";

// Rename the module variable.
use "file/test" -> not_test;

// Import only selected module members
use "file/test" -> (fn_a, fn_b, TypeA);

// Import all the module members.
use "file/test" -> *;
```

Module members can be made private by prefixing the variable with an underscore.

### foreign function interface

The FFI can be used to create functions and types in C++ that can be used inside the language. 

An FFI function has to have the following signature.

```c++
Voxl::LocalValue function_name(Voxl::Context& c);
```

Example functions

```c++
using namespace Voxl;

static constexpr int addArgCount = 2;
LocalValue add(Context& c) {
	auto self = c.args(0);
	auto other= c.args(1);
	return c.get("Vec2")(self.get("x") + other.get("x"), self.get("y") + other.get("y"));
}

static constexpr int floorArgCount = 1;
LocalValue floor(Context& c) {
	auto number = c.args(0);
    // Could just use asNumber();
    if (number.isInt()) {
        return number;
    } else if (number.isFloat()) {
        return LocalValue::intNum(::floor(number.asFloat()), c);
    }
    throw NativeException(c.get("TypeError")(LocalValue("expected a number")));
}
```

###### Creating FFI functions

FFI functions can be made built-ins by calling `Vm::defineNativeFunction`. Built-ins are accessible from every module.

```c++
vm.defineNativeFunction(nameString, functionPointer, argCount);
```

or a module can be created by providing a main function, which takes 0 arguments.

```c++
LocalValue mainFunction(Context& c) {
    c.createFunction(nameString, functionPointer, argCount);
}

vm.createModule(nameString, mainFunction);
```

Functions can take an additional data pointer argument. When a function is called the passed pointer is stored Context::data.

###### Creating FFI types

```c++
struct Type : public ObjNativeInstance {
    Voxl::Int value;
    
    static constexpr int initArgCount = 1;
	static LocalValue init(Context& c) {
        // Throws a TypeError if the type doesn't match.
        auto self = c.args(0).asObj<Type>();
        self->value = 5;
        return LocalValue::null(c);
    }
	static constexpr int getArgCount = 1;
	static LocalValue get(Context& c) {
		auto self = c.args(0).asObj<Type>();
        return LocalValue::intNum(self->value, c);
    }
}
```

Inside the module main function the type can be created.

```c++
c.createClass<Type>(
    "Type",
    {
        { "$init", Type::init, Type::initArgCount },
        { "get", Type::get, Type::getArgCount }
    }
);
```

