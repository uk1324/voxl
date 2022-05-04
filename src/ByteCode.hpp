#pragma once

#include <Op.hpp>

#include <stdint.h>
#include <vector>

namespace Lang
{

class Value;

struct ByteCode
{
	std::vector<uint8_t> code;
	// Could use RLE compression if the size is an issuse though I don't see why would it be.
	// Finding the line of an opcode would just require searching through the array.
	// Using the line numbers in disassembly would be just done linearly.
	std::vector<size_t> lineNumberAtOffset;

	// Could store the constants in am array seperate from the code that is shared between all the ByteCode instances.
	// This would make it possible to reuse constants but it might sometimes have to make it threadsafe for some situations
	// though it probably isn't necessary.
	std::vector<Value> constants;
};

}