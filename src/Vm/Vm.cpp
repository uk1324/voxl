#include <Vm/Vm.hpp>
#include <Vm/List.hpp>
#include <Utf8.hpp>
#include <Debug/DebugOptions.hpp>
#include <Asserts.hpp>
#include <Context.hpp>
#include <ReadFile.hpp>
#include <Debug/Disassembler.hpp>
#include <Format.hpp>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <stdarg.h>

using namespace Voxl;

// TODO: Calling stack.top requires 2 levles of indirection 
// Using the global variable requires 1 level of indirection but also requires setting and unsetting the pointer on call and return.
// The former is simpler.

#define TRY(somethingThatReturnsResult) \
	{ \
		const auto expressionResult = somethingThatReturnsResult; \
		if (expressionResult.type != ResultType::Ok) \
		{ \
			return expressionResult; \
		} \
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

#define TRY_PUSH_EXCEPTION_HANDLER() \
	if (m_exceptionHandlers.push() == false) \
	{ \
		return fatalError("exception handler stack overflow"); \
	}

Vm::Vm(Allocator& allocator)
	: m_allocator(&allocator)
	, m_errorReporter(nullptr)
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
// TODO: The GC might run during the constructor so the values have to be check for begin null.
// This also may be fixable by making constant classes that are always marked.
// Currently only strings and functions can be constant because they don't contain any changing content.
	, m_listType(nullptr)
	, m_intType(nullptr)
	, m_listIteratorType(nullptr)
	, m_stopIterationType(nullptr)
	, m_stringType(nullptr)
	, m_typeErrorType(nullptr)
	, m_finallyBlockDepth(0)
{
	auto listString = m_allocator->allocateStringConstant("List").value;
	m_listType = m_allocator->allocateClass(listString, sizeof(List), reinterpret_cast<MarkingFunction>(List::mark));	

	auto addFn = [this](ObjClass* type, std::string_view name, NativeFunction function, int argCount)
	{
		auto nameObj = m_allocator->allocateStringConstant(name).value;
		auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount, &m_builtins, nullptr);
		type->fields.set(nameObj, Value(functionObj));
	};
	addFn(m_listType, "$init", List::init, 1);
	addFn(m_listType, "$iter", List::iter, 1);
	addFn(m_listType, "push", List::push, 2);
	addFn(m_listType, "size", List::get_size, 1);
	addFn(m_listType, "$get_index", List::get_index, 2);
	addFn(m_listType, "$set_index", List::set_index, 3);

	auto listIteratorString = m_allocator->allocateStringConstant("_ListIterator").value;
	m_listIteratorType = m_allocator->allocateClass(listIteratorString, sizeof(ListIterator), reinterpret_cast<MarkingFunction>(ListIterator::mark));
	addFn(m_listIteratorType, "$init", ListIterator::init, 2);
	addFn(m_listIteratorType, "$next", ListIterator::next, 1);

	auto intString = m_allocator->allocateStringConstant("Int").value;
	m_intType = m_allocator->allocateClass(intString, 0, nullptr);

	auto stringString = m_allocator->allocateStringConstant("String").value;
	m_stringType = m_allocator->allocateClass(stringString, 0, nullptr);

	auto stopIterationString = m_allocator->allocateStringConstant("StopIteration").value;
	m_stopIterationType = m_allocator->allocateClass(stopIterationString, 0, nullptr);

	auto typeErrorString = m_allocator->allocateStringConstant("TypeError").value;
	m_typeErrorType = m_allocator->allocateClass(typeErrorString, 0, nullptr);

	reset();
}

