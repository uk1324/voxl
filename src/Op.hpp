#pragma once

#include <stdint.h>

namespace Voxl
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

		// get -> [value]
		// set -> [rhs]
		GetConstant, // index
		GetLocal, // index
		SetLocal, // index [rhs]
		CreateGlobal, // [initializer, name]
		GetGlobal, // [name]
		SetGlobal, // [rhs, name]
		GetUpvalue, // index
		SetUpvalue, // index [rhs]
		GetField, // [value name] -> [field]
		SetField, // [rhs instance name]
		StoreMethod, // [class function name]
		GetIndex, // [value, index]
		SetIndex, // [value, index, rhs]

		// -> [constant]
		LoadNull,
		LoadTrue,
		LoadFalse,

		CreateClass, // [name] -> [class]
		Closure, // localsCount (localsCount * { indexU8, isLocalU8 }) [function] -> [closure]

		// jump = bytesToJumpForward
		// -> []
		// {
		Jump, // jump
		JumpIfTrue, // jump [condition] 
		JumpIfFalse, // jump [condition] 
		JumpIfFalseAndPop, // jump [condition] 
		// }
		JumpBack, // bytesToJumpBack

		Call, // argCount [function, args...]
		Return,
		TryBegin, // absolute jump to catch stmt.
		TryEnd,
		FinallyBegin,
		FinallyEnd,
		Throw, // [value]
		Rethrow,
		CloseUpvalue, // index
		MatchClass, // [value, class] -> [value, isInstanceBool]
		PopStack,
		Import, // [filename] -> [module]
		ModuleSetLoaded, // [module] -> [module]
		ModuleImportAllToGlobalNamespace, // [module] -> []
		CloneTop, // [value] -> [value value]
		ExpressionStatementBegin, // [] -> []
		ExpressionStatementReturn,
		Inherit, // [class, superclass] -> [class]
	};
}