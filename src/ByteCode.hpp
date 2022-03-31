#pragma once

#include <Op.hpp>

#include <stdint.h>
#include <vector>

// One constant table should be shared between ByteCode objets

namespace Lang
{
	class ByteCode
	{
	public:
		void emitOp(Op op);
		void emitByte(uint8_t byte);
		void emitWord(uint16_t word);
		void emitDword(uint32_t dword);

		void addConstant();

	public:
		std::vector<uint8_t> data;
	};
}