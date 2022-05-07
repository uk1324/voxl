#pragma once

#include <ByteCode.hpp>
#include <KeyTraits.hpp>
#include <ostream>

namespace Lang
{

enum class ObjType
{
	String,
	Function,
	ForeignFunction,
	Allocation,
	Class,
	Instance,
	BoundFunction,
};

struct ObjString;
struct ObjFunction;
struct ObjNativeFunction;
struct ObjAllocation;
struct ObjClass;
struct ObjInstanceHead;
struct ObjBoundFunction;

class Allocator;
using MarkingFunction = void (*)(void*, Allocator&);
using UpdateFunction = void (*)(void*, void*, Allocator&);

struct Obj
{
	ObjType type;
	Obj* next; // nullptr if it is the newest allocation
	Obj* newLocation; // nullptr if value hasn't been copied to the other region.

	// TODO maybe inline these.
	bool isString();
	ObjString* asString();
	bool isFunction();
	ObjFunction* asFunction();
	bool isForeignFunction();
	ObjNativeFunction* asForeignFunction();
	bool isAllocation();
	ObjAllocation* asAllocation();
	bool isClass();
	ObjClass* asClass();
	bool isInstance();
	ObjInstanceHead* asInstance();
	bool isBoundFunction();
	ObjBoundFunction* asBoundFunction();
};

struct ObjString
{
	Obj obj;
	const char* chars;
	size_t size;
	// UTF-8 char count.
	size_t length;
};

struct ObjFunction
{
	Obj obj;
	ObjString* name;
	int argCount;
	ByteCode byteCode;
};

struct ObjAllocation
{
	Obj obj;
	size_t size;
	void* data();
};

enum class ValueType
{
	Int,
	Float,
	Obj,
	Null,
	Bool,
};

using Int = int64_t;
using Float = double;

class Value
{
public:
	Value() {};
	explicit Value(Int value);
	explicit Value(Float value);
	explicit Value(Obj* obj);
	explicit Value(bool boolean);
		
	bool isInt() const;
	bool isFloat() const;
	bool isObj() const;
	bool isNull() const;
	bool isBool() const;

public:
	static Value null();
	static Value integer(Int value);

public:
	ValueType type;

	union
	{
		Int intNumber;
		Float floatNumber;
		Obj* obj;
		bool boolean;
	} as;
};

enum class NativeFunctionResultType
{
	Ok,
	Exception,
	Fatal,
};

struct NativeFunctionResult
{
	/* implicit */ NativeFunctionResult(const Value& value);
	[[nodiscard]] static NativeFunctionResult exception(const Value& value);
	[[nodiscard]] static NativeFunctionResult fatal();

	NativeFunctionResultType type;
	Value value;

private:
	// Can't just use struct initialization becuase a constructor is defined.
	NativeFunctionResult() = default;
};

}

#include <HashMap.hpp>

namespace Lang
{

class Vm;
class Alloactor;
#define VOXL_NATIVE_FN(name) NativeFunctionResult name(Value* args, int argCount, Vm& vm, Allocator& allocator)
using NativeFunction = NativeFunctionResult(*)(Value* /*arguments*/, int /*argumentCount*/, Vm&, Allocator&);

struct ObjNativeFunction
{
	Obj obj;
	ObjString* name;
	int argCount;
	NativeFunction function;
};

using HashTable = HashMap<ObjString*, Value, ObjStringKeyTraits>;

struct ObjClass
{
	Obj obj;
	ObjString* name;
	HashTable fields;
	
	size_t instanceSize;
	MarkingFunction mark;
	UpdateFunction update;
};

struct ObjInstanceHead
{
	Obj obj;
	ObjClass* class_;
};

struct ObjBoundFunction
{
	Obj obj;
	ObjFunction* function;
	Value value;
};

struct ObjForeignInstance
{
	Obj obj;
};

}

std::ostream& operator<< (std::ostream& os, Lang::Value value);
std::ostream& operator<< (std::ostream& os, Lang::Obj* obj);
bool operator== (const Lang::Value& lhs, const Lang::Value& rhs);