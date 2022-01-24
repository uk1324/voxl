#pragma once

#include <Lang/ByteCode.hpp>
#include <Lang/Value.hpp>

namespace Lang
{
	struct Program
	{
		ByteCode code;
		std::vector<Value> constants;

		// stores main bytecode and constant table
	};
}