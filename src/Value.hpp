#pragma once

#include <ByteCode.hpp>
#include <ostream>

namespace Lang
{
class Value;

enum class ObjType
{
	String,
	Function,
	ForeignFunction,
	Allocation,
};

struct Obj
{
	ObjType type;
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

std::ostream& operator<< (std::ostream& os, Lang::Value value);
bool operator== (const Lang::Value& lhs, const Lang::Value& rhs);