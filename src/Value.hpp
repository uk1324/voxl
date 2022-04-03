#pragma once

#include <ostream>

namespace Lang
{

enum class ObjType
{
	String
};

struct Obj
{
	ObjType type;
};

struct ObjString
{
	Obj obj;
	const char* chars;
	size_t length;
};

enum class ValueType
{
	Int,
	Float,
	Obj,
	Null,
};

using Int = int64_t;
using Float = double;

class Value
{
public:
	Value() {};
	explicit Value(Int value);
	explicit Value(Obj* obj);
		
public:
	static Value null();

public:
	ValueType type;

	union
	{
		Int intNumber;
		Float floatNumber;
		Obj* obj;
	} as;
};

}

std::ostream& operator<< (std::ostream& os, Lang::Value value);