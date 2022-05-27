#include "Vm.hpp"
#include <Vm/Vm.hpp>
#include <Vm/List.hpp>
#include <Utf8.hpp>
#include <Debug/DebugOptions.hpp>
#include <Asserts.hpp>
#include <iostream>
#include <sstream>

#ifdef VOXL_DEBUG_PRINT_VM_EXECUTION_TRACE
	#include <Debug/Disassembler.hpp>
#endif

using namespace Lang;

static VOXL_NATIVE_FN(floor)
{
	if (args[0].isInt())
	{
		return args[0];
	}
	if (args[0].isFloat())
	{
		return Value(static_cast<Int>(::floor(args[0].as.floatNumber)));
	}
	return NativeFunctionResult::exception(Value::null());
}

#define TRY(somethingThatReturnsResult) \
	if (somethingThatReturnsResult == Result::RuntimeError) \
	{ \
		return Result::RuntimeError; \
	}

#define TRY_PUSH(value) \
	if (m_stack.push(value) == false) \
	{ \
		return fatalError("stack overflow"); \
	}

#define TRY_PUSH_CALL_STACK() \
	if (m_callStack.push() == false) \
	{ \
		return fatalError("call stack overflow"); \
	}

Vm::Vm(Allocator& allocator)
	: m_allocator(&allocator)
	, m_errorPrinter(nullptr)
	, m_rootMarkingFunctionHandle(allocator.registerMarkingFunction(this, mark))
	, m_initString(m_allocator->allocateStringConstant("$init").value)
	, m_addString(m_allocator->allocateStringConstant("$add").value)
	, m_subString(m_allocator->allocateStringConstant("$sub").value)
	, m_mulString(m_allocator->allocateStringConstant("$mul").value)
	, m_divString(m_allocator->allocateStringConstant("$div").value)
	, m_modString(m_allocator->allocateStringConstant("$mod").value)
	, m_ltString(m_allocator->allocateStringConstant("$lt").value)
	, m_leString(m_allocator->allocateStringConstant("$le").value)
	, m_gtString(m_allocator->allocateStringConstant("$gt").value)
	, m_geString(m_allocator->allocateStringConstant("$ge").value)
	, m_getIndexString(m_allocator->allocateStringConstant("$get_index").value)
	, m_setIndexString(m_allocator->allocateStringConstant("$set_index").value)
{

	defineNativeFunction("floor", floor, 1);

	auto listString = m_allocator->allocateStringConstant("List").value;
	m_listType = m_allocator->allocateClass(listString, sizeof(List), reinterpret_cast<MarkingFunction>(List::mark));	

	auto addFn = [this](std::string_view name, NativeFunction function, int argCount)
	{
		auto nameObj = m_allocator->allocateString(name);
		auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount);
		m_listType->fields.set(nameObj, Value(reinterpret_cast<Obj*>(functionObj)));
	};
	addFn("$init", List::init, 1);
	addFn("push", List::push, 2);
	addFn("size", List::get_size, 1);
	addFn("$get_index", List::get_index, 2);
	addFn("$set_index", List::set_index, 3);
	reset();
}

Vm::Result Vm::execute(ObjFunction* program, ErrorPrinter& errorPrinter)
{
	m_callStack.clear();
	m_stack.clear();
	m_errorPrinter = &errorPrinter;

	TRY_PUSH_CALL_STACK();
	auto& frame = m_callStack.top();
	frame.instructionPointer = program->byteCode.code.data();
	frame.values = m_stack.topPtr;
	frame.function = program;
	frame.isTrySet = false;
	frame.numberOfValuesToPopOffExceptArgs = 0;

	const auto result = run();
	if (result == Result::Success)
	{
		// The program should always finish without anything on both the excecution and call stack.
		ASSERT(m_callStack.isEmpty() && m_stack.isEmpty());
	}
	return result;
}

void Vm::reset()
{
	m_globals.clear();
	auto listString = m_allocator->allocateStringConstant("List").value;
	m_globals.set(listString, Value(reinterpret_cast<Obj*>(m_listType)));
	defineNativeFunction("floor", floor, 1);
}

