#pragma once

#include <ByteCode.hpp>
#include <KeyTraits.hpp>
#include <HashMap.hpp>
#include <ostream>

namespace Lang
{

using Int = int64_t;
using Float = double;

struct Obj;

#define VALUE_TYPE_LIST(macro) \
	macro(Int) \
	macro(Float) \
	macro(Obj) \
	macro(Null) \
	macro(Bool)

enum class ValueType
{
#define COMMA(type) type,
	VALUE_TYPE_LIST(COMMA)
#undef COMMA
};

class Value
{
public:
	Value() {};
	explicit Value(Int value);
	explicit Value(Float value);
	explicit Value(Obj* obj);
	explicit Value(bool boolean);
		
#define GENERATE_HELPERS(valueType) \
	bool is##valueType() const \
	{ \
		return type == ValueType::valueType; \
	}
	VALUE_TYPE_LIST(GENERATE_HELPERS)
#undef GENERATE_HELPERS

	Int asInt()
	{
		ASSERT(isInt());
		return as.intNumber;
	}

	Float asFloat()
	{
		ASSERT(isFloat());
		return as.floatNumber;
	}

	Obj* asObj()
	{
		ASSERT(isObj());
		return as.obj;
	}

	bool asBool()
	{
		ASSERT(isBool());
		return as.boolean;
	}

public:
	static Value null();
	static Value integer(Int value);

	ValueType type;

	union
	{
		Int intNumber;
		Float floatNumber;
		Obj* obj;
		bool boolean;
	} as;
};

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
	bool is##objType() const \
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

struct ObjString : public Obj
{
	const char* chars;
	size_t size;
	// UTF-8 char count.
	size_t length;
};

struct ObjFunction : public Obj
{
	ObjString* name;
	int argCount;
	ByteCode byteCode;
	int upvalueCount;
};

class Context;
class LocalValue;
using NativeFunction = LocalValue(*)(Context&);

struct NativeException
{
	NativeException(const LocalValue& value);
	NativeException(const Value& value);

	Value value;
};

struct ObjNativeFunction : public Obj
{
	ObjString* name;
	int argCount;
	NativeFunction function;
};

using HashTable = HashMap<ObjString*, Value, ObjStringKeyTraits>;

struct ObjClass : public Obj
{
	ObjString* name;
	HashTable fields;
	size_t instanceSize;

	MarkingFunction mark;
};

struct ObjInstance : public Obj
{
	ObjClass* class_;
	HashTable fields;
};

struct ObjNativeInstance : public Obj
{
	ObjClass* class_;
};

struct ObjBoundFunction : public Obj
{
	Obj* callable;
	Value value;
};

struct ObjUpvalue : public Obj
{
	Value value;
	Value* location;
};

struct ObjClosure : public Obj
{
	ObjFunction* function;
	ObjUpvalue** upvalues;
	int upvalueCount;
};

}

std::ostream& operator<< (std::ostream& os, Lang::Value value);
std::ostream& operator<< (std::ostream& os, Lang::Obj* obj);
bool operator== (const Lang::Value& lhs, const Lang::Value& rhs);

#undef OBJ_TYPE_LIST
#undef VALUE_TYPE_LIST