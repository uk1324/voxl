#pragma once

#include <stdint.h>

namespace Lang
{
	// Could store a constant inside the code for load and set global because they never use any runtime values.
	// This would reduce code size.
	enum class Op : uint8_t
	{
		// [lhs, rhs]
		// {
		Add, 
		Subtract, 
		Multiply, 
		Divide, 
		Modulo, 
		Concat,
		// }

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
		Equals,
		LoadTrue,
		LoadFalse,
		JumpIfFalse, // 32bit - bytes to jump forward
		JumpIfFalseAndPop, // 32bit - bytes to jump forward
		JumpIfTrue,
		Jump,
		JumpBack, // 
		CreateClass, // name
		GetProperty, // instance name
		SetProperty, // [rhs instance name]
		StoreMethod, // [class function name]
	};
}