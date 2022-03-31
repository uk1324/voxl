#include <Vm/Vm.hpp>
#include <assert.h>
#include <iostream>

using namespace Lang;

Vm::Result Vm::run(const Program& program)
{
	m_instructionPointer = program.code.data.data();

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
					pushStack(Value(a.as.intNumber + b.as.intNumber));
				}
				else
				{
					//return Vm::R
				}
				popStack();
				popStack();
				break;
			}

			case Op::LoadConstant:
			{
				const auto constantIndex = readUint32();
				m_stack.push_back(program.constants[constantIndex]);
				break;
			}

			case Op::PopStack:
			{
				popStack();
				break;
			}

			case Op::LoadGlobal:
			{
				pushStack(m_globals[readUint32()]);
				break;
			}

			case Op::SetGlobal:
			{
				m_globals[readUint32()] = peekStack();
				break;
			}

			case Op::Print:
			{
				printValue(peekStack());
				break;
			}

			case Op::Return:
			{
				return Result::Success;
			}

			default:
				assert(false);
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
			assert(false);
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