VmResult Vm::execute(
	ObjFunction* program,
	ObjModule* module,
	Scanner& scanner,
	Parser& parser,
	Compiler& compiler,
	const SourceInfo& sourceInfo,
	ErrorReporter& errorReporter)
{
	auto path = std::filesystem::absolute(sourceInfo.displayedFilename);
	m_modules.set(m_allocator->allocateStringConstant(std::string_view(path.string())).value, Value(module));

	m_scanner = &scanner;
	m_parser = &parser;
	m_compiler = &compiler;
	m_sourceInfo = &sourceInfo;
	m_compiler->m_module = nullptr;

	m_callStack.clear();
	m_stack.clear();
	m_errorReporter = &errorReporter;

	if (m_callStack.push() == false)
	{
		ASSERT_NOT_REACHED();
		return VmResult::RuntimeError;
	}
	auto& frame = m_callStack.top();
	// TODO: Maybe just call callValue()?
	frame.instructionPointer = program->byteCode.code.data();
	frame.values = m_stack.topPtr;
	frame.function = program;
	frame.numberOfValuesToPopOffExceptArgs = 0;
	frame.callable = program;
	m_globals = program->globals;

	try 
	{
		const auto result = run();
		if (result.type == ResultType::Ok)
		{
			// The program should always finish without anything on both the excecution and call stack.
			ASSERT(m_stack.isEmpty());
			ASSERT(m_callStack.isEmpty());
			ASSERT(m_exceptionHandlers.isEmpty());
			ASSERT(m_finallyBlockDepth == 0);
			return VmResult::Success;
		}
	}
	catch (const FatalException&)
	{}
	return VmResult::RuntimeError;
}

void Vm::reset()
{
	m_builtins.clear();
	m_modules.clear();
	m_builtins.set(m_listType->name, Value(m_listType));
	m_builtins.set(m_intType->name, Value(m_intType));
	m_builtins.set(m_stringType->name, Value(m_stringType));
	m_builtins.set(m_stopIterationType->name, Value(m_stopIterationType));
	m_builtins.set(m_listIteratorType->name, Value(m_listIteratorType));
}

