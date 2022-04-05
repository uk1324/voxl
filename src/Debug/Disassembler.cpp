#include <Debug/Disassembler.hpp>
#include <Value.hpp>
#include <Asserts.hpp>

#include <iostream>
#include <iomanip>

using namespace Lang;

//void Lang::disassembleByteCode(const ByteCode& code)
//{
//	Disassembler disassembler(code);
//	disassembler.disassemble();
//}
//
//Disassembler::Disassembler(const ByteCode& code)
//	: m_code(code)
//	, m_index(0)
//{}
//
//void Disassembler::disassemble()
//{
//	while (m_index < m_code.data.size())
//	{
//		std::cout << std::setw(6) << m_index << " | ";
//
//		disassembleInstruction(static_cast<Op>(m_code.data[m_index]));
//
//		std::cout << '\n';
//	}
//}
//
//void Disassembler::disassembleInstruction(Op op)
//{
//	switch (op)
//	{
//		case Op::Add:	   simpleInstruction("add"); break;
//		case Op::Return: simpleInstruction("return"); break;
//		case Op::LoadConstant: loadInstruction("loadConstant"); break;
//		case Op::SetGlobal: loadInstruction("setGlobal"); break;
//		case Op::LoadGlobal: loadInstruction("loadGlobal"); break;
//		case Op::PopStack: simpleInstruction("popStack"); break;
//		case Op::Print: simpleInstruction("print"); break;
//
//		default:
//			std::cout << "invalid op code";
//			m_index++;
//			break;
//	}
//}
//
//void Disassembler::simpleInstruction(std::string_view name)
//{
//	std::cout << name;
//	m_index++;
//}
//
//void Disassembler::loadInstruction(std::string_view name)
//{
//	std::cout << name;
//	m_index += 5;
//}

static size_t justOp(std::string_view name)
{
	std::cout << name;
	return 1;
}

static size_t opNumber(std::string_view name, const ByteCode& byteCode, size_t offset)
{
	std::cout << name;
	uint32_t value = 0;
	for (size_t i = 0; i < 4; i++)
	{
		value <<= 8;
		value |= byteCode.code[offset + 1 + i];
	}
	std::cout << " " << value;
	return 5;
}

static size_t opConstant(std::string_view name, const ByteCode& byteCode, size_t offset)
{
	std::cout << name;
	uint32_t index = 0;
	for (size_t i = 0; i < 4; i++)
	{
		index <<= 8;
		index |= byteCode.code[offset + 1 + i];
	}
	// Justify left later
	std::cout << ' ' << index << ' ' << byteCode.constants[index];
	return 5;
}

size_t Lang::disassembleInstruction(const ByteCode& byteCode, size_t offset)
{
	if ((offset > 0) && (byteCode.lineNumberAtOffset[offset] == byteCode.lineNumberAtOffset[offset - 1]))
	{
		//std::cout << std::setw(6) << " | ";
		std::cout << "     | ";
	}
	else
	{
		std::cout << std::right << std::setw(6) << byteCode.lineNumberAtOffset[offset] + 1 << ' ';
	}

	switch (static_cast<Op>(byteCode.code[offset]))
	{
		case Op::Add: return justOp("add");
		case Op::LoadConstant: return opConstant("loadConstant", byteCode, offset);
		case Op::LoadLocal: return opNumber("loadLocal", byteCode, offset);
		case Op::SetLocal: return opNumber("loadLocal", byteCode, offset);
		case Op::Call: return opNumber("call", byteCode, offset);
		case Op::LoadGlobal: return justOp("loadGlobal");
		case Op::SetGlobal: return justOp("setGlobal");
		case Op::CreateGlobal: return justOp("createGlobal");
		case Op::Print: return justOp("print");
		case Op::LoadNull: return justOp("loadNull");
		case Op::PopStack: return justOp("popStack");
		case Op::Return: return justOp("return");

		default:
			std::cout << "invalid op";
			return 1;
	}
}

void Lang::disassembleByteCode(const ByteCode& byteCode)
{
	size_t offset = 0;
	while (offset < byteCode.code.size())
	{
		offset += disassembleInstruction(byteCode, offset);
		std::cout << '\n';
	}
}