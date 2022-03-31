#pragma once

#include <ByteCode.hpp>

#include <string_view>

namespace Lang
{
	class Disassembler
	{
		friend void disassembleByteCode(const ByteCode& code);

	public:
		Disassembler(const ByteCode& code);

		void disassemble();
		void disassembleInstruction(Op op);

		void simpleInstruction(std::string_view name);
		void loadInstruction(std::string_view name);

	private:
		const ByteCode& m_code;
		size_t m_index;
	};
}