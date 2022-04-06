#pragma once

#include <ByteCode.hpp>
#include <ostream>

namespace Lang
{

enum class ObjType
{
	String,
	Function,
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

struct ObjFunction
{
	Obj obj;
	ObjString* name;
	int argumentCount;
	ByteCode byteCode;
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