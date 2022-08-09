#include <Debug/Disassembler.hpp>
#include <Value.hpp>
#include <Asserts.hpp>

#include <iostream>
#include <iomanip>

using namespace Voxl;

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

static size_t opConstant(std::string_view name, const ByteCode& byteCode, size_t offset, const Allocator& allocator)
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
	const auto& constant = allocator.getConstant(index);
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

static size_t closureOp(std::string name, const ByteCode& byteCode, size_t offset)
{
	auto& count = byteCode.code[offset + 1];
	auto value = &count;
	std::cout << name;
	for (size_t i = 0; i < count; i++)
	{
		value++;
		auto index = *value;
		value++;
		auto isLocal = *value;
		std::cout << " |" << (isLocal ? "local" : "upvalue") << '-' << static_cast<int>(index);
	}
	return 2 + static_cast<size_t>(count) * 2;
}

static size_t closeUpvalueOp(std::string name, const ByteCode& byteCode, size_t offset)
{
	auto& index = byteCode.code[offset + 1];
	std::cout << name << ' ' << static_cast<int>(index);
	return 2;
}

void Voxl::debugPrintValue(const Value& value)
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

size_t Voxl::disassembleInstruction(const ByteCode& byteCode, size_t offset, const Allocator& allocator)
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
		case Op::GetField: return justOp("getProperty");
		case Op::SetField: return justOp("setProperty");
		case Op::StoreMethod: return justOp("storeMethod");
		case Op::Concat: return justOp("concat");
		case Op::Equals: return justOp("equals");
		case Op::NotEquals: return justOp("notEquals");
		case Op::GetConstant: return opConstant("loadConstant", byteCode, offset, allocator);
		case Op::GetLocal: return opNumber("loadLocal", byteCode, offset);
		case Op::SetLocal: return opNumber("setLocal", byteCode, offset);
		case Op::Call: return opNumber("call", byteCode, offset);
		case Op::GetGlobal: return justOp("loadGlobal");
		case Op::SetGlobal: return justOp("setGlobal");
		case Op::CreateGlobal: return justOp("createGlobal");
		case Op::JumpIfFalse: return jump("jumpIfFalse", byteCode, offset, 1);
		case Op::JumpIfTrue: return jump("jumpIfTrue", byteCode, offset, 1);
		case Op::JumpIfFalseAndPop: return jump("jumpIfFalseAndPop", byteCode, offset, 1);
		case Op::Jump: return jump("jump", byteCode, offset, 1);
		case Op::JumpBack: return jump("jumpBack", byteCode, offset, -1);
		case Op::LoadNull: return justOp("loadNull");
		case Op::LoadTrue: return justOp("loadTrue");
		case Op::LoadFalse: return justOp("loadFalse");
		case Op::SetIndex: return justOp("setIndex");
		case Op::GetIndex: return justOp("getIndex");
		case Op::Not: return justOp("not");
		case Op::Negate: return justOp("negate");
		case Op::PopStack: return justOp("popStack");
		case Op::Return: return justOp("return");
		case Op::Closure: return closureOp("closure", byteCode, offset);
		case Op::GetUpvalue: return opNumber("getUpvalue", byteCode, offset);
		case Op::SetUpvalue: return opNumber("setUpvalue", byteCode, offset);
		case Op::CloseUpvalue: return closeUpvalueOp("closeUpvalue", byteCode, offset);
		case Op::MatchClass: return justOp("matchClass");
		case Op::Rethrow: return justOp("rethrow");
		case Op::Import: return justOp("import");
		case Op::CloneTop: return justOp("cloneTop");
		case Op::ModuleImportAllToGlobalNamespace: return justOp("moduleImportAllToGlobalNamespace");
		case Op::ModuleSetLoaded: return justOp("moduleSetLoaded");

		default:
			std::cout << "invalid op";
			return 1;
	}
}

void Voxl::disassembleByteCode(const ByteCode& byteCode, const Allocator& allocator)
{
	size_t offset = 0;
	while (offset < byteCode.code.size())
	{
		offset += disassembleInstruction(byteCode, offset, allocator);
		std::cout << '\n';
	}
}