Vm::Result Vm::run()
{
	for (;;)
	{
	#ifdef VOXL_DEBUG_PRINT_VM_EXECUTION_TRACE
		debugPrintStack();
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
				TRY(callValue(*optMethod, 2, 0)); \
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
					TRY(callValue(*optMethod, 2, 0));
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
					TRY(callValue(*optMethod, 2, 0));
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
					TRY(callValue(*optMethod, 2, 0)); \
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
			const auto& lhs = m_stack.peek(1);
			const auto& rhs = m_stack.peek(0);
			std::stringstream result;
			// Could optmize this by not recalculating the length using Utf8::strlen every time.
			// Could also create an stream for ObjString* objects to avoid pointless allocating in std::stringstream.
			// Could store a reserved stringstream inside the vm. There may be an issue if a native function is called
			// which then calls a normal function which calls an overload for printing.
			result << lhs << rhs;
			ObjString* string = m_allocator->allocateString(result.str());
			m_stack.pop();
			m_stack.pop();
			TRY_PUSH(Value(string));
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
			auto name = m_stack.peek(0).asObj()->asString();
			auto& value = m_stack.peek(1);
			bool doesNotAlreadyExist = setGlobal(name, value);
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
			auto name = m_stack.peek(0).asObj()->asString();
			const auto& value = getGlobal(name);
			if (value.has_value() == false)
			{
				return fatalError("'%s' is not defined", name->chars);
			}
			m_stack.pop();
			TRY_PUSH(*value);
			break;
		}

		case Op::SetGlobal:
		{
			// Don't know if I should allow redeclaration of global in a language focused on being used as a REPL.
			auto name = m_stack.peek(0).asObj()->asString();
			auto& value = m_stack.peek(1);
			bool doesNotAlreadyExist = setGlobal(name, value);
			if (doesNotAlreadyExist)
			{
				return fatalError("'%s' is not defined", name->chars);
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

			const auto field = getField(lhs, fieldName);
			m_stack.pop();
			m_stack.pop();
			if (field.has_value())
			{
				TRY_PUSH(*field);
				break;
			}

			if (lhs.isObj())
			{
				const auto lhsObj = lhs.asObj();
				if (lhsObj->isModule() && (lhsObj->asModule()->isLoaded == false))
				{
					return fatalError("partially initialized module 'TODO' has no attribute 'TODO' (most likely due to a circular import)");
				}
			}

			TRY_PUSH(Value::null());
			// Maybe "value of type %t"
			//return fatalError("value has no field '%s'", fieldName->chars);
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
			if (auto class_ = getClass(value); class_.has_value())
			{
				auto getIndexFunction = class_->fields.get(m_getIndexString);
				if (getIndexFunction.has_value() == false)
				{
					return fatalError("type doesn't define an index function");
				}
				TRY(callValue(*getIndexFunction, 2, 0));
			}
			break;
		}

		case Op::SetIndex:
		{
			auto& value = m_stack.peek(1);
			if (auto class_ = getClass(value); class_.has_value())
			{
				auto setIndexFunction = class_->fields.get(m_setIndexString);
				if (setIndexFunction.has_value() == false)
				{
					return fatalError("type doesn't define an set index function");
				}
				TRY(callValue(*setIndexFunction, 3, 0));
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
				m_stack.pop();
				return Result::ok();
			}
			else
			{
				const auto& frame = m_callStack.top();
				const auto& result = frame.isInitializer ? *frame.values : m_stack.peek(0);
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

				while ((m_exceptionHandlers.isEmpty() == false) && (m_exceptionHandlers.top().callFrame == &frame))
				{
					m_exceptionHandlers.pop();
				}

				m_callStack.pop();

				auto callable = m_callStack.top().callable;
				switch (callable->type)
				{
				case ObjType::Function:
					m_globals = callable->asFunction()->globals;
					break;

				case ObjType::NativeFunction:
					m_globals = callable->asNativeFunction()->globals;
					break;

				default:
					ASSERT_NOT_REACHED();
				}

				TRY_PUSH(result);

				if (m_callStack.top().isNativeFunction())
				{
					return Result::ok();
				}
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
			TRY_PUSH(Value(class_));
			break;
		}

		case Op::TryBegin:
		{
			const auto jump = readUint32();
			TRY_PUSH_EXCEPTION_HANDLER();
			auto& handler = m_exceptionHandlers.top();
			auto& frame = m_callStack.top();
			handler.callFrame = &frame;
			handler.handlerCodeLocation = frame.instructionPointer + jump;
			handler.stackTopPtrBeforeTry = m_stack.topPtr;
			break;
		}

		case Op::TryEnd:
			m_exceptionHandlers.pop();
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
			// Setting the upvalue count to 0 so if the GC runs while upvalues are allocated it 
			// doesn't try to add unititalized pointers.
			// Could set some flag for half initialized closures for example setting each pointer in closure->upvalues to nullptr.
			// This would require a check every time a closure is marked or just to assume that pointers can be 
			// nullptr (read Allocator::addObj).
			closure->upvalueCount = 0;
			m_stack.pop();
			TRY_PUSH(Value(closure));
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
			closure->upvalueCount = function->upvalueCount;
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

		case Op::MatchClass:
		{
			const auto& class_ = m_stack.peek(0).as.obj->asClass();
			const auto& value = m_stack.peek(1);
			const auto valueClass = getClass(value);
			m_stack.top() = Value(valueClass.has_value() && (&*valueClass == class_));
			break;
		}

		case Op::Import:
		{
			auto filename = m_stack.peek(0).asObj()->asString();
			m_stack.pop();
			// At the end the stack has to contain the module and the return value of the main function which is always null.
			// If a module is already loaded the value has to be pushed anyway.
			if (const auto module = m_modules.get(filename); module.has_value())
			{
				TRY_PUSH(*module);
				TRY_PUSH(Value::null());
				break;
			}
			if (const auto module = m_nativeModulesMains.find(std::string_view(filename->chars, filename->size));
				module != m_nativeModulesMains.end())
			{
				const auto moduleObj = m_allocator->allocateModule();
				m_modules.set(filename, Value(moduleObj));
				const auto main = m_allocator->allocateForeignFunction(filename, module->second, 0, &moduleObj->globals, nullptr);
				TRY_PUSH(Value(moduleObj));
				TRY(callValue(Value(main), 0, 0));
				break;
			}
			auto path = std::filesystem::absolute(
				m_sourceInfo->directory / std::filesystem::path(std::string_view(filename->chars, filename->size)));
			if (path.has_extension() == false)
				path.replace_extension("voxl");
			auto pathString = m_allocator->allocateStringConstant(path.string()).value;
			if (auto module = m_modules.get(pathString); module.has_value())
			{
				TRY_PUSH(*module);
				TRY_PUSH(Value::null());
				break;
			}
			SourceInfo sourceInfo;
			// Should this be filename or pathString?
			sourceInfo.displayedFilename = filename->chars;
			sourceInfo.directory = std::move(path.remove_filename());
			auto source = stringFromFile(pathString->chars);
			sourceInfo.source = source;
			auto scannerResult = m_scanner->parse(sourceInfo, *m_errorReporter);
			auto parserResult = m_parser->parse(scannerResult.tokens, sourceInfo, *m_errorReporter);
			if (scannerResult.hadError || parserResult.hadError)
			{
				return fatalError("import error");
			}
			auto compilerResult = m_compiler->compile(parserResult.ast, *m_sourceInfo, *m_errorReporter);
			if (compilerResult.hadError)
			{
				return fatalError("import error");
			}
			TRY_PUSH(Value(compilerResult.module));
			m_modules.set(pathString, Value(compilerResult.module));
			TRY(callObjFunction(compilerResult.program, 0, 0, false));
			break;
		}

		case Op::ModuleSetLoaded:
		{
			auto module = m_stack.peek(0).asObj()->asModule();
			module->isLoaded = true;
			break;
		}

		case Op::ModuleImportAllToGlobalNamespace:
		{
			auto module = m_stack.peek(0).asObj()->asModule();
			if (module->isLoaded == false)
			{
				return fatalError("import error");
			}
			for (auto& [key, value] : module->globals)
			{
				// TODO: maybe check if this overrides.
				// Would require the function to return if the key already exits. Right now it sets it and returns false.
				setGlobal(key, value);
			}
			m_stack.pop();
			break;
		}

		case Op::CloneTop:
		{
			TRY_PUSH(m_stack.peek(0));
			break;
		}

		case Op::FinallyBegin:
			m_finallyBlockDepth++;
			break;

		case Op::FinallyEnd:
			ASSERT(m_finallyBlockDepth != 0);
			m_finallyBlockDepth--;
			break;


		case Op::Inherit:
		{
			const auto& class_ = m_stack.peek(1).asObj()->asClass();
			auto& superclassValue = m_stack.peek(0);
			if ((superclassValue.isObj() == false) || (superclassValue.asObj()->isClass() == false))
			{
				return fatalError("expected class");
			}
			auto superclass = superclassValue.asObj()->asClass();
			class_->superclass = *superclass;
			m_stack.pop();
			break;
		}

		case Op::CreateList:
		{
			auto list = m_allocator->allocateNativeInstance(m_listType);
			static_cast<List*>(list)->init();
			TRY_PUSH(Value(list));
			break;
		}

		case Op::ListPush:
		{
			auto& listValue = m_stack.peek(1);
			const auto& newElement = m_stack.peek(0);
			ASSERT(listValue.isObj());
			auto listObj = listValue.asObj();
			ASSERT(listObj->isNativeInstance());
			auto listInstance = listObj->asNativeInstance();
			// TODO: Make this check a function.
			ASSERT(listInstance->class_->mark == reinterpret_cast<MarkingFunction>(List::mark));
			const auto list = static_cast<List*>(listInstance);
			m_stack.pop();
			list->push(newElement);
			break;
		}

		default:
			ASSERT_NOT_REACHED();
			return Result::fatal();
		}
	}
}

