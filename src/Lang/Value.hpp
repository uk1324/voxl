#pragma once

#include <Lang/Obj.hpp>

#include <stdint.h>

namespace Lang
{
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