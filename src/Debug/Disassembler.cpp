#include <Debug/Disassembler.hpp>
#include <Value.hpp>
#include <Asserts.hpp>

#include <iostream>
#include <iomanip>

using namespace Lang;

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
	std::cout << " c[" << index << "] -> ";
	const auto& constant = byteCode.constants[index];
	debugPrintValue(constant);
	return 5;
}

static size_t jump(std::string_view name, const ByteCode& byteCode, size_t offset, int sign)
{
	std::cout << name;
	uint32_t jumpSize = 0;
	for (size_t i = 0; i < 4; i++)
	{
		jumpSize <<= 8;
		jumpSize |= byteCode.code[offset + 1 + i];
	}
	auto offsetAfterDecoding = offset + 5;
	std::cout << ' ' << offset << " -> " << ((sign == 1) ? (offsetAfterDecoding + jumpSize) : (offsetAfterDecoding - jumpSize));
	return 5;
}

void Lang::debugPrintValue(const Value& value)
{
	if ((value.type == ValueType::Obj) && (value.as.obj->type == ObjType::String))
	{
		std::cout << '"' << value << '"';
	}
	else
	{
		std::cout << value;
	}
}

size_t Lang::disassembleInstruction(const ByteCode& byteCode, size_t offset)
{
	std::cout << std::left << std::setw(5) << offset;
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
		case Op::Subtract: return justOp("subtract");
		case Op::Multiply: return justOp("multiply");
		case Op::Divide: return justOp("divide");
		case Op::Modulo: return justOp("modulo");
		case Op::Less: return justOp("less");
		case Op::LessEqual: return justOp("lessEqual");
		case Op::More: return justOp("more");
		case Op::MoreEqual: return justOp("moreEqual");
		case Op::Throw: return justOp("throw");
		case Op::TryBegin: return opNumber("tryBegin", byteCode, offset);
		case Op::TryEnd: return justOp("tryEnd");
		case Op::CreateClass: return justOp("createClass");
		case Op::GetProperty: return justOp("getProperty");
		case Op::SetProperty: return justOp("setProperty");
		case Op::StoreMethod: return justOp("storeMethod");
		case Op::Concat: return justOp("concat");
		case Op::Equals: return justOp("equals");
		case Op::LoadConstant: return opConstant("loadConstant", byteCode, offset);
		case Op::LoadLocal: return opNumber("loadLocal", byteCode, offset);
		case Op::SetLocal: return opNumber("setLocal", byteCode, offset);
		case Op::Call: return opNumber("call", byteCode, offset);
		case Op::LoadGlobal: return justOp("loadGlobal");
		case Op::SetGlobal: return justOp("setGlobal");
		case Op::CreateGlobal: return justOp("createGlobal");
		case Op::JumpIfFalse: return jump("jumpIfFalse", byteCode, offset, 1);
		case Op::JumpIfTrue: return jump("jumpIfTrue", byteCode, offset, 1);
		case Op::JumpIfFalseAndPop: return jump("JumpIfFalseAndPop", byteCode, offset, 1);
		case Op::Jump: return jump("jump", byteCode, offset, 1);
		case Op::JumpBack: return jump("jumpBack", byteCode, offset, -1);
		case Op::Print: return justOp("print");
		case Op::LoadNull: return justOp("loadNull");
		case Op::LoadTrue: return justOp("loadTrue");
		case Op::LoadFalse: return justOp("loadFalse");
		case Op::Not: return justOp("not");
		case Op::Negate: return justOp("negate");
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