void Vm::defineNativeFunction(std::string_view name, NativeFunction function, int argCount)
{
	auto nameObj = m_allocator->allocateStringConstant(name).value;
	auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount, &m_builtins, nullptr);
	m_builtins.set(nameObj, Value(functionObj));
}

void Vm::createModule(std::string_view name, NativeFunction moduleMain)
{
	m_nativeModulesMains[name] = moduleMain;
}

Value Vm::call(const Value& calle, Value* values, int argCount)
{
	int numberOfValuesToPopOffExceptArgs = 0;
	if (calle.isObj() && calle.as.obj->isClass())
	{
		numberOfValuesToPopOffExceptArgs = 1;
		if (m_stack.push(calle) == false)
		{
			throw FatalException{};
		}
	}

	for (int i = 0; i < argCount; i++)
	{
		if (m_stack.push(values[i]) == false)
		{
			throw FatalException{};
		}
	}

	const auto callResult = callValue(calle, argCount, numberOfValuesToPopOffExceptArgs);
	switch (callResult.type)
	{
	case ResultType::Ok: break;
	case ResultType::Fatal: throw FatalException{};
	case ResultType::Exception: throw NativeException(callResult.value);
	}

	// TODO: Would also need to check for native function in constructors.
	// A simpler solution might be returning if the function is a native function from callValue
	// Becuase so many places call callValue, I could create a new function callValueAndReturnIfNative.
	// Then just call this function from callValue and ignore the reutrn value.
	if (calle.isObj())
	{
		auto shouldCallRun = true;

		if (calle.as.obj->isClass())
		{
			auto optInitializer = calle.as.obj->asClass()->fields.get(m_initString);
			if ((optInitializer.has_value() == false) 
				|| (optInitializer->isObj() && optInitializer->as.obj->isNativeFunction()))
			{
				shouldCallRun = false;
			}
		}
		else if (calle.as.obj->isNativeFunction())
		{
			shouldCallRun = false;
		}

		if (shouldCallRun == false)
		{
			const auto& returnValue = m_stack.peek(0);
			m_stack.pop();
			return returnValue;
		}
	}

	const auto runResult = run();
	switch (runResult.type)
	{
		case ResultType::Ok:
		{
			const auto& returnValue = m_stack.peek(0);
			m_stack.pop();
			return returnValue;
		}
		case ResultType::Fatal: throw FatalException{};
		case ResultType::Exception: throw NativeException(runResult.value);
	}

	ASSERT_NOT_REACHED();
	return Value::null();
}

