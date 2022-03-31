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
		LoadGlobal,
		SetGlobal,
		Print,
		LoadNull,
		PopStack,
		Return
	};
}