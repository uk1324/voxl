#pragma once

#include <stdint.h>

namespace Lang
{
	enum class ValueType
	{
		Int,
		Float
	};

	using Int = int64_t;
	using Float = double;

	struct Value
	{
	public:
		Value() {};
		explicit Value(Int value);

	public:
		ValueType type;

		union
		{
			Int intNumber;
			Float floatNumber;
		} as;
	};
}