uint8_t Vm::readUint8()
{
	uint8_t value = *m_callStack.top().instructionPointer;
	m_callStack.top().instructionPointer++;
	return value;
}

// TODO: could make a constructor for NativeFunctionResult that takes the vm and a message which it logs.
Vm::Result Vm::fatalError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const auto message = formatToTempBuffer(format, args);
	va_end(args);
	m_errorReporter->onVmError(*this, message);
	return Result::fatal();
}

Vm::Result Vm::callObjFunction(ObjFunction* function, int argCount, int numberOfValuesToPopOffExceptArgs, bool isInitializer)
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
	frame.callable = function;
	m_globals = function->globals;
	frame.numberOfValuesToPopOffExceptArgs = numberOfValuesToPopOffExceptArgs;
	frame.isInitializer = isInitializer;
	return Result::ok();
}

Vm::Result Vm::callValue(Value value, int argCount, int numberOfValuesToPopOffExceptArgs, bool isInitializer)
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
			TRY(callObjFunction(obj->asFunction(), argCount, numberOfValuesToPopOffExceptArgs, isInitializer));
			break;
		}

		case ObjType::Closure:
		{
			auto closure = obj->asClosure();
			TRY(callObjFunction(closure->function, argCount, numberOfValuesToPopOffExceptArgs, isInitializer));
			m_callStack.top().upvalues = closure->upvalues;
			break;
		}

		case ObjType::NativeFunction:
		{
			auto function = obj->asNativeFunction();
			if (argCount != function->argCount)
			{
				return fatalError("expected %d arguments but got %d", function->argCount, argCount);
			} 
			TRY_PUSH_CALL_STACK();
			auto& frame = m_callStack.top();
			frame.setNativeFunction();
			frame.callable = function;
			frame.isInitializer = isInitializer;
			m_globals = function->globals;
			try
			{
				const auto args = m_stack.topPtr - argCount;
				Context context(args, argCount, *m_allocator, *this);
				const auto result = function->function(context);
				m_stack.topPtr -= numberOfValuesToPopOffExceptArgs + static_cast<size_t>(argCount);
				TRY_PUSH(isInitializer ? args[0] : result.value);
				m_callStack.pop();
				auto callable = m_callStack.top().callable;
				switch (callable->type)
				{
				case ObjType::Function:
					m_globals = callable->asFunction()->globals;
					break;

				case ObjType::NativeFunction:
					m_globals = callable->asNativeFunction()->globals;
					break;

				default:
					ASSERT_NOT_REACHED();
				}
			}
			catch (const NativeException& exception)
			{
				m_callStack.pop();

				auto callable = m_callStack.top().callable;
				switch (callable->type)
				{
				case ObjType::Function:
					m_globals = callable->asFunction()->globals;
					break;

				case ObjType::NativeFunction:
					m_globals = callable->asNativeFunction()->globals;
					break;

				default:
					ASSERT_NOT_REACHED();
				}

				TRY(throwValue(exception.value));
			}

			break;
		}

		case ObjType::Class:
		{
			// The class gets replaced with the instance so any other case would not work.
			ASSERT(numberOfValuesToPopOffExceptArgs == 1);

			auto class_ = obj->asClass();
			auto instance = (class_->instanceSize == 0)
				? static_cast<Obj*>(m_allocator->allocateInstance(class_))
				: static_cast<Obj*>(m_allocator->allocateNativeInstance(class_));

			// TODO: Handle special classes like Int.
			m_stack.topPtr[-argCount - 1] = Value(instance); // Replace the class with the instance.
			if (const auto initializer = class_->fields.get(m_initString); initializer.has_value())
			{
				TRY(callValue(*initializer, argCount + 1, 0, true));
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
			TRY(callValue(Value(boundFunction->callable), argCount + 1, 0))
			break;
		}

		default:
		{
			return fatalError("type is not calable");
		}
	}

	return Result::ok();
}

