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
using MarkingFunctionPtr = void (*)(void*, Allocator&);
using InitFunctionPtr = void (*)(void*);
using FreeFunctionPtr = void (*)(void*);

template<typename T>
using MarkingFunction = void (*)(T*, Allocator&);
template<typename T>
using InitFunction = void (*)(T*);
template<typename T>
using FreeFunction = void (*)(T*);

struct ObjString : public Obj
{
	const char* chars;
	size_t size;
	// UTF-8 char count.
	size_t length;
	size_t hash;

	static size_t hashString(const char* chars, size_t charsSize)
	{
		// TODO: Don't need to hash the whole thing if the string is long.
		// Don't know what the implementation does.
		return std::hash<std::string_view>()(std::string_view(chars, charsSize));
	}
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
	MarkingFunctionPtr mark;

	// This is called before $init so the object is in a valid state when entering $init. The user might try to allocate
	// something inside $init and if the object sin't a valid state at that time undefined behaviour happens. 
	// Also when inheriting from a native class the subclass has to be initialized without storing this here 
	// on each initializer of a class inherited from a native class the class hierarchy would need to be checked for
	// the init method.
	InitFunctionPtr init;
	// TODO: Almost every native class needs an init so it might be better to use it instead of mark for checking
	// the type.

	// TODO: Could store optional.
	FreeFunctionPtr free;

	// This count is needed because to free an instance of a class the free function stored inside class is needed so the class
	// has to be kept alive if all the instances haven't been deleted yet. With the current implementation a faster way to achieve this
	// would be to store objects allocated first in front or just have a doubly liked list. A instance of a class
	// can't be allocated before the class so when freeing the instances will always be freed before the classes.
	// Another alternative to this would be to store the free function inside each instance, but this would take up more memory than the first 
	// option.
	size_t nativeInstanceCount;

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

// Native classes could be implemented using virtual inheritance, but then they would need to store more data 
// and also checking if the type is correct would require a dynamic_cast which is more expensive and requires RTTI.
struct ObjNativeInstance : public Obj
{
	ObjClass* class_;
	template<typename T>
	bool isOfType()
	{
		return class_->mark == reinterpret_cast<MarkingFunctionPtr>(T::mark);
	}
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