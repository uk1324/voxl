#pragma once

#include <Asserts.hpp>
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

}

std::ostream& operator<< (std::ostream& os, Lang::Value value);
std::ostream& operator<< (std::ostream& os, Lang::Obj* obj);
bool operator== (const Lang::Value& lhs, const Lang::Value& rhs);

#undef OBJ_TYPE_LIST
#undef VALUE_TYPE_LIST