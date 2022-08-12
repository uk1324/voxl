#pragma once

#include <Op.hpp>

#include <stdint.h>
#include <vector>

namespace Voxl
{

	class Value;

	struct ByteCode
	{
		void append(const ByteCode& src);

		std::vector<uint8_t> code;
		// Could use RLE compression if the size is an issuse though I don't see why would it be.
		// Finding the line of an opcode would just require searching through the array.
		// Using the line numbers in disassembly would be just done linearly.
		std::vector<size_t> lineNumberAtOffset;
	};

}