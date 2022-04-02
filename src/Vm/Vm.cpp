#include <Vm/Vm.hpp>
#include <Asserts.hpp>
#include <iostream>

using namespace Lang;

Vm::Vm()
	: m_instructionPointer(nullptr)
	, m_allocator(nullptr)
	, m_errorPrinter(nullptr)
{}

Vm::Result Vm::run(const Program& program, Allocator& allocator, ErrorPrinter& errorPrinter)
{
	m_instructionPointer = program.code.data.data();
	m_allocator = &allocator;
	m_errorPrinter = &errorPrinter;

	for (;;)
	{
		Op op = static_cast<Op>(*m_instructionPointer);
		m_instructionPointer++;

		switch (op)
		{
			case Op::Add:
			{
				Value a = peekStack(1);
				Value b = peekStack(0);
				if (a.type == ValueType::Int && b.type == ValueType::Int)
				{
					popStack();
					popStack();
					pushStack(Value(a.as.intNumber + b.as.intNumber));
				}
				else
				{
					return Result::RuntimeError;
				}
				break;
			}

			case Op::LoadConstant:
			{
				const auto constantIndex = readUint32();
				m_stack.push_back(program.constants[constantIndex]);
				break;
			}

			case Op::LoadLocal:
			{
				size_t stackOffset = (size_t)readUint32();
				pushStack(m_stack[stackOffset]);
				break;
			}

			case Op::SetLocal:
			{
				size_t stackOffset = (size_t)readUint32();
				m_stack[stackOffset] = peekStack(0);
				popStack();
				break;
			}

			case Op::CreateGlobal:
			{
				// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
				ObjString* name = reinterpret_cast<ObjString*>(peekStack(0).as.obj);
				Value value = peekStack(1);
				bool doesNotAlreadyExist = m_globals.insert_or_assign(name, value).second;
				if (doesNotAlreadyExist == false)
				{
					errorPrinter.outStream() << "redeclaration of '" << name->chars << '\'';
					return Result::RuntimeError;
				}
				popStack();
				popStack();
				break;
			}

			case Op::LoadGlobal:
			{
				ObjString* name = reinterpret_cast<ObjString*>(peekStack(0).as.obj);
				auto value = m_globals.find(name);
				if (value == m_globals.end())
				{
					errorPrinter.outStream() << '\'' << name->chars << "' is not defined\n";
					return Result::RuntimeError;
				}
				popStack();
				pushStack(value->second);
				break;
			}

			case Op::SetGlobal:
			{
				// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
				ObjString* name = reinterpret_cast<ObjString*>(peekStack(0).as.obj);
				Value value = peekStack(1);
				bool doesNotAlreadyExist = m_globals.insert_or_assign(name, value).second;
				if (doesNotAlreadyExist)
				{
					errorPrinter.outStream() << '\'' << name->chars << "' is not defined\n";
					return Result::RuntimeError;
				}
				popStack();
				popStack();
				break;
			}

			case Op::Print:
			{
				printValue(peekStack());
				break;
			}

			case Op::PopStack:
			{
				popStack();
				break;
			}

			case Op::Return:
			{
				return Result::Success;
			}

			default:
				ASSERT_NOT_REACHED();
				return Result::RuntimeError;
		}
	}
}

uint8_t Vm::readUint8()
{
	uint8_t value = *m_instructionPointer;
	m_instructionPointer++;
	return value;
}

void Vm::printValue(const Value& value)
{
	switch (value.type)
	{
		case ValueType::Int:
			std::cout << value.as.intNumber;
			break;

		case ValueType::Float:
			std::cout << value.as.floatNumber;
			break;

		default:
			ASSERT_NOT_REACHED();
	}
}

const Value& Vm::peekStack(size_t depth) const
{
	return m_stack[m_stack.size() - depth];
}

Value& Vm::peekStack(size_t depth)
{
	return m_stack[m_stack.size() - 1 - depth];
}

void Vm::popStack()
{
	m_stack.pop_back();
}

void Vm::pushStack(Value value)
{
	m_stack.push_back(value);
}

uint32_t Vm::readUint32()
{
	uint32_t value = readUint8();
	for (int i = 0; i < 3; i++)
	{
		value <<= 8;
		value |= static_cast<uint32_t>(readUint8());
	}
	return value;
}

size_t Vm::ObjStringHasher::operator()(const ObjString* string) const
{
	return std::hash<std::string_view>()(std::string_view(string->chars, string->length));
}

size_t Vm::ObjStringComparator::operator()(const ObjString* a, const ObjString* b) const
{
	return (a->length == b->length) && (memcmp(a->chars, b->chars, a->length) == 0);
}