std::optional<Value> Vm::getField(Value& value, ObjString* fieldName)
{
	// TODO: When using type errors just use
	/*printf("expected %s got %s", a->name, b->name)*/
	// Dereferencing the results from HashTable::get() because it return a std::optional<Value&> not std::optional<Value>.
	// Can't use a reference type because bound functions can be allocated.

	if (value.isObj())
	{
		const auto obj = value.asObj();
		if (obj->isInstance())
		{
			if (const auto field = obj->asInstance()->fields.get(fieldName); field.has_value())
				return *field;
		}
		else if (obj->isClass())
		{
			std::optional<ObjClass&> class_(*obj->asClass());
			while (class_.has_value())
			{
				auto method = class_->fields.get(fieldName);
				if (method.has_value())
					return *method;

				class_ = class_->superclass;
			}
			return std::nullopt;
		}
		else if (obj->isModule())
		{
			const auto module = obj->asModule();
			// TODO: Maybe add check so names that begin with an underscore are private.
			return *module->globals.get(fieldName);
		}
	}
	
	auto method = getMethod(value, fieldName);
	if (method->isObj() == false)
		return std::nullopt;

	auto methodObj = method->asObj();
	if (methodObj->canBeBound() == false)
		return std::nullopt;

	return Value(m_allocator->allocateBoundFunction(methodObj, value));
}

