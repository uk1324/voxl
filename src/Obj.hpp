#pragma once

#include <HashTable.hpp>
#include <ByteCode.hpp>

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