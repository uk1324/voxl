#pragma once

#include <HashTable.hpp>
#include <ByteCode.hpp>

namespace Voxl
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
	macro(BoundFunction) \
	macro(Module)

enum class ObjType
{
#define COMMA(type) type,
	OBJ_TYPE_LIST(COMMA)
#undef COMMA
};

#define FORWARD_DECLARE(type) struct Obj##type;
OBJ_TYPE_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

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
	} \
	const Obj##objType* as##objType() const \
	{ \
		ASSERT(is##objType()); \
		return reinterpret_cast<const Obj##objType*>(this); \
	}

	OBJ_TYPE_LIST(GENERATE_HELPERS)
#undef GENERATE_HELPERS

	bool Obj::canBeBound() const
	{
		switch (type)
		{
		case ObjType::Function:	return true;
		case ObjType::NativeFunction: return true;
		case ObjType::String: return false;
		case ObjType::Closure: return false;
		case ObjType::Upvalue: return false;
		case ObjType::NativeInstance: return false;
		case ObjType::Class: return false;
		case ObjType::Instance: return false;
		case ObjType::BoundFunction: return false;
		case ObjType::Module: return false;
		}
		return false;
	}
};

class Allocator;
using MarkingFunction = void (*)(void*, Allocator&);
using InitFunction = void (*)(void*);
using FreeFunction = void (*)(void*);

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
	HashTable* globals;
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
	HashTable* globals;
	void* context;
};

struct ObjClass : public Obj
{
	ObjString* name;
	HashTable fields;
	size_t instanceSize;
	std::optional<ObjClass&> superclass;
	MarkingFunction mark;

	// This is called before $init so the object is in a valid state when entering $init. The user might try to allocate
	// something inside $init and if the object sin't a valid state at that time undefined behaviour happens. 
	// Also when inheriting from a native class the subclass has to be initialized without storing this here 
	// on each initializer of a class inherited from a native class the class hierarchy would need to be checked for
	// the init method.
	InitFunction init;
	// TODO: Almost every native class needs an init so it might be better to use it instead of mark for checking
	// the type.

	// TODO: Could store optional.
	FreeFunction free;

	bool isNative()
	{
		return instanceSize != 0;
	}
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

struct ObjModule : public Obj
{
	HashTable globals;
	bool isLoaded;
};

#undef OBJ_TYPE_LIST

}