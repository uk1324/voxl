#pragma once

#include <stdint.h>

namespace Lang
{
	enum class Op : uint8_t
	{
		Add,
		AddInt,
		LoadConstant,
		LoadLocal,
		SetLocal,
		LoadGlobal,
		SetGlobal,
		IncrementStack,
		IncrementGlobals,
		Print,
		PopStack,
		Return
	};
}