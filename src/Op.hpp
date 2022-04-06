#pragma once

#include <stdint.h>

namespace Lang
{
	// Could store a constant inside the code for load and set global because they never use any runtime values.
	// This would reduce code size.
	enum class Op : uint8_t
	{
		Add, // [lhs, rhs]
		LoadConstant,
		LoadLocal,
		SetLocal,
		LoadGlobal, // [name]
		SetGlobal, // [value, name]
		CreateGlobal, // [initializer, name]
		Call, // argCount [function, args...]
		Print, // [value]
		LoadNull,
		PopStack,
		Return,
		Negate,
		Not,
		LoadTrue,
		LoadFalse,
		JumpIfFalse, // 32bit - bytes to jump forward
		JumpIfTrue
	};
}