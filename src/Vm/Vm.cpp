#include <Vm/Vm.hpp>
#include <Asserts.hpp>
#include <iostream>

//#define DEBUG_PRINT_EXECUTION_TRACE
#ifdef DEBUG_PRINT_EXECUTION_TRACE
	#include <Debug/Disassembler.hpp>
#endif


using namespace Lang;

Vm::Vm()
	: m_allocator(nullptr)
	, m_errorPrinter(nullptr)
	, m_stackTop(nullptr)
	, m_callStackSize(0)
{}

Vm::Result Vm::execute(ObjFunction* program, Allocator& allocator, ErrorPrinter& errorPrinter)
{
	m_allocator = &allocator;
	m_errorPrinter = &errorPrinter;
	m_stackTop = m_stack.data();
	m_callStack[0] = CallFrame{ program->byteCode.code.data(), m_stack.data(), program };
	m_callStackSize = 1;
	pushStack(Value(reinterpret_cast<Obj*>(program)));

	return run();
}

Vm::Result Vm::run()
{
	std::cout << '\n';
	for (;;)
	{
	#ifdef DEBUG_PRINT_EXECUTION_TRACE
		std::cout << "[ ";
		for (Value* value = m_stack.data(); value != m_stackTop; value++)
		{
			std::cout << *value << ' ';
		}
		std::cout << "]\n";
		disassembleInstruction(
			callStackTop().function->byteCode,
			callStackTop().instructionPointer - callStackTop().function->byteCode.code.data());
		std::cout << '\n';
	#endif
		Op op = static_cast<Op>(*callStackTop().instructionPointer);
		callStackTop().instructionPointer++;

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
			pushStack(callStackTop().function->byteCode.constants[constantIndex]);
			break;
		}

		case Op::LoadLocal:
		{
			size_t stackOffset = (size_t)readUint32();
			pushStack(callStackTop().values[stackOffset + 1]);
			break;
		}

		case Op::SetLocal:
		{
			size_t stackOffset = (size_t)readUint32();
			callStackTop().values[stackOffset + 1] = peekStack(0);
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
				m_errorPrinter->outStream() << "redeclaration of '" << name->chars << '\'';
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
				m_errorPrinter->outStream() << '\'' << name->chars << "' is not defined\n";
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
				m_errorPrinter->outStream() << '\'' << name->chars << "' is not defined\n";
				return Result::RuntimeError;
			}
			popStack();
			popStack();
			break;
		}

		case Op::LoadNull:
			pushStack(Value::null());
			break;

		case Op::LoadTrue:
			pushStack(Value(true));
			break;

		case Op::LoadFalse:
			pushStack(Value(false));
			break;

		case Op::Negate:
		{
			auto& value = peekStack(0);
			if (value.type == ValueType::Int)
			{
				value.as.intNumber = -value.as.intNumber;
			}
			else
			{
				return fatalError("can only negate numers");
			}
			break;
		}

		case Op::Not:
		{
			auto& value = peekStack(0);
			if (value.type == ValueType::Bool)
			{
				value.as.boolean = !value.as.boolean;
			}
			else
			{
				return fatalError("can use not on bools");
			}
			break;
		}

		case Op::Call:
		{
			auto argCount = readUint32();
			auto calleValue = peekStack(argCount);
			if ((calleValue.type != ValueType::Obj) || (calleValue.as.obj->type != ObjType::Function))
			{
				m_errorPrinter->outStream() << "cannot call object that isn't function\n";
				return Result::RuntimeError;
			}
			auto calle = reinterpret_cast<ObjFunction*>(calleValue.as.obj);

			if ((m_callStackSize + 1) == m_callStack.size())
			{
				return fatalError("call stack overflow");
			}

			m_callStack[m_callStackSize] = CallFrame{ calle->byteCode.code.data(), m_stackTop - 1 - argCount, calle };
			m_callStackSize++;

			break;
		}

		case Op::Print:
		{
			std::cout << peekStack() << '\n';
			break;
		}

		case Op::PopStack:
		{
			popStack();
			break;
		}

		case Op::Return:
		{
			if (m_callStackSize == 1)
			{
				return Result::Success;
			}
			else
			{
				Value result = peekStack();
				m_stackTop = callStackTop().values + callStackTop().function->argumentCount + 1;
				m_stackTop -= 2;
				m_callStackSize--;
				pushStack(result);
			}
			break;
		}

		case Op::JumpIfFalse:
		{
			auto jump = readUint32();
			auto& value = peekStack(0);
			if ((value.type != ValueType::Bool))
			{
				return fatalError("must be bool");
			}
			if (value.as.boolean == false)
			{
				callStackTop().instructionPointer += jump;
			}
			break;
		}

		case Op::JumpIfTrue:
		{
			auto jump = readUint32();
			auto& value = peekStack(0);
			if ((value.type != ValueType::Bool))
			{
				return fatalError("must be bool");
			}
			if (value.as.boolean)
			{
				callStackTop().instructionPointer += jump;
			}
			break;
		}

		default:
			ASSERT_NOT_REACHED();
			return Result::RuntimeError;
		}
	}
}

uint8_t Vm::readUint8()
{
	uint8_t value = *m_callStack[m_callStackSize - 1].instructionPointer;
	m_callStack[m_callStackSize - 1].instructionPointer++;
	return value;
}

const Value& Vm::peekStack(size_t depth) const
{
	return *(m_stackTop - depth - 1);
}

Value& Vm::peekStack(size_t depth)
{
	return *(m_stackTop - depth - 1);
}

void Vm::popStack()
{
	m_stackTop--;
}

void Vm::pushStack(Value value)
{
	*m_stackTop = value;
	m_stackTop++;
}

const Vm::CallFrame& Vm::callStackTop() const
{
	return m_callStack[m_callStackSize - 1];
}

Vm::Result Vm::fatalError(const char* format, ...)
{
	char errorBuffer[256];
	va_list args;
	va_start(args, format);
	vsnprintf(errorBuffer, sizeof(errorBuffer), format, args);
	va_end(args);
	m_errorPrinter->outStream() << "fatal runtime error: " << errorBuffer << '\n';
	for (CallFrame* callFrame = &callStackTop(); callFrame != m_callStack.data(); callFrame--)
	{
		size_t instructionOffset = callFrame->instructionPointer - callFrame->function->byteCode.code.data();
		auto lineNumber = callFrame->function->byteCode.lineNumberAtOffset[instructionOffset];
		m_errorPrinter->outStream() << "line " << lineNumber << " in " << callFrame->function->name->chars << "()\n";
	}
	return Result::RuntimeError;
}


Vm::CallFrame& Vm::callStackTop()
{
	return m_callStack[m_callStackSize - 1];
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
