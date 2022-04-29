#pragma once

#include <ByteCode.hpp>
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
struct ObjClass;
struct ObjInstance;
struct ObjBoundFunction;

struct Obj
{
	ObjType type;
	Obj* next; // nullptr if it is the newest allocation
	Obj* newLocation; // nullptr if value hasn't been copied to the other region.

	bool isString();
	ObjString* asString();
	bool isClass();
	ObjClass* asClass();
	bool isInstance();
	ObjInstance* asInstance();
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
	int argumentCount;
	ByteCode byteCode;
};

using ForeignFunction = Value (*)(Value* /*arguments*/, int /*argumentCount*/);

struct ObjForeignFunction
{
	Obj obj;
	ObjString* name;
	ForeignFunction function;
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

}

#include <HashMap.hpp>

namespace Lang
{

struct ObjStringKeyTraits
{
	inline static bool compareKeys(const ObjString* a, const ObjString* b)
	{
		return (a->size == b->size) && (memcmp(a->chars, b->chars, a->size) == 0);
	}

	inline static size_t hashKey(const ObjString* key)
	{
		return std::hash<std::string_view>()(std::string_view(key->chars, key->size));
	}

	inline static void setKeyNull(ObjString*& key)
	{
		key = nullptr;
	}

	inline static void setKeyTombstone(ObjString*& key)
	{
		key = reinterpret_cast<ObjString*>(1);
	}

	inline static bool isKeyNull(const ObjString* key)
	{
		return key == nullptr;
	}

	inline static bool isKeyTombstone(const ObjString* key)
	{
		return key == reinterpret_cast<ObjString*>(1);
	}
};

using HashTable = HashMap<ObjString*, Value, ObjStringKeyTraits>;

struct ObjClass
{
	Obj obj;
	ObjString* name;
	HashTable fields;
	HashTable methods;
};

struct ObjInstance
{
	Obj obj;
	ObjClass* class_;
	HashTable fields;
};

struct ObjBoundFunction
{
	Obj obj;
	ObjFunction* function;
	ObjInstance* instance;
};

}

std::ostream& operator<< (std::ostream& os, Lang::Value value);
std::ostream& operator<< (std::ostream& os, Lang::Obj* obj);
bool operator== (const Lang::Value& lhs, const Lang::Value& rhs);