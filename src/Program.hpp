#pragma once

#include <ByteCode.hpp>
#include <Value.hpp>

namespace Lang
{
	struct Program
	{
		ByteCode code;
		std::vector<Value> constants;

		// stores main bytecode and constant table
	};
}