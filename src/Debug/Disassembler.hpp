#pragma once

#include <ByteCode.hpp>
#include <string_view>

// If I want to use instructions like extend arg the has to treat the extend arg and everything after it as one instruction
// for it to make sense. I would need to make extend arg a special case in the vm.
// Check if is extend arg if yes then count all the extends after it or if not extend arg execute any other instruction and print it .


namespace Lang
{

size_t disassembleInstruction(const ByteCode& byteCode, size_t offset);
void disassembleByteCode(const ByteCode& byteCode);

	//class Disassembler
	//{
	//	friend void disassembleByteCode(const ByteCode& code);

	//public:
	//	Disassembler(const ByteCode& code);

	//	void disassemble();
	//	void disassembleInstruction(Op op);

	//	void simpleInstruction(std::string_view name);
	//	void loadInstruction(std::string_view name);

	//private:
	//	const ByteCode& m_code;
	//	size_t m_index;
	//};
}