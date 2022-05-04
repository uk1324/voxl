#include <Vm/Vm.hpp>
#include <Utf8.hpp>
#include <Debug/DebugOptions.hpp>
#include <Asserts.hpp>
#include <iostream>
#include <sstream>

#ifdef VOXL_DEBUG_PRINT_VM_EXECUTION_TRACE
	#include <Debug/Disassembler.hpp>
#endif

using namespace Lang;

Vm::Vm(Allocator& allocator)
	: m_allocator(&allocator)
	, m_errorPrinter(nullptr)
	, m_stackTop(nullptr)
	, m_callStackSize(0)
	, m_rootMarkingFunctionHandle(allocator.registerRootMarkingFunction(this, mark))
	, m_updateFunctionHandle(allocator.registerUpdateFunction(this, update))
{
	HashTable::init(m_globals);
	m_initString = m_allocator->allocateStringConstant("$init").value;
	m_addString = m_allocator->allocateStringConstant("$add").value;
	m_subString = m_allocator->allocateStringConstant("$sub").value;
	m_mulString = m_allocator->allocateStringConstant("$mul").value;
	m_divString = m_allocator->allocateStringConstant("$div").value;
	m_modString = m_allocator->allocateStringConstant("$mod").value;
	m_ltString = m_allocator->allocateStringConstant("$lt").value;
	m_leString = m_allocator->allocateStringConstant("$le").value;
	m_gtString = m_allocator->allocateStringConstant("$gt").value;
	m_geString = m_allocator->allocateStringConstant("$ge").value;
}

Vm::Result Vm::execute(ObjFunction* program, ErrorPrinter& errorPrinter)
{
	m_errorPrinter = &errorPrinter;
	m_stackTop = m_stack.data();
	m_callStack[0] = CallFrame{ program->byteCode.code.data(), m_stack.data(), program };
	m_callStackSize = 1;

	auto result = run();
	if (result == Result::Success)
	{
		ASSERT(m_stack.data() == m_stackTop); // The program should always finish without anything on the stack.
	}
	return result;
}

void Vm::reset()
{
	m_globals.clear();
}

