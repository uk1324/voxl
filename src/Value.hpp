#pragma once

#include <ByteCode.hpp>
#include <KeyTraits.hpp>
#include <HashMap.hpp>
#include <ostream>

namespace Lang
{

#define OBJ_TYPE_LIST(macro) \
	macro(String) \
	macro(Function) \
	macro(Closure) \
	macro(Upvalue) \
	macro(NativeFunction) \
	macro(NativeInstance) \
	macro(Class) \
	macro(Instance) \
	macro(BoundFunction)	

enum class ObjType
{
#define COMMA(type) type,
	OBJ_TYPE_LIST(COMMA)
#undef COMMA
};

#define FORWARD_DECLARE(type) struct Obj##type;
OBJ_TYPE_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class Allocator;
using MarkingFunction = void (*)(void*, Allocator&);

struct Obj
{
	ObjType type;
	Obj* next; // nullptr if it is the newest allocation
	bool isMarked;

#define GENERATE_HELPERS(objType) \
	bool is##objType() \
	{ \
		return type == ObjType::objType; \
	} \
	Obj##objType* as##objType() \
	{ \
		ASSERT(is##objType()); \
		return reinterpret_cast<Obj##objType*>(this); \
	}

	OBJ_TYPE_LIST(GENERATE_HELPERS)

#undef GENERATE_HELPERS
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
	int upvalueCount;
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

class Vm;
class Alloactor;
#define VOXL_NATIVE_FN(name) NativeFunctionResult name( \
	[[maybe_unused]] Value* args, \
	[[maybe_unused]] int argCount, \
	[[maybe_unused]] Vm& vm, \
	[[maybe_unused]] Allocator& allocator)
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
};

struct ObjInstance
{
	Obj obj;
	ObjClass* class_;
	HashTable fields;
};

struct ObjNativeInstance
{
	Obj obj;
	ObjClass* class_;
};

struct ObjBoundFunction
{
	Obj obj;
	Obj* callable;
	Value value;
};

struct ObjUpvalue
{
	Obj obj;
	Value value;
	Value* location;
};

struct ObjClosure
{
	Obj obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	int upvalueCount;
};

}

std::ostream& operator<< (std::ostream& os, Lang::Value value);
std::ostream& operator<< (std::ostream& os, Lang::Obj* obj);
bool operator== (const Lang::Value& lhs, const Lang::Value& rhs);