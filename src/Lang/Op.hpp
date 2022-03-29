#pragma once

#include <stdint.h>

namespace Lang
{
	enum class Op : uint8_t
	{
		Add,
		LoadConstant,
		LoadLocal,
		SetLocal,
		DefineGlobal,
		LoadGlobal,
		SetGlobal,
		Print,
		LoadNull,
		PopStack,
		Return
	};
}