Vm::Result Vm::run()
{
	for (;;)
	{
	#ifdef VOXL_DEBUG_PRINT_VM_EXECUTION_TRACE
		std::cout << "[ ";
		for (const auto& value : m_stack)
		{
			debugPrintValue(value);
			std::cout << ' ';
		}
		std::cout << "]\n";
		disassembleInstruction(
			m_callStack.top().function->byteCode,
			m_callStack.top().instructionPointer - m_callStack.top().function->byteCode.code.data(), *m_allocator);
		std::cout << '\n';
	#endif

	#ifdef VOXL_DEBUG_STRESS_TEST_GC
		m_allocator->runGc();
	#endif

		auto op = static_cast<Op>(*m_callStack.top().instructionPointer);
		m_callStack.top().instructionPointer++;

		switch (op)
		{
// TODO:
// Could make this better by not popping of both values and just replacing them.
// Also it uses values instead of references for peeking.
#define BINARY_ARITHMETIC_OP(op, overloadNameString) \
	{ \
		Value lhs = m_stack.peek(1); \
		Value rhs = m_stack.peek(0); \
		if (lhs.isObj() && lhs.as.obj->isInstance()) \
		{ \
			auto instance = lhs.as.obj->asInstance(); \
			auto optMethod = instance->class_->fields.get(overloadNameString); \
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
			m_stack.pop(); \
			m_stack.pop(); \
			if (lhs.isInt() && rhs.isInt()) \
			{ \
				TRY_PUSH(Value(lhs.as.intNumber op rhs.as.intNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isFloat()) \
			{ \
				TRY_PUSH(Value(lhs.as.floatNumber op rhs.as.floatNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isInt()) \
			{ \
				TRY_PUSH(Value(lhs.as.floatNumber op static_cast<Float>(rhs.as.intNumber))); \
			} \
			else if (lhs.isInt() && rhs.isFloat()) \
			{ \
				TRY_PUSH(Value(static_cast<Float>(lhs.as.intNumber) op rhs.as.floatNumber)); \
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
#undef BINARY_ARITHMETIC_OP
			// TODO: handle integer division by zero
		case Op::Divide: 
		{
			Value lhs = m_stack.peek(1);
			Value rhs = m_stack.peek(0);
			if (lhs.isObj() && lhs.as.obj->isInstance())
			{
				auto instance = lhs.as.obj->asInstance();
				auto optMethod = instance->class_->fields.get(m_modString);
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
				m_stack.pop();
				m_stack.pop();
				if (lhs.isInt() && rhs.isInt())
				{
					TRY_PUSH(Value(static_cast<Float>(lhs.as.intNumber) / static_cast<Float>(rhs.as.intNumber)));
				}
				else if (lhs.isFloat() && rhs.isFloat())
				{
					TRY_PUSH(Value(lhs.as.floatNumber /rhs.as.floatNumber));
				}
				else if (lhs.isFloat() && rhs.isInt())
				{
					TRY_PUSH(Value(lhs.as.floatNumber / static_cast<Float>(rhs.as.intNumber)));
				}
				else if (lhs.isInt() && rhs.isFloat())
				{
					TRY_PUSH(Value(static_cast<Float>(lhs.as.intNumber) / rhs.as.floatNumber));
				}
				else
				{
					return fatalError("no operator % for these types");
				}
			}
			break;
		}
		case Op::Modulo: 
		{
			Value lhs = m_stack.peek(1);
			Value rhs = m_stack.peek(0);
			if (lhs.isObj() && lhs.as.obj->isInstance())
			{
				auto instance = lhs.as.obj->asInstance();
				auto optMethod = instance->class_->fields.get(m_modString);
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
				m_stack.pop();
				m_stack.pop();
				if (lhs.isInt() && rhs.isInt())
				{
					TRY_PUSH(Value(lhs.as.intNumber % rhs.as.intNumber));
				}
				else if (lhs.isFloat() && rhs.isFloat())
				{
					TRY_PUSH(Value(fmod(lhs.as.floatNumber, rhs.as.floatNumber)));
				}
				else if (lhs.isFloat() && rhs.isInt())
				{
					TRY_PUSH(Value(fmod(lhs.as.floatNumber, static_cast<Float>(rhs.as.intNumber))));
				}
				else if (lhs.isInt() && rhs.isFloat())
				{
					TRY_PUSH(Value(fmod(static_cast<Float>(lhs.as.intNumber), rhs.as.floatNumber)));
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
		Value lhs = m_stack.peek(1); \
		Value rhs = m_stack.peek(0); \
		if (lhs.isObj()) \
		{ \
			if (rhs.isObj() && lhs.as.obj->isString() && rhs.as.obj->isString()) \
			{ \
				auto left = lhs.as.obj->asString(); \
				auto right = rhs.as.obj->asString(); \
				m_stack.pop(); \
				m_stack.pop(); \
				TRY_PUSH(Value(Utf8::strcmp(left->chars, left->size, right->chars, right->size) op 0)); \
			} \
			else if (lhs.as.obj->isInstance()) \
			{ \
				auto instance = lhs.as.obj->asInstance(); \
				auto optMethod = instance->class_->fields.get(overloadNameString); \
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
			m_stack.pop(); \
			m_stack.pop(); \
			if (lhs.isInt() && rhs.isInt()) \
			{ \
				TRY_PUSH(Value(lhs.as.intNumber op rhs.as.intNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isFloat()) \
			{ \
				TRY_PUSH(Value(lhs.as.floatNumber op rhs.as.floatNumber)); \
			} \
			else if (lhs.isFloat() && rhs.isInt()) \
			{ \
				TRY_PUSH(Value(lhs.as.floatNumber op static_cast<Float>(rhs.as.intNumber))); \
			} \
			else if (lhs.isInt() && rhs.isFloat()) \
			{ \
				TRY_PUSH(Value(static_cast<Float>(lhs.as.intNumber) op rhs.as.floatNumber)); \
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
			auto& lhs = m_stack.peek(1);
			auto& rhs = m_stack.peek(0);
			Value result(lhs == rhs);
			m_stack.pop();
			m_stack.pop();
			TRY_PUSH(result);
			break;
		}

		case Op::NotEquals:
		{
			auto& lhs = m_stack.peek(1);
			auto& rhs = m_stack.peek(0);
			Value result(!(lhs == rhs));
			m_stack.pop();
			m_stack.pop();
			TRY_PUSH(result);
			break;
		}

		case Op::Concat:
		{
			Value lhs = m_stack.peek(1);
			Value rhs = m_stack.peek(0);
			std::stringstream result;
			// Could optmize this by not recalculating the length using Utf8::strlen every time.
			// Could also create an stream for ObjString* objects to avoid pointless allocating in std::stringstream.
			// Could store a reserved stringstream inside the vm. There may be an issue if a native function is called
			// which then calls a normal function which calls an overload for printing.
			result << lhs << rhs;
			ObjString* string = m_allocator->allocateString(result.str());
			m_stack.pop();
			m_stack.pop();
			TRY_PUSH(Value(reinterpret_cast<Obj*>(string)));
			break;
		}

		case Op::Negate:
		{
			auto& value = m_stack.peek(0);
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
			auto& value = m_stack.peek(0);
			if (value.isBool())
			{
				value.as.boolean = !value.as.boolean;
			}
			else
			{
				return fatalError("is not bool");
			}
			break;
		}

		case Op::GetConstant:
		{
			const auto constantIndex = readUint32();
			TRY_PUSH(m_allocator->getConstant(constantIndex));
			break;
		}

		case Op::GetLocal:
		{
			auto stackOffset = readUint32();
			TRY_PUSH(m_callStack.top().values[stackOffset]);
			break;
		}

		case Op::SetLocal:
		{
			auto stackOffset = readUint32();
			m_callStack.top().values[stackOffset] = m_stack.peek(0);
			break;
		}

		case Op::CreateGlobal:
		{
			// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
			ObjString* name = reinterpret_cast<ObjString*>(m_stack.peek(0).as.obj);
			Value value = m_stack.peek(1);
			bool doesNotAlreadyExist = m_globals.set(name, value);
			if (doesNotAlreadyExist == false)
			{
				return fatalError("redeclaration of '%s'", name->chars);
			}
			m_stack.pop();
			m_stack.pop();
			break;
		}

		case Op::GetGlobal:
		{
			ObjString* name = reinterpret_cast<ObjString*>(m_stack.peek(0).as.obj);
			const auto value = m_globals.get(name);
			if (value.has_value() == false)
			{
				return fatalError("'%s' is not defined", name->chars);
			}
			m_stack.pop();
			TRY_PUSH(**value);
			break;
		}

		case Op::SetGlobal:
		{
			// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
			ObjString* name = reinterpret_cast<ObjString*>(m_stack.peek(0).as.obj);
			Value value = m_stack.peek(1);
			bool doesNotAlreadyExist = m_globals.set(name, value);
			if (doesNotAlreadyExist)
			{
				m_errorPrinter->outStream() << '\'' << name->chars << "' is not defined\n";
				return Result::RuntimeError;
			}
			m_stack.pop();
			break;
		}

		case Op::GetUpvalue:
		{
			const auto index = readUint32();
			TRY_PUSH(*m_callStack.top().upvalues[index]->location);
			break;
		}

		case Op::SetUpvalue:
		{
			const auto index = readUint32();
			*m_callStack.top().upvalues[index]->location = m_stack.peek(0);
			break;
		}

		case Op::GetField:
		{
			auto fieldName = m_stack.peek(0).as.obj->asString();
			auto lhs = m_stack.peek(1);

			if (auto class_ = getClassOrNullptr(lhs); class_ != nullptr)
			{
				if (auto function = class_->fields.get(fieldName);
					function.has_value() && (*function)->isObj()
					&& ((*function)->as.obj->isFunction() || (*function)->as.obj->isNativeFunction()))
				{
					auto boundFunction = m_allocator->allocateBoundFunction((*function)->as.obj, lhs);
					m_stack.pop();
					m_stack.pop();
					TRY_PUSH(Value(reinterpret_cast<Obj*>(boundFunction)));
					break;
				}
			}

			if ((lhs.isObj() == false))
			{
				return fatalError("type has no field '%s'", fieldName->chars);
			}

			auto obj = lhs.as.obj;
			m_stack.pop();
			m_stack.pop();
			if (obj->isInstance())
			{
				if (auto value = obj->asInstance()->fields.get(fieldName); value.has_value())
				{
					TRY_PUSH(**value);
				}
				else
				{
					TRY_PUSH(Value::null());
				}
				break;
			}
			else if (obj->isClass())
			{
				if (auto value = obj->asClass()->fields.get(fieldName); value.has_value())
				{
					TRY_PUSH(**value);
				}
				else
				{
					TRY_PUSH(Value::null());
				}
				break;
			}

			return fatalError("type has no field '%s'", fieldName->chars);
			break;
		}

		case Op::SetField:
		{
			auto fieldName = m_stack.peek(0).as.obj->asString();
			auto lhs = m_stack.peek(1);
			auto rhs = m_stack.peek(2);
			if ((lhs.isObj() == false))
			{
				return fatalError("cannot use field access on this type");
			}
			auto obj = lhs.as.obj;
			m_stack.pop();
			m_stack.pop();
			if (obj->isInstance())
			{
				obj->asInstance()->fields.set(fieldName, rhs);
				break;
			}
			else if (obj->isClass())
			{
				obj->asClass()->fields.set(fieldName, rhs);
				break;
			}

			return fatalError("cannot use field access on this type");
			break;
		}

		case Op::StoreMethod:
		{
			auto methodNameValue = m_stack.peek(0);
			ASSERT(methodNameValue.as.obj->isString());
			auto fieldName = methodNameValue.as.obj->asString();
			auto classValue = m_stack.peek(2);
			auto methodValue = m_stack.peek(1);

			ASSERT(classValue.isObj() && classValue.as.obj->isClass());
			auto class_ = classValue.as.obj->asClass();
			class_->fields.set(fieldName, methodValue);
			m_stack.pop();
			m_stack.pop();
			break;
		}

		case Op::GetIndex:
		{
			auto& value = m_stack.peek(1);
			if (auto class_ = getClassOrNullptr(value); class_ != nullptr)
			{
				auto getIndexFunction = class_->fields.get(m_getIndexString);
				if (getIndexFunction.has_value() == false)
				{
					return fatalError("type doesn't define an index function");
				}
				if (callValue(**getIndexFunction, 2, 0) == Result::RuntimeError)
				{
					return Result::RuntimeError;
				}
			}
			break;
		}

		case Op::SetIndex:
		{
			auto& value = m_stack.peek(1);
			if (auto class_ = getClassOrNullptr(value); class_ != nullptr)
			{
				auto setIndexFunction = class_->fields.get(m_setIndexString);
				if (setIndexFunction.has_value() == false)
				{
					return fatalError("type doesn't define an set index function");
				}
				if (callValue(**setIndexFunction, 3, 0) == Result::RuntimeError)
				{
					return Result::RuntimeError;
				}
			}
			else
			{
				return fatalError("type doesn't define an set index function");
			}
			break;
		}

		case Op::LoadNull:
			TRY_PUSH(Value::null());
			break;

		case Op::LoadTrue:
			TRY_PUSH(Value(true));
			break;

		case Op::LoadFalse:
			TRY_PUSH(Value(false));
			break;

		case Op::Jump:
		{
			auto jump = readUint32();
			m_callStack.top().instructionPointer += jump;
			break;
		}

		case Op::JumpIfTrue:
		{
			const auto jump = readUint32();
			const auto& value = m_stack.peek(0);
			if ((value.type != ValueType::Bool))
			{
				return fatalError("must be bool");
			}
			if (value.as.boolean)
			{
				m_callStack.top().instructionPointer += jump;
			}
			break;
		}

		case Op::JumpIfFalse:
		{
			const auto jump = readUint32();
			const auto& value = m_stack.peek(0);
			if ((value.type != ValueType::Bool))
			{
				return fatalError("must be bool");
			}
			if (value.as.boolean == false)
			{
				m_callStack.top().instructionPointer += jump;
			}
			break;
		}

		case Op::JumpIfFalseAndPop:
		{
			const auto jump = readUint32();
			const auto& value = m_stack.peek(0);
			if ((value.type != ValueType::Bool))
			{
				return fatalError("must be bool");
			}
			if (value.as.boolean == false)
			{
				m_callStack.top().instructionPointer += jump;
			}
			m_stack.pop();
			break;
		}

		case Op::JumpBack:
		{
			const auto jump = readUint32();
			m_callStack.top().instructionPointer -= jump;
			break;
		}

		case Op::Call:
		{
			const auto argCount = readUint32();
			const auto& calleValue = m_stack.peek(argCount);
			TRY(callValue(calleValue, argCount, 1 /* pop callValue */));
			break;
		}

		case Op::Print:
		{
			std::cout << m_stack.peek(0);
			break;
		}

		case Op::PopStack:
		{
			m_stack.pop();
			break;
		}

		case Op::Return:
		{
			if (m_callStack.size() == 1)
			{
				m_callStack.pop();
				return Result::Success;
			}
			else
			{
				const auto& result = m_stack.peek(0);
				const auto& frame = m_callStack.top();
				m_stack.topPtr = frame.values - frame.numberOfValuesToPopOffExceptArgs;

				auto isLocal = [this, &frame](ObjUpvalue* upvalue)
				{
					return upvalue->location >= frame.values;
				};

				for (;;)
				{
					auto it = std::find_if(m_openUpvalues.begin(), m_openUpvalues.end(), isLocal);
					if (it == m_openUpvalues.end())
					{
						break;
					}
					auto upvalue = *it;
					upvalue->value = *upvalue->location;
					upvalue->location = &upvalue->value;
					m_openUpvalues.erase(it);
				}

				m_callStack.pop();
				TRY_PUSH(result);
			}
			break;
		}

		case Op::CreateClass:
		{
			auto nameValue = m_stack.peek(0);

			ASSERT((nameValue.type == ValueType::Obj) && (nameValue.as.obj->isString()));
			auto name = nameValue.as.obj->asString();
			auto class_ = m_allocator->allocateClass(name, 0, nullptr);
			m_stack.pop();
			TRY_PUSH(Value(reinterpret_cast<Obj*>(class_)));
			break;
		}

		case Op::TryBegin:
		{
			auto jump = readUint32();
			m_callStack.top().isTrySet = true;
			m_callStack.top().absoluteJumpToCatch = jump;
			break;
		}

		case Op::TryEnd:
			m_callStack.top().isTrySet = false;
			break;

		case Op::Throw:
		{
			TRY(throwValue(m_stack.peek(0)));
			break;
		}

		case Op::Closure:
		{
			auto function = m_stack.peek(0).as.obj->asFunction();
			auto closure = m_allocator->allocateClosure(function);
			const auto upvalueCount = readUint8();
			for (uint8_t i = 0; i < upvalueCount; i++)
			{
				const auto index = readUint8();
				const auto isLocal = readUint8();

				if (isLocal)
				{
					auto upvalue = m_allocator->allocateUpvalue(m_callStack.top().values + index);
					m_openUpvalues.push_back(upvalue);
					closure->upvalues[i] = upvalue;
				}
				else
				{
					closure->upvalues[i] = m_callStack.top().upvalues[index];
				}
			}
			m_stack.pop();
			TRY_PUSH(Value(reinterpret_cast<Obj*>(closure)));
			break;
		}

		case Op::CloseUpvalue:
		{
			const auto index = readUint8();
			for (auto it = m_openUpvalues.begin(); it != m_openUpvalues.end(); it++)
			{
				auto& upvalue = *it;
				auto& local = m_callStack.top().values[index];
				if (upvalue->location == &local)
				{
					upvalue->value = local;
					upvalue->location = &upvalue->value;
					m_openUpvalues.erase(it);
					break;
				}
			}
			break;
		}

		default:
			ASSERT_NOT_REACHED();
			return Result::RuntimeError;
		}
	}
}

void Vm::defineNativeFunction(std::string_view name, NativeFunction function, int argCount)
{
	auto nameObj = m_allocator->allocateString(name);
	auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount);
	m_globals.set(nameObj, Value(reinterpret_cast<Obj*>(functionObj)));
}

uint8_t Vm::readUint8()
{
	uint8_t value = *m_callStack.top().instructionPointer;
	m_callStack.top().instructionPointer++;
	return value;
}

Vm::Result Vm::fatalError(const char* format, ...)
{
	char errorBuffer[256];
	va_list args;
	va_start(args, format);
	vsnprintf(errorBuffer, sizeof(errorBuffer), format, args);
	va_end(args);
	m_errorPrinter->outStream() << "fatal runtime error: " << errorBuffer << '\n';
	for (auto& frame = m_callStack.crbegin(); frame != m_callStack.crend(); ++frame)
	{
		const auto function = frame->function;
		const auto instructionOffset = frame->instructionPointer - function->byteCode.code.data();
		const auto lineNumber = function->byteCode.lineNumberAtOffset[instructionOffset] + 1;
		m_errorPrinter->outStream() << "line " << lineNumber << " in " << function->name->chars << "()\n";
	}
	return Result::RuntimeError;
}

Vm::Result Vm::callObjFunction(ObjFunction* function, int argCount, int numberOfValuesToPopOffExceptArgs)
{
	if (argCount != function->argCount)
	{
		return fatalError("expected %d arguments but got %d", function->argCount, argCount);
	}

	TRY_PUSH_CALL_STACK();
	auto& frame = m_callStack.top();
	frame.instructionPointer = function->byteCode.code.data();
	frame.values = m_stack.topPtr - argCount;
	frame.function = function;
	frame.numberOfValuesToPopOffExceptArgs = numberOfValuesToPopOffExceptArgs;
	frame.isTrySet = false;
	return Result::Success;
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
			// callObjFunction is only used in this function so I could implement it as a goto or a lambda.
			TRY(callObjFunction(obj->asFunction(), argCount, numberOfValuesToPopOffExceptArgs));
			break;
		}

		case ObjType::Closure:
		{
			auto closure = obj->asClosure();
			TRY(callObjFunction(closure->function, argCount, numberOfValuesToPopOffExceptArgs));
			m_callStack.top().upvalues = closure->upvalues;
			break;
		}

		case ObjType::NativeFunction:
		{
			auto calle = obj->asNativeFunction();
			if (argCount != calle->argCount)
			{
				return fatalError("expected %d arguments but got %d", calle->argCount, argCount);
			} 
			TRY_PUSH_CALL_STACK();
			auto& frame = m_callStack.top();
			frame.setNativeFunction();
			const auto result = calle->function(m_stack.topPtr - argCount, argCount, *this, *m_allocator);
			m_callStack.pop();

			switch (result.type)
			{
				case NativeFunctionResultType::Ok:
					m_stack.topPtr -= numberOfValuesToPopOffExceptArgs + static_cast<size_t>(argCount);
					TRY_PUSH(result.value);
					break;

				case NativeFunctionResultType::Exception:
					TRY(throwValue(result.value));
					break;

				case NativeFunctionResultType::Fatal:
					return Result::RuntimeError;
			}
			break;
		}

		case ObjType::Class:
		{
			// The class gets replaced with the instance so any other case would not work.
			ASSERT(numberOfValuesToPopOffExceptArgs == 1);

			auto calle = obj->asClass();
			auto instance = (calle->instanceSize == 0)
				? reinterpret_cast<Obj*>(m_allocator->allocateInstance(calle))
				: reinterpret_cast<Obj*>(m_allocator->allocateNativeInstance(calle));

			m_stack.topPtr[-argCount - 1] = Value(instance); // Replace the class with the instance.
			if (auto initializer = calle->fields.get(m_initString); initializer.has_value())
			{
				TRY(callValue(**initializer, argCount + 1, 0));
				// Could check if the function returns null like python,
				// or I could just ignore the return value like javascript and it with the instance.
				// Another option would be to check if it return value is the instance or maybe the same type as class.

				// All of these options would require storing something inside the call frame.
				// An option would be to if numberOfValuesToPopOffExceptArgs is negative then
				// don't push the return value.
			}
			else if (argCount != 0)
			{
				return fatalError("expected 0 args but got %d", argCount);
			}
			break;
		}

		case ObjType::BoundFunction:
		{
			auto boundFunction = obj->asBoundFunction();
			// Replace the function with the bound value
			m_stack.topPtr[-argCount - 1] = boundFunction->value;
			if (boundFunction->callable->isBoundFunction())
			{
				return fatalError("cannot bind a function twice");
			}
			TRY(callValue(Value(reinterpret_cast<Obj*>(boundFunction->callable)), argCount + 1, 0))
			break;
		}

		default:
			return fatalError("type is not calable\n");
	}

	return Result::Success;
}

Vm::Result Vm::throwValue(const Value& value)
{
	while (m_callStack.isEmpty() == false)
	{
		auto& frame = m_callStack.top();

		if (frame.isTrySet)
		{
			frame.isTrySet = false;
			frame.instructionPointer = &frame.function->byteCode.code[frame.absoluteJumpToCatch];
			m_stack.topPtr = frame.values - frame.numberOfValuesToPopOffExceptArgs;
			TRY_PUSH(value);
			return Result::Success;
		}

		m_callStack.pop();
	}
	return fatalError("uncaught exception");
}

ObjClass* Vm::getClassOrNullptr(const Value& value)
{
	if (value.isObj())
	{
		if (value.as.obj->isInstance())
			return value.as.obj->asInstance()->class_;
		if (value.as.obj->isNativeInstance())
			return value.as.obj->asNativeInstance()->class_;
	}
	return nullptr;
}

void Vm::mark(Vm* vm, Allocator& allocator)
{
	for (auto value = vm->m_stack.data(); value != vm->m_stack.topPtr; value++)
	{
		allocator.addValue(*value);
	}

	for (auto& value : vm->m_stack)
	{
		allocator.addValue(value);
	}

	allocator.addHashTable(vm->m_globals);
	allocator.addObj(reinterpret_cast<Obj*>(vm->m_listType));

	for (const auto& frame : vm->m_callStack)
	{
		allocator.addObj(reinterpret_cast<Obj*>(frame.function));
	}

	for (auto upvalue : vm->m_openUpvalues)
	{
		allocator.addObj(reinterpret_cast<Obj*>(upvalue));
	}
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

bool Vm::CallFrame::isNativeFunction()
{
	return function == nullptr;
}

void Vm::CallFrame::setNativeFunction()
{
	function = nullptr;
}
