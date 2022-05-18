#pragma once

#include <stdint.h>

namespace Lang
{
	// Could store a constant inside the code for load and set global because they never use any runtime values.
	// This would reduce code size.
	enum class Op : uint8_t
	{
		// [lhs, rhs] -> [result]
		// {
		Add, 
		Subtract, 
		Multiply, 
		Divide, 
		Modulo, 
		Concat,
		Less,
		LessEqual,
		More,
		MoreEqual,
		Equals,
		NotEquals,
		// }

		// [value] -> [result]
		// {
		Negate,
		Not,
		// }

		GetIndex, // [value, index]
		SetIndex, // [value, index, rhs]
		LoadConstant,
		LoadLocal,
		SetLocal,
		LoadUpvalue,
		SetUpvalue,
		LoadGlobal, // [name]
		SetGlobal, // [value, name]
		CreateGlobal, // [initializer, name]
		Call, // argCount [function, args...]
		Print, // [value]
		LoadNull,
		PopStack,
		Return,
		LoadTrue,
		LoadFalse,
		JumpIfFalse, // 32bit - bytes to jump forward
		JumpIfFalseAndPop, // 32bit - bytes to jump forward
		JumpIfTrue,
		Jump,
		JumpBack, // 
		CreateClass, // name
		GetField, // instance name
		SetField, // [rhs instance name]
		StoreMethod, // [class function name]
		TryBegin, // absolute jump to catch stmt.
		TryEnd,
		Throw, // [value]
		Closure,
		CloseUpvalue,
	};
}