Vm::Result Vm::run()
{
	for (;;)
	{
	#ifdef VOXL_DEBUG_PRINT_VM_EXECUTION_TRACE
		std::cout << "[ ";
		for (Value* value = m_stack.data(); value != m_stackTop; value++)
		{
			debugPrintValue(*value);
			std::cout << ' ';
		}
		std::cout << "]\n";
		disassembleInstruction(
			callStackTop().function->byteCode,
			callStackTop().instructionPointer - callStackTop().function->byteCode.code.data());
		std::cout << '\n';
	#endif

	#ifdef VOXL_DEBUG_STRESS_TEST_GC
		m_allocator->runGc();
	#endif

		Op op = static_cast<Op>(*callStackTop().instructionPointer);
		callStackTop().instructionPointer++;

		switch (op)
		{
#define BINARY_ARITHMETIC_OP(op, overloadNameString) \
	{ \
		Value lhs = peekStack(1); \
		Value rhs = peekStack(0); \
		if (lhs.isObj() && lhs.as.obj->isInstance()) \
		{ \
			auto instance = lhs.as.obj->asInstance(); \
			auto optMethod = instance->class_->methods.get(overloadNameString); \
			if (optMethod.has_value()) \
			{ \
				if (callValue(**optMethod, 2, 0) == Result::RuntimeError) \
				{ \
					return Result::RuntimeError; \
				} \
			} \
			else \
			{ \
				return fatalError("no operator" #op " for these types"); \
			} \
		} \
		else \
		{ \
			popStack(); \
			popStack(); \
			if (lhs.isInt() && rhs.isInt()) \
			{ \
				pushStack(Value(lhs.as.intNumber op rhs.as.intNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isFloat()) \
			{ \
				pushStack(Value(lhs.as.floatNumber op rhs.as.floatNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isInt()) \
			{ \
				pushStack(Value(lhs.as.floatNumber op static_cast<Float>(rhs.as.intNumber))); \
			} \
			else if (lhs.isInt() && rhs.isFloat()) \
			{ \
				pushStack(Value(static_cast<Float>(lhs.as.intNumber) op rhs.as.floatNumber)); \
			} \
			else \
			{ \
				return fatalError("no operator " #op " for these types"); \
			} \
		} \
		break; \
	}
		case Op::Add: BINARY_ARITHMETIC_OP(+, m_addString)
		case Op::Subtract: BINARY_ARITHMETIC_OP(-, m_subString)
		case Op::Multiply: BINARY_ARITHMETIC_OP(*, m_mulString)
			// TODO: handle integer division by zero
		case Op::Divide: BINARY_ARITHMETIC_OP(/, m_divString)
#undef BINARY_ARITHMETIC_OP
		case Op::Modulo: 
		{
			Value lhs = peekStack(1);
			Value rhs = peekStack(0);
			if (lhs.isObj() && lhs.as.obj->isInstance())
			{
				auto instance = lhs.as.obj->asInstance();
				auto optMethod = instance->class_->methods.get(m_modString);
				if (optMethod.has_value())
				{
					if (callValue(**optMethod, 2, 0) == Result::RuntimeError)
					{
						return Result::RuntimeError;
					}
				}
				else
				{
					return fatalError("no operator % for these types");
				}
			}
			else
			{
				popStack();
				popStack();
				if (lhs.isInt() && rhs.isInt())
				{
					pushStack(Value(lhs.as.intNumber % rhs.as.intNumber));
				}
				else if (lhs.isFloat() && rhs.isFloat())
				{
					pushStack(Value(fmod(lhs.as.floatNumber, rhs.as.floatNumber)));
				}
				else if (lhs.isFloat() && rhs.isInt())
				{
					pushStack(Value(fmod(lhs.as.floatNumber, static_cast<Float>(rhs.as.intNumber))));
				}
				else if (lhs.isInt() && rhs.isFloat())
				{
					pushStack(Value(fmod(static_cast<Float>(lhs.as.intNumber), rhs.as.floatNumber)));
				}
				else
				{
					return fatalError("no operator % for these types");
				}
			}
				break;
		}

#define BINARY_COMPARASION_OP(op, overloadNameString) \
	{ \
		Value lhs = peekStack(1); \
		Value rhs = peekStack(0); \
		if (lhs.isObj()) \
		{ \
			if (rhs.isObj() && lhs.as.obj->isString() && rhs.as.obj->isString()) \
			{ \
				auto left = lhs.as.obj->asString(); \
				auto right = rhs.as.obj->asString(); \
				popStack(); \
				popStack(); \
				pushStack(Value(Utf8::strcmp(left->chars, left->size, right->chars, right->size) op 0)); \
			} \
			else if (lhs.as.obj->isInstance()) \
			{ \
				auto instance = lhs.as.obj->asInstance(); \
				auto optMethod = instance->class_->methods.get(overloadNameString); \
				if (optMethod.has_value()) \
				{ \
					if (callValue(**optMethod, 2, 0) == Result::RuntimeError) \
					{ \
						return Result::RuntimeError; \
					} \
				} \
				else \
				{ \
					return fatalError("no operator" #op " for these types"); \
				} \
			} \
		} \
		else \
		{ \
			popStack(); \
			popStack(); \
			if (lhs.isInt() && rhs.isInt()) \
			{ \
				pushStack(Value(lhs.as.intNumber op rhs.as.intNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isFloat()) \
			{ \
				pushStack(Value(lhs.as.floatNumber op rhs.as.floatNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isInt()) \
			{ \
				pushStack(Value(lhs.as.floatNumber op static_cast<Float>(rhs.as.intNumber))); \
			} \
			else if (lhs.isInt() && rhs.isFloat()) \
			{ \
				pushStack(Value(static_cast<Float>(lhs.as.intNumber) op rhs.as.floatNumber)); \
			} \
			else \
			{ \
				return fatalError("no operator " #op " for these types"); \
			} \
		} \
		break; \
	}

		case Op::Less: BINARY_COMPARASION_OP(<, m_ltString)
		case Op::LessEqual: BINARY_COMPARASION_OP(<=, m_leString)
		case Op::More: BINARY_COMPARASION_OP(>, m_gtString)
		case Op::MoreEqual: BINARY_COMPARASION_OP(>=, m_geString)
#undef BINARY_COMPARASION_OP

		case Op::Equals:
		{
			auto& lhs = peekStack(1);
			auto& rhs = peekStack(0);
			Value result(lhs == rhs);
			popStack();
			popStack();
			pushStack(result);
			break;
		}

		case Op::Concat:
		{
			Value lhs = peekStack(1);
			Value rhs = peekStack(0);
			std::stringstream result;
			// Could optmize this by not recalculating the length using Utf8::strlen every time.
			// Could also create an stream for ObjString* objects to avoid pointless allocating in std::stringstream.
			result << lhs << rhs;
			ObjString* string = m_allocator->allocateString(result.str());
			popStack();
			popStack();
			pushStack(Value(reinterpret_cast<Obj*>(string)));
			break;
		}

		case Op::LoadConstant:
		{
			const auto constantIndex = readUint32();
			pushStack(m_allocator->getConstant(constantIndex));
			break;
		}

		case Op::LoadLocal:
		{
			size_t stackOffset = (size_t)readUint32();
			pushStack(callStackTop().values[stackOffset]);
			break;
		}

		case Op::SetLocal:
		{
			size_t stackOffset = (size_t)readUint32();
			callStackTop().values[stackOffset] = peekStack(0);
			break;
		}

		case Op::CreateGlobal:
		{
			// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
			ObjString* name = reinterpret_cast<ObjString*>(peekStack(0).as.obj);
			Value value = peekStack(1);
			bool doesNotAlreadyExist = m_globals.insert(*m_allocator, name, value);
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
			const auto value = m_globals.get(name);
			if (value.has_value() == false)
			{
				return fatalError("'%s' is not defined", name->chars);
			}
			popStack();
			pushStack(**value);
			break;
		}

		case Op::SetGlobal:
		{
			// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
			ObjString* name = reinterpret_cast<ObjString*>(peekStack(0).as.obj);
			Value value = peekStack(1);
			bool doesNotAlreadyExist = m_globals.insert(*m_allocator, name, value);
			if (doesNotAlreadyExist)
			{
				m_errorPrinter->outStream() << '\'' << name->chars << "' is not defined\n";
				return Result::RuntimeError;
			}
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

			if (callValue(calleValue, argCount, 1 /* pop callValue */) == Result::RuntimeError)
			{
				return Result::RuntimeError;
			}
			break;
		}

		case Op::Print:
		{
			std::cout << peekStack();
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
				m_stackTop = callStackTop().values;
				m_stackTop -= callStackTop().numberOfValuesToPopOffExceptArgs;
				m_callStackSize--;
				pushStack(result);
			}
			break;
		}

		case Op::Jump:
		{
			size_t jump = readUint32();
			callStackTop().instructionPointer += jump;
			break;
		}

		case Op::JumpBack:
		{
			size_t jump = readUint32();
			callStackTop().instructionPointer -= jump;
			break;
		}

		case Op::JumpIfFalseAndPop:
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
			popStack();
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

		case Op::CreateClass:
		{
			auto nameValue = peekStack(0);

			ASSERT((nameValue.type == ValueType::Obj) && (nameValue.as.obj->isString()));
			auto name = nameValue.as.obj->asString();
			auto class_ = m_allocator->allocateClass(name);
			popStack();
			class_->name = name;
			pushStack(Value(reinterpret_cast<Obj*>(class_)));
			break;
		}

		case Op::GetProperty:
		{
			auto fieldNameValue = peekStack(0);
			ASSERT(fieldNameValue.as.obj->isString());
			auto fieldName = fieldNameValue.as.obj->asString();
			auto lhs = peekStack(1);
			if ((lhs.isObj() == false))
			{
				return fatalError("can only use field access operator on objects");
			}
			auto obj = lhs.as.obj;
			if (obj->isInstance())
			{
				auto instance = obj->asInstance();
				auto optResult = instance->fields.get(fieldName);
				
				if (optResult.has_value())
				{
					popStack();
					popStack();
					pushStack(**optResult);
				}
				else
				{
					auto optMethod = instance->class_->methods.get(fieldName);
					if (optMethod.has_value())
					{
						ASSERT((*optMethod)->isObj());
						ASSERT((*optMethod)->as.obj->type == ObjType::Function);
						auto function = reinterpret_cast<ObjFunction*>((*optMethod)->as.obj);
						auto method = m_allocator->allocateBoundFunction(function, instance);
						popStack();
						popStack();
						pushStack(Value(reinterpret_cast<Obj*>(method)));
					}
					else
					{
						// TODO Maybe don't allow access of non existent fields. Then I would have to add a special method for checking
						// if the field exists.
						popStack();
						popStack();
						pushStack(Value::null());
					}

				}
			}
			else if (obj->isClass())
			{
				auto class_ = obj->asClass();
				auto optResult = class_->fields.get(fieldName);
				popStack();
				popStack();
				if (optResult.has_value())
				{
					pushStack(**optResult);
				}
				else
				{
					// TODO Maybe don't allow access of non existent fields. Then I would have to add a special method for checking
					// if the field exists.
					pushStack(Value::null());
				}
			}
			else
			{
				return fatalError("cannot use field access operator on this kind of object");
			}
			break;
		}

		case Op::SetProperty:
		{
			auto fieldNameValue = peekStack(0);
			ASSERT(fieldNameValue.as.obj->isString());
			auto fieldName = fieldNameValue.as.obj->asString();
			auto lhs = peekStack(1);
			auto rhs = peekStack(2);
			if ((lhs.isObj() == false))
			{
				return fatalError("can only use field access operator on objects");
			}
			auto obj = lhs.as.obj;
			if (obj->isInstance())
			{
				auto instance = obj->asInstance();
				instance->fields.insert(*m_allocator, fieldName, rhs);
				popStack();
				popStack();
			}
			else if (obj->isClass())
			{
				auto class_ = obj->asClass();
				class_->fields.insert(*m_allocator, fieldName, rhs);
				popStack();
				popStack();
			}
			else
			{
				return fatalError("cannot use field access operator on this kind of object");
			}
			break;
		}

		case Op::StoreMethod:
		{
			auto methodNameValue = peekStack(0);
			ASSERT(methodNameValue.as.obj->isString());
			auto fieldName = methodNameValue.as.obj->asString();
			auto classValue = peekStack(2);
			auto methodValue = peekStack(1);
			
			ASSERT(classValue.isObj());
			ASSERT(classValue.as.obj->isClass());
			auto class_ = classValue.as.obj->asClass();
			class_->methods.insert(*m_allocator, fieldName, methodValue);
			popStack();
			popStack();
			break;
		}

		case Op::TryBegin:
		{
			auto jump = readUint32();
			callStackTop().isTrySet = true;
			callStackTop().absoluteJumpToCatch = jump;
			break;
		}

		case Op::TryEnd:
			callStackTop().isTrySet = false;
			break;

		case Op::Throw:
		{
			auto& thrownValue = peekStack();

			while (m_callStackSize != 0)
			{
				auto& frame = callStackTop();

				if (frame.isTrySet)
				{
					frame.instructionPointer = &frame.function->byteCode.code[frame.absoluteJumpToCatch];
					frame.isTrySet = false;
					m_stackTop = callStackTop().values;
					m_stackTop -= callStackTop().numberOfValuesToPopOffExceptArgs;
					pushStack(thrownValue);
					goto breakLabel;
				}

				m_callStackSize--;
			}
			m_callStackSize++;
			return fatalError("uncaught exception");
		breakLabel:
			break;
		}

		default:
			ASSERT_NOT_REACHED();
			return Result::RuntimeError;
		}
	}
}

void Vm::createForeignFunction(std::string_view name, ForeignFunction function, int argCount)
{
	auto nameObj = m_allocator->allocateString(name);
	auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount);
	m_globals.insert(*m_allocator, nameObj, Value(reinterpret_cast<Obj*>(functionObj)));
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
	if (m_stackTop == m_stack.data() + m_stack.size())
	{
		ASSERT_NOT_REACHED();
	}
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

Vm::Result Vm::callValue(Value value, int argCount, int numberOfValuesToPopOffExceptArgs)
{
	if (value.isObj() == false)
	{
		return fatalError("type is not calable\n");
	}
	auto obj = value.as.obj;
	switch (obj->type)
	{
		case ObjType::Function:
		{
			auto calle = obj->asFunction();

			if (argCount != calle->argCount)
			{
				return fatalError("expected %d arguments but got %d", calle->argCount, argCount);
			}
			if ((m_callStackSize + 1) == m_callStack.size())
			{
				return fatalError("call stack overflow");
			}
			m_callStack[m_callStackSize] = CallFrame{ 
				calle->byteCode.code.data(), 
				m_stackTop - argCount, 
				calle, 
				numberOfValuesToPopOffExceptArgs 
			};
			m_callStackSize++;
			break;
		}

		case ObjType::ForeignFunction:
		{
			auto calle = obj->asForeignFunction();
			if (argCount != calle->argCount)
			{
				return fatalError("expected %d arguments but got %d", calle->argCount, argCount);
			} 
			auto result = calle->function(m_stackTop - argCount, argCount);
			m_stackTop -= numberOfValuesToPopOffExceptArgs + static_cast<size_t>(argCount);
			pushStack(result);
			break;
		}

		case ObjType::Class:
		{
			auto calle = obj->asClass();
			auto instance = m_allocator->allocateInstance(calle);
			instance->class_ = calle;
			auto optInitalizer = calle->methods.get(m_initString);
			auto hashTable = instance->class_->methods;
			for (size_t i = 0; i < hashTable.capacity(); i++)
			{
				auto& bucket = hashTable.data()[i];

				if (hashTable.isBucketEmpty(bucket) == false)
				{
					//std::cout << "key: " << reinterpret_cast<Obj*>(bucket.key) << " value: " << bucket.value << '\n';
				}
			}
			ASSERT(numberOfValuesToPopOffExceptArgs == 1);
			m_stackTop[-argCount - 1] = Value(reinterpret_cast<Obj*>(instance)); // Replace the class with the instance.
			if (optInitalizer.has_value())
			{
				if (callValue(**optInitalizer, argCount + 1, 0) == Result::RuntimeError)
				{
					return Result::RuntimeError;
				}
			}
			else if (argCount != 0)
			{
				return fatalError("expected 0 args but got %d", argCount);
			}
			break;
		}

		case ObjType::BoundFunction:
		{
			auto calle = obj->asBoundFunction();
			m_stackTop[-argCount - 1] = Value(reinterpret_cast<Obj*>(calle->instance));
			auto function = calle->function;
			if (callValue(Value(reinterpret_cast<Obj*>(function)), argCount + 1, 0) == Result::RuntimeError)
			{
				return Result::RuntimeError;
			}
			break;
		}

		default:
			return fatalError("type is not calable\n");
	}

	return Result::Success;
}

void Vm::mark(Vm* vm, Allocator& allocator)
{
	for (auto value = vm->m_stack.data(); value != vm->m_stackTop; value++)
	{
		allocator.addValue(*value);
	}

	allocator.addHashTable(vm->m_globals);

	for (auto frame = vm->m_callStack.data(); frame != &vm->callStackTop() + 1; frame++)
	{
		allocator.addObj(reinterpret_cast<Obj*>(frame->function));
	}
}

void Vm::update(Vm* vm)
{
	for (auto value = vm->m_stack.data(); value != vm->m_stackTop; value++)
	{
		Allocator::updateValue(*value);
	}

	Allocator::updateHashTable(vm->m_globals);

	for (auto frame = vm->m_callStack.data(); frame != &vm->callStackTop() + 1; frame++)
	{
		frame->function = reinterpret_cast<ObjFunction*>(Allocator::newObjLocation(reinterpret_cast<Obj*>(frame->function)));
	}
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