std::optional<Value> Vm::getMethod(Value& value, ObjString* methodName)
{
	// TODO: Maybe have a seperate hash table for method and fields of a class. Method could only be added using Impl.
	// Could return by reference instead of value.
	auto class_ = getClass(value);
	while (class_.has_value())
	{
		auto method = class_->fields.get(methodName);
		if (method.has_value())
			return *method;

		class_ = class_->superclass;
	}
	return std::nullopt;
}

Vm::Result Vm::throwValue(const Value& value)
{
	if (m_exceptionHandlers.isEmpty())
	{
		return fatalError("uncaught exception");
	}

	if (m_finallyBlockDepth > 0)
	{
		return fatalError("cannot throw exception from finally");
	}

	const auto& handler = m_exceptionHandlers.top();
	auto& catchFrame = *handler.callFrame;

	for (auto frame = m_callStack.crbegin(); (frame != m_callStack.crend()) && (&*frame > &catchFrame); ++frame)
	{
		if (frame->isNativeFunction())
		{
			return Result::exception(value);
		}
		m_callStack.pop();
	}

	m_stack.topPtr = handler.stackTopPtrBeforeTry;
	catchFrame.instructionPointer = handler.handlerCodeLocation;
	TRY_PUSH(value);

	m_exceptionHandlers.pop();

	return Result::ok();
}

std::optional<ObjClass&> Vm::getClass(const Value& value)
{
	if (value.isInt())
		return *m_intType;

	if (value.isObj())
	{
		if (value.as.obj->isString())
			return *m_stringType;
		if (value.as.obj->isInstance())
			return *value.as.obj->asInstance()->class_;
		if (value.as.obj->isNativeInstance())
			return *value.as.obj->asNativeInstance()->class_;
	}
	return std::nullopt;
}

Value Vm::typeErrorExpected(ObjClass*)
{
	return Value(m_allocator->allocateInstance(m_typeErrorType));
}

std::optional<Value&> Vm::getGlobal(ObjString* name)
{
	auto optValue = m_globals->get(name);
	if (optValue.has_value())
		return *optValue;

	return m_builtins.get(name);
}

bool Vm::setGlobal(ObjString* name, const Value& value)
{
	return m_globals->set(name, value);
}

void Vm::debugPrintStack()
{
	std::cout << "[ ";
	for (const auto& value : m_stack)
	{
		debugPrintValue(value);
		std::cout << ' ';
	}
	std::cout << "]\n";
}

void Vm::mark(Vm* vm, Allocator& allocator)
{
	for (auto& value : vm->m_stack)
	{
		allocator.addValue(value);
	}

	// Maybe be faster not to add the strings becuase they are constants.
	allocator.addHashTable(vm->m_modules);

	allocator.addHashTable(vm->m_builtins);
	if (vm->m_listType != nullptr)
		allocator.addObj(vm->m_listType);
	if (vm->m_intType != nullptr)
		allocator.addObj(vm->m_intType);
	if (vm->m_listIteratorType != nullptr)
		allocator.addObj(vm->m_listIteratorType);
	if (vm->m_stopIterationType != nullptr)
	allocator.addObj(vm->m_stopIterationType);
	if (vm->m_stringType != nullptr)
		allocator.addObj(vm->m_stringType);
	if (vm->m_typeErrorType)
		allocator.addObj(vm->m_typeErrorType);

	for (const auto& frame : vm->m_callStack)
	{
		if (frame.function != nullptr)
			allocator.addObj(frame.function);
	}

	for (auto upvalue : vm->m_openUpvalues)
	{
		allocator.addObj(upvalue);
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

bool Vm::CallFrame::isNativeFunction() const
{
	return function == nullptr;
}

void Vm::CallFrame::setNativeFunction()
{
	function = nullptr;
}

Vm::Result Vm::Result::ok()
{
	return Result(ResultType::Ok);
}

Vm::Result Vm::Result::exception(const Value& value)
{
	Result result(ResultType::Exception);
	result.value = value;
	return result;
}

Vm::Result Vm::Result::fatal()
{
	return Result(ResultType::Fatal);
}

Vm::Result::Result(ResultType type)
	: type(type)
{}