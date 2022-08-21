#pragma once

#include <Asserts.hpp>
#include <ostream>

namespace Voxl
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

	const Int asInt() const
	{
		ASSERT(isInt());
		return as.intNumber;
	}

	const Float asFloat() const
	{
		ASSERT(isFloat());
		return as.floatNumber;
	}

	Obj* asObj()
	{
		ASSERT(isObj());
		return as.obj;
	}

	const Obj* asObj() const
	{
		ASSERT(isObj());
		return as.obj;
	}

	bool asBool() const
	{
		ASSERT(isBool());
		return as.boolean;
	}

public:
	static Value null();
	static Value intNum(Int value);
	static Value floatNum(Float value);

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

std::ostream& operator<< (std::ostream& os, Voxl::Value value);
std::ostream& operator<< (std::ostream& os, Voxl::Obj* obj);

#undef VALUE_TYPE_LIST