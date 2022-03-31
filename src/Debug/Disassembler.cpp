#include <Debug/Disassembler.hpp>

#include <iostream>
#include <iomanip>

using namespace Lang;

void Lang::disassembleByteCode(const ByteCode& code)
{
	Disassembler disassembler(code);
	disassembler.disassemble();
}

Disassembler::Disassembler(const ByteCode& code)
	: m_code(code)
	, m_index(0)
{}

void Disassembler::disassemble()
{
	while (m_index < m_code.data.size())
	{
		std::cout << std::setw(6) << m_index << " | ";

		disassembleInstruction(static_cast<Op>(m_code.data[m_index]));

		std::cout << '\n';
	}
}

void Disassembler::disassembleInstruction(Op op)
{
	switch (op)
	{
		case Op::Add:	   simpleInstruction("add"); break;
		case Op::Return: simpleInstruction("return"); break;
		case Op::LoadConstant: loadInstruction("loadConstant"); break;
		case Op::SetGlobal: loadInstruction("setGlobal"); break;
		case Op::LoadGlobal: loadInstruction("loadGlobal"); break;
		case Op::PopStack: simpleInstruction("popStack"); break;
		case Op::Print: simpleInstruction("print"); break;

		default:
			std::cout << "invalid op code";
			m_index++;
			break;
	}
}

void Disassembler::simpleInstruction(std::string_view name)
{
	std::cout << name;
	m_index++;
}

void Disassembler::loadInstruction(std::string_view name)
{
	std::cout << name;
	m_index += 5;
}