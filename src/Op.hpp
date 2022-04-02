#pragma once

#include <stdint.h>

namespace Lang
{
	enum class Op : uint8_t
	{
		Add, // [lhs, rhs]
		LoadConstant,
		LoadLocal,
		SetLocal,
		LoadGlobal, // [name]
		SetGlobal, // [value, name]
		CreateGlobal, // [initializer, name]
		Print, // [value]
		LoadNull,
		PopStack,
		Return
	};
}