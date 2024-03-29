#include <Vm/Vm.hpp>
#include <Vm/List.hpp>
#include <Vm/Dict.hpp>
#include <Vm/String.hpp>
#include <Vm/Number.hpp>
#include <Vm/Errors.hpp>
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

// TODO: Maybe instead all those functions WITH/WITHOUT VALUE have a converions function between the types.
// or use a function that returns the stored value with the same name for both types.

#define TRY_OUTSIDE_RUN(somethingThatReturnsResult) \
	 do \
	 { \
		const auto expressionResult = somethingThatReturnsResult; \
		if (expressionResult.type != ResultType::Ok) \
		{ \
			return expressionResult; \
		} \
	} while (false)

#define TRY_INSIDE_RUN(somethingThatReturnsResult) \
	 do \
	 { \
		const auto expressionResult = somethingThatReturnsResult; \
		if (expressionResult.type == ResultType::ExceptionHandled) \
		{ \
			goto switchBreak; \
		} \
		else if (expressionResult.type != ResultType::Ok) \
		{ \
			return expressionResult; \
		} \
	} while (false)

#define TRY_WITH_VALUE_OUTSIDE_RUN(resultWithValue) \
	 do \
	 { \
		const auto expressionResult = resultWithValue; \
		if (expressionResult.type != ResultType::Ok) \
		{ \
			Result newResult(expressionResult.type); \
			newResult.exceptionValue = expressionResult.value; \
			return newResult; \
		} \
	} while (false)

#define TRY_WITH_VALUE_INSIDE_RUN(resultWithValue) \
	 do \
	 { \
		const auto expressionResult = resultWithValue; \
		if (expressionResult.type == ResultType::ExceptionHandled) \
		{ \
			goto switchBreak; \
		} \
		else if (expressionResult.type != ResultType::Ok) \
		{ \
			Result newResult(expressionResult.type); \
			newResult.exceptionValue = expressionResult.value; \
			return newResult; \
		} \
	} while (false)

#define TRY_WITHOUT_VALUE(result) \
	 do \
	 { \
		const auto expressionResult = result; \
		if (expressionResult.type != ResultType::Ok) \
		{ \
			ResultWithValue resultWithValue(expressionResult.type); \
			resultWithValue.value = expressionResult.exceptionValue; \
			return resultWithValue; \
		} \
	} while (false)

#define RETURN_WITHOUT_VALUE(result) \
	do \
	 { \
		const auto expressionResult = result; \
		ResultWithValue resultWithValue(expressionResult.type); \
		resultWithValue.value = expressionResult.exceptionValue; \
		return resultWithValue; \
	} while (false)

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
	, m_initString(allocator.allocateStringConstant("$init").value)
	, m_addString(allocator.allocateStringConstant("$add").value)
	, m_subString(allocator.allocateStringConstant("$sub").value)
	, m_mulString(allocator.allocateStringConstant("$mul").value)
	, m_divString(allocator.allocateStringConstant("$div").value)
	, m_modString(allocator.allocateStringConstant("$mod").value)
	, m_ltString(allocator.allocateStringConstant("$lt").value)
	, m_leString(allocator.allocateStringConstant("$le").value)
	, m_gtString(allocator.allocateStringConstant("$gt").value)
	, m_geString(allocator.allocateStringConstant("$ge").value)
	, m_getIndexString(allocator.allocateStringConstant("$get_index").value)
	, m_setIndexString(allocator.allocateStringConstant("$set_index").value)
	, m_eqString(allocator.allocateStringConstant("$eq").value)
	, m_strString(allocator.allocateStringConstant("$str").value)
	, m_msgString(allocator.allocateStringConstant("msg").value)
	, m_emptyString(allocator.allocateStringConstant("").value)
// The GC might run during the constructor so the values have to be checked inside mark for begin null.
// This also may be fixable by making constant classes that are always marked.
// Currently only strings and functions can be constant because they don't contain any changing data.

// When adding a new type remember to set it to nullptr here and mark it in Vm::mark().
	, m_listType(nullptr)
	, m_listIteratorType(nullptr)
	, m_dictType(nullptr)
	, m_numberType(nullptr)
	, m_intType(nullptr)
	, m_floatType(nullptr)
	, m_boolType(nullptr)
	, m_typeType(nullptr)
	, m_nullType(nullptr)
	, m_stopIterationType(nullptr)
	, m_stringType(nullptr)
	, m_typeErrorType(nullptr)
	, m_nameErrorType(nullptr)
	, m_zeroDivisionErrorType(nullptr)
	, m_finallyBlockDepth(0)
{
	// Cannot use allocateNativeClass overload with initializer list inside constructor because the GC might run. 

	auto addFn = [this](ObjClass* type, std::string_view name, NativeFunction function, int argCount)
	{
		auto nameObj = m_allocator->allocateStringConstant(name).value;
		auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount, &m_builtins, nullptr);
		type->fields.set(nameObj, Value(functionObj));
	};

	auto listString = m_allocator->allocateStringConstant("List").value;
	m_listType = m_allocator->allocateNativeClass(listString, List::init, List::free);
	addFn(m_listType, "$iter", List::iter, List::iterArgCount);
	addFn(m_listType, "push", List::push, List::pushArgCount);
	addFn(m_listType, "size", List::get_size, List::getSizeArgCount);
	addFn(m_listType, "$get_index", List::get_index, List::getIndexArgCount);
	addFn(m_listType, "$set_index", List::set_index, List::setIndexArgCount);

	auto listIteratorString = m_allocator->allocateStringConstant("_ListIterator").value;
	m_listIteratorType = m_allocator->allocateNativeClass<ListIterator>(listIteratorString, ListIterator::construct, nullptr);
	addFn(m_listIteratorType, "$init", ListIterator::init, ListIterator::initArgCount);
	addFn(m_listIteratorType, "$next", ListIterator::next, ListIterator::nextArgCount);

	auto dictString = m_allocator->allocateStringConstant("Dict").value;
	m_dictType = m_allocator->allocateNativeClass(dictString, Dict::init, Dict::free);
	addFn(m_dictType, "$get_index", Dict::get_index, Dict::getIndexArgCount);
	addFn(m_dictType, "$set_index", Dict::set_index, Dict::setIndexArgCount);
	addFn(m_dictType, "size", Dict::get_size, Dict::getSizeArgCount);

	auto numberString = m_allocator->allocateStringConstant("Number").value;
	m_numberType = m_allocator->allocateClass(numberString);
	addFn(m_numberType, "floor", Number::floor, Number::floorArgCount);
	addFn(m_numberType, "ceil", Number::ceil, Number::ceilArgCount);
	addFn(m_numberType, "round", Number::round, Number::roundArgcount);
	addFn(m_numberType, "pow", Number::pow, Number::powArgCount);
	addFn(m_numberType, "sqrt", Number::sqrt, Number::sqrtArgCount);
	addFn(m_numberType, "is_nan", Number::is_nan, Number::isNanArgCount);
	addFn(m_numberType, "is_inf", Number::is_inf, Number::isInfArgCount);
	addFn(m_numberType, "sin", Number::sin, Number::sinArgCount);
	addFn(m_numberType, "cos", Number::cos, Number::cosArgCount);
	addFn(m_numberType, "tan", Number::tan, Number::tanArgCount);

	auto intString = m_allocator->allocateStringConstant("Int").value;
	m_intType = m_allocator->allocateClass(intString);
	m_intType->superclass = *m_numberType;

	auto floatString = m_allocator->allocateStringConstant("Float").value;
	m_floatType = m_allocator->allocateClass(floatString);
	m_floatType->superclass = *m_numberType;

	auto boolString = m_allocator->allocateStringConstant("Bool").value;
	m_boolType = m_allocator->allocateClass(boolString);

	auto typeString = m_allocator->allocateStringConstant("Type").value;
	m_typeType = m_allocator->allocateClass(typeString);

	auto nullString = m_allocator->allocateStringConstant("String").value;
	m_nullType = m_allocator->allocateClass(nullString);

	auto stringString = m_allocator->allocateStringConstant("String").value;
	m_stringType = m_allocator->allocateClass(stringString);
	addFn(m_stringType, "len", String::len, String::lenArgCount);
	addFn(m_stringType, "$hash", String::hash, String::hashArgCount);

	auto stopIterationString = m_allocator->allocateStringConstant("StopIteration").value;
	m_stopIterationType = m_allocator->allocateClass(stopIterationString);

	auto typeErrorString = m_allocator->allocateStringConstant("TypeError").value;
	m_typeErrorType = m_allocator->allocateClass(typeErrorString);
	addFn(m_typeErrorType, "$init", GenericStringError::init, GenericStringError::initArgCount);
	addFn(m_typeErrorType, "$str", GenericStringError::str, GenericStringError::strArgCount);

	auto nameErrorString = m_allocator->allocateStringConstant("NameError").value;
	m_nameErrorType = m_allocator->allocateClass(nameErrorString);
	addFn(m_nameErrorType, "$init", GenericStringError::init, GenericStringError::initArgCount);
	addFn(m_nameErrorType, "$str", GenericStringError::str, GenericStringError::strArgCount);

	auto zeroDivisonErrorString = m_allocator->allocateStringConstant("ZeroDivisionError").value;
	m_zeroDivisionErrorType = m_allocator->allocateClass(zeroDivisonErrorString);
	addFn(m_zeroDivisionErrorType, "$init", GenericStringError::init, GenericStringError::initArgCount);
	addFn(m_zeroDivisionErrorType, "$str", GenericStringError::str, GenericStringError::strArgCount);

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

	if (callObjFunction(program, 0, 0, false).type != ResultType::Ok)
	{
		ASSERT_NOT_REACHED();
		return VmResult::RuntimeError;
	}

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
	m_builtins.set(m_dictType->name, Value(m_dictType));
	m_builtins.set(m_numberType->name, Value(m_numberType));
	m_builtins.set(m_intType->name, Value(m_intType));
	m_builtins.set(m_floatType->name, Value(m_floatType));
	m_builtins.set(m_stringType->name, Value(m_stringType));
	m_builtins.set(m_boolType->name, Value(m_boolType));
	m_builtins.set(m_stopIterationType->name, Value(m_stopIterationType));
	m_builtins.set(m_listIteratorType->name, Value(m_listIteratorType));
	m_builtins.set(m_typeErrorType->name, Value(m_typeErrorType));
	m_builtins.set(m_nameErrorType->name, Value(m_nameErrorType));
	m_builtins.set(m_zeroDivisionErrorType->name, Value(m_zeroDivisionErrorType));
}

#define TRY TRY_INSIDE_RUN
#define TRY_WITH_VALUE TRY_WITH_VALUE_INSIDE_RUN
Vm::Result Vm::run()
{
	for (;;)
	{
	#ifdef VOXL_DEBUG_PRINT_VM_EXECUTION_TRACE
		debugPrintStack();
		if (m_callStack.top().callable->isFunction())
		{
			const auto function = m_callStack.top().callable->asFunction();
			disassembleInstruction(
				function->byteCode,
				m_instructionPointer - function->byteCode.code.data(), *m_allocator);
			std::cout << '\n';
		}
		else
		{
			std::cout << "cannot disassemble non function\n";
			ASSERT_NOT_REACHED();
		}
		
	#endif

	#ifdef VOXL_DEBUG_STRESS_TEST_GC
		m_allocator->runGc();
	#endif
		auto op = static_cast<Op>(*m_instructionPointer);
		m_instructionPointer++;

		switch (op)
		{

#define BINARY_ARITHMETIC_OP(op, overloadNameString) \
	{ \
		const auto& lhs = m_stack.peek(1); \
		const auto& rhs = m_stack.peek(0); \
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
				goto unsupportedTypes##overloadNameString; \
			} \
		} \
		else \
		{ \
			m_stack.pop(); \
			if (lhs.isInt() && rhs.isInt()) \
			{ \
				m_stack.top() = Value(lhs.as.intNumber op rhs.as.intNumber); \
			} \
			else if (lhs.isFloat() && rhs.isFloat()) \
			{ \
				m_stack.top() = Value(lhs.as.floatNumber op rhs.as.floatNumber); \
			} \
			else if (lhs.isFloat() && rhs.isInt()) \
			{ \
				m_stack.top() = Value(lhs.as.floatNumber op static_cast<Float>(rhs.as.intNumber)); \
			} \
			else if (lhs.isInt() && rhs.isFloat()) \
			{ \
				m_stack.top() = Value(static_cast<Float>(lhs.as.intNumber) op rhs.as.floatNumber); \
			} \
			else \
			{ \
				goto unsupportedTypes##overloadNameString; \
			} \
		} \
		break; \
		unsupportedTypes##overloadNameString: \
		TRY(throwTypeErrorUnsupportedOperandTypesFor(#op, lhs, rhs)); \
		break; \
	}
		// Making function that work both from the vm and from the ffi is hard because for simple types the values don't have to be on the stack,
		// but for overload calls they need to be. It also requires calling callFromVmAndReturn. The simples way to implement this would be
		// to just make everything a function even for basic types. 
		case Op::Add: BINARY_ARITHMETIC_OP(+, m_addString)
		case Op::Subtract: BINARY_ARITHMETIC_OP(-, m_subString)
		case Op::Multiply: BINARY_ARITHMETIC_OP(*, m_mulString)
#undef BINARY_ARITHMETIC_OP
		case Op::Divide: 
		{
			const auto& lhs = m_stack.peek(1);
			const auto& rhs = m_stack.peek(0);
			if (lhs.isObj() && lhs.as.obj->isInstance())
			{
				auto instance = lhs.as.obj->asInstance();
				auto optMethod = instance->class_->fields.get(m_divString);
				if (optMethod.has_value())
					TRY(callValue(*optMethod, 2, 0));
				else
					goto noOverloadForDivision;

				break;
			}
			else
			{
				Float a, b;

				if (lhs.isInt())
					a = static_cast<Float>(lhs.asInt());
				else if (lhs.isFloat())
					a = lhs.asFloat();
				else
					goto noOverloadForDivision;

				if (rhs.isInt())
					b = static_cast<Float>(rhs.asInt());
				else if (rhs.isFloat())
					b = rhs.asFloat();
				else
					goto noOverloadForDivision;

				if (b == static_cast<Float>(0))
					TRY(throwErrorWithMsg(m_zeroDivisionErrorType, "division by zero"));

				m_stack.pop();
				m_stack.top() = Value::floatNum(a / b);

				break;
			}
			break;
		noOverloadForDivision:
			TRY(throwTypeErrorUnsupportedOperandTypesFor("/", lhs, rhs));
			break;
		}
		case Op::Modulo: 
		{
			const auto& lhs = m_stack.peek(1);
			const auto& rhs = m_stack.peek(0);
			if (lhs.isObj() && lhs.as.obj->isInstance())
			{
				auto instance = lhs.as.obj->asInstance();
				auto optMethod = instance->class_->fields.get(m_modString);
				if (optMethod.has_value())
					TRY(callValue(*optMethod, 2, 0));
				else
					goto noOverloadForModulo;

				break;
			}
			else if (lhs.isInt() && rhs.isInt())
			{
				m_stack.pop();
				m_stack.top() = Value::intNum(lhs.asInt() % rhs.asInt());
				break;
			}
			else
			{
				Float a, b;

				if (lhs.isInt())
					a = static_cast<Float>(lhs.asInt());
				else if (lhs.isFloat())
					a = lhs.asFloat();
				else
					goto noOverloadForModulo;

				if (rhs.isInt())
					b = static_cast<Float>(rhs.asInt());
				else if (rhs.isFloat())
					b = rhs.asFloat();
				else
					goto noOverloadForModulo;

				if (b == static_cast<Float>(0))
					TRY(throwErrorWithMsg(m_zeroDivisionErrorType, "division by zero"));

				m_stack.pop();
				m_stack.top() = Value::floatNum(fmod(a, b));

				break;
			}
			break;
		noOverloadForModulo:
			TRY(throwTypeErrorUnsupportedOperandTypesFor("%", lhs, rhs));
			break;
		}

#define BINARY_COMPARASION_OP(op, overloadNameString) \
	{ \
		const auto& lhs = m_stack.peek(1); \
		const auto& rhs = m_stack.peek(0); \
		if (lhs.isObj()) \
		{ \
			if (rhs.isObj() && lhs.as.obj->isString() && rhs.as.obj->isString()) \
			{ \
				auto left = lhs.as.obj->asString(); \
				auto right = rhs.as.obj->asString(); \
				m_stack.pop(); \
				m_stack.top() = Value(Utf8::strcmp(left->chars, left->size, right->chars, right->size) op 0); \
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
			if (lhs.isInt() && rhs.isInt()) \
			{ \
				m_stack.top() = Value(lhs.as.intNumber op rhs.as.intNumber); \
			} \
			else if (lhs.isFloat() && rhs.isFloat()) \
			{ \
				m_stack.top() = Value(lhs.as.floatNumber op rhs.as.floatNumber); \
			} \
			else if (lhs.isFloat() && rhs.isInt()) \
			{ \
				m_stack.top() = Value(lhs.as.floatNumber op static_cast<Float>(rhs.as.intNumber)); \
			} \
			else if (lhs.isInt() && rhs.isFloat()) \
			{ \
				m_stack.top() = Value(static_cast<Float>(lhs.as.intNumber) op rhs.as.floatNumber); \
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
			TRY(equals());
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
			bool doesNotAlreadyExist = m_globals->set(name, value);
			if (doesNotAlreadyExist == false)
			{
				TRY(throwErrorWithMsg(m_nameErrorType, "redeclaration of '%s'", name->chars));
			}
			m_stack.pop();
			m_stack.pop();
			break;
		}

		case Op::GetGlobal:
		{
			auto name = m_stack.peek(0).asObj()->asString();
			const auto result = getGlobal(name);
			TRY_WITH_VALUE(result);
			m_stack.pop();
			TRY_PUSH(result.value);
			break;
		}

		case Op::SetGlobal:
		{
			auto name = m_stack.peek(0).asObj()->asString();
			auto& value = m_stack.peek(1);
			TRY(setGlobal(name, value));
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

			TRY(getField(lhs, fieldName));
			const auto value = m_stack.top();
			m_stack.pop();
			m_stack.pop();
			m_stack.pop();
			TRY_PUSH(value);
			break;
		}

		case Op::SetField:
		{
			auto rhs = m_stack.peek(0);
			auto fieldName = m_stack.peek(1).as.obj->asString();
			auto lhs = m_stack.peek(2);
			TRY(setField(lhs, fieldName, rhs));
			m_stack.pop();
			m_stack.pop();
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
			auto& value = m_stack.peek(2);
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
			m_instructionPointer += jump;
			break;
		}

		case Op::JumpIfTrue:
		{
			const auto jump = readUint32();
			const auto& value = m_stack.peek(0);
			if ((value.isBool() == false))
			{
				TRY(throwTypeErrorExpectedFound(m_boolType, value));
			}
			if (value.asBool())
			{
				m_instructionPointer += jump;
			}
			break;
		}

		case Op::JumpIfFalse:
		{
			const auto jump = readUint32();
			const auto& value = m_stack.peek(0);
			if (value.isBool() == false)
			{
				TRY(throwTypeErrorExpectedFound(m_boolType, value));
			}
			if (value.asBool() == false)
			{
				m_instructionPointer += jump;
			}
			break;
		}

		case Op::JumpIfFalseAndPop:
		{
			const auto jump = readUint32();
			const auto& value = m_stack.peek(0);
			if (value.isBool() == false)
			{
				TRY(throwTypeErrorExpectedFound(m_boolType, value));
			}
			if (value.asBool() == false)
			{
				m_instructionPointer += jump;
			}
			m_stack.pop();
			break;
		}

		case Op::JumpBack:
		{
			const auto jump = readUint32();
			m_instructionPointer -= jump;
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

				popCallStack();

				TRY_PUSH(result);

				auto callable = m_callStack.top().callable;
				if ((callable == nullptr) || callable->isNativeFunction())
					return Result::ok();
			}
			break;
		}

		case Op::CreateClass:
		{
			auto nameValue = m_stack.peek(0);

			ASSERT((nameValue.type == ValueType::Obj) && (nameValue.as.obj->isString()));
			auto name = nameValue.as.obj->asString();
			auto class_ = m_allocator->allocateClass(name);
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
			handler.handlerCodeLocation = m_instructionPointer + jump;
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
			auto valueClass = getClass(value);
			bool matched = false;
			while (valueClass.has_value())
			{
				if (&*valueClass == class_)
				{
					matched = true;
					break;
				}
				valueClass = valueClass->superclass;
			}
			m_stack.top() = Value(matched);
			break;
		}

		case Op::Import:
		{
			auto filename = m_stack.peek(0).asObj()->asString();
			m_stack.pop();
			TRY(importModule(filename));
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
			TRY(importAllFromModule(module));
			m_stack.pop();
			break;
		}

		case Op::CloneTop:
		{
			TRY_PUSH(m_stack.peek(0));
			break;
		}

		case Op::CloneTopTwo:
		{
			ASSERT(m_stack.size() >= 2);
			TRY_PUSH(m_stack.peek(1));
			TRY_PUSH(m_stack.peek(1));
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
			auto class_ = m_stack.peek(1).asObj()->asClass();
			auto& superclassValue = m_stack.peek(0);
			if ((superclassValue.isObj() == false) || (superclassValue.asObj()->isClass() == false))
			{
				TRY(throwTypeErrorExpectedFound(m_typeType, superclassValue));
			}
			auto superclass = superclassValue.asObj()->asClass();
			class_->superclass = *superclass;
			if (superclass->isNative())
			{
				class_->mark = superclass->mark;
				class_->init = superclass->init;
				class_->instanceSize = superclass->instanceSize;
			}

			m_stack.pop();
			break;
		}

		case Op::CreateList:
		{
			auto list = m_allocator->allocateNativeInstance(m_listType);
			List::init(static_cast<List*>(list));
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
			ASSERT(listInstance->isOfType<List>());
			const auto list = static_cast<List*>(listInstance);
			m_stack.pop();
			list->push(newElement);
			break;
		}

		case Op::CreateDict:
		{
			auto dict = m_allocator->allocateNativeInstance(m_dictType);
			Dict::init(static_cast<Dict*>(dict));
			TRY_PUSH(Value(dict));
			break;
		}

		case Op::DictSet:
		{
			const auto dict = m_stack.peek(2);
			auto insert = m_dictType->fields.get(m_setIndexString);
			ASSERT(insert.has_value());
			TRY(callValue(*insert, 3, 0));
			m_stack.top() = dict;
			break;
		}

		default:
			ASSERT_NOT_REACHED();
			return Result::fatal();
		}

		switchBreak:;
	}
}
#undef TRY
#define TRY TRY_OUTSIDE_RUN
#undef TRY_WITH_VALUE
#define TRY_WITH_VALUE TRY_WITH_VALUE_OUTSIDE_RUN

void Vm::defineNativeFunction(std::string_view name, NativeFunction function, int argCount)
{
	auto nameObj = m_allocator->allocateStringConstant(name).value;
	auto functionObj = m_allocator->allocateForeignFunction(nameObj, function, argCount, &m_builtins, nullptr);
	m_builtins.set(nameObj, Value(functionObj));
}

void Vm::createModule(std::string_view name, NativeFunction moduleMain, void* data)
{
	m_nativeModulesMains[name] = { moduleMain, data };
}

uint8_t Vm::readUint8()
{
	uint8_t value = *m_instructionPointer;
	m_instructionPointer++;
	return value;
}

Vm::Result Vm::fatalError(const char* format, ...)
{
	// Save the instruction pointer so onVmError can read it.
	if (m_callStack.isEmpty() == false)
		m_callStack.top().instructionPointerBeforeCall = m_instructionPointer;
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
	if (m_callStack.isEmpty() == false)
		m_callStack.top().instructionPointerBeforeCall = m_instructionPointer;
	TRY_PUSH_CALL_STACK();
	auto& frame = m_callStack.top();
	m_instructionPointer = function->byteCode.code.data();
	frame.values = m_stack.topPtr - argCount;
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
			// TODO Maybe put the common parts into a function idk.
			if (m_callStack.isEmpty() == false)
				m_callStack.top().instructionPointerBeforeCall = m_instructionPointer;
			TRY_PUSH_CALL_STACK();
			auto& frame = m_callStack.top();
			frame.callable = function;
			frame.isInitializer = isInitializer;
			m_globals = function->globals;
			try
			{
				const auto args = m_stack.topPtr - argCount;
				Context context(args, argCount, *m_allocator, *this, function->context);
				const auto result = function->function(context);
				m_stack.popN(numberOfValuesToPopOffExceptArgs + static_cast<size_t>(argCount));
				TRY_PUSH(isInitializer ? args[0] : result.value);
				popCallStack();
			}
			catch (const NativeException& exception)
			{
				popCallStack();
				TRY(throwValue(exception.value));
			}

			break;
		}

		case ObjType::Class:
		{
			// The class gets replaced with the instance so any other case would not work.
			ASSERT(numberOfValuesToPopOffExceptArgs == 1);

			auto class_ = obj->asClass();

			auto returnFromSpecialConstructor = [this, argCount, numberOfValuesToPopOffExceptArgs](const Value& value)
			{
				m_stack.topPtr -= static_cast<intptr_t>(argCount) + static_cast<intptr_t>(numberOfValuesToPopOffExceptArgs);
				*m_stack.topPtr = value;
				m_stack.topPtr++;
			};

			// TODO: Add constructor for bool.
			if (class_ == m_intType)
			{
				if (argCount != 1)
					return fatalError("expected 1 args but got %d", argCount);

				const auto args = m_stack.topPtr - argCount;

				if (args[0].isInt())
					returnFromSpecialConstructor(args[0]);
				else if (args[0].isFloat())
					// TODO: Don't know what rounding to use.
					returnFromSpecialConstructor(Value::intNum(static_cast<Int>(args[0].asFloat())));
				else
					return fatalError("expected number got b");
			}
			else if ((class_ == m_floatType) || (class_ == m_numberType))
			{
				if (argCount != 1)
					return fatalError("expected 1 args but got %d", argCount);

				const auto args = m_stack.topPtr - argCount;

				if (args[0].isFloat())
					returnFromSpecialConstructor(args[0]);
				else if (args[0].isInt())
					returnFromSpecialConstructor(Value::floatNum(static_cast<Float>(args[0].asInt())));
				else
					return fatalError("expected number got b");
			}
			else if (class_ == m_stringType)
			{
				if (argCount != 0)
					return fatalError("expected 0 args but got %d", argCount);

				returnFromSpecialConstructor(Value(m_emptyString));
			}
			else
			{
				// TODO: Maybe for special types just create an instance  in the constructor and call a native function
				// This would require the functions to be able to set arguments.

				auto instance = class_->isNative()
					? static_cast<Obj*>(m_allocator->allocateNativeInstance(class_))
					: static_cast<Obj*>(m_allocator->allocateInstance(class_));

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
			}
			
			break;
		}

		case ObjType::BoundFunction:
		{
			auto boundFunction = obj->asBoundFunction();

			// Replace the function with the bound value
			ASSERT(numberOfValuesToPopOffExceptArgs == 1);
			m_stack.topPtr[-argCount - 1] = boundFunction->value;

			if (boundFunction->callable->isBoundFunction())
			{
				return fatalError("cannot bind a function twice");
			}
			TRY(callValue(Value(boundFunction->callable), argCount + 1, 0));
			break;
		}

		default:
		{
			return fatalError("type is not calable");
		}
	}

	return Result::ok();
}

std::optional<Value> Vm::atField(Value& value, ObjString* fieldName)
{
	// TODO: When using type errors just use
	/*printf("expected %s got %s", a->name, b->name)*/
	// Dereferencing the results from HashTable::get() because it return a std::optional<Value&> not std::optional<Value>.
	// Can't use a reference type because bound functions be created and returned.

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
			if (isModuleMemberPublic(fieldName))
			{
				auto field = module->globals.get(fieldName);
				if (field.has_value())
					return *field;
				return std::nullopt;
			}
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
	// This also prevents overriding methods like constructors.
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

Vm::Result Vm::setField(const Value& lhs, ObjString* fieldName, const Value& rhs)
{
	if ((lhs.isObj() == false))
		return fatalError("cannot use field access on this type");

	auto obj = lhs.as.obj;
	if (obj->isInstance())
	{
		obj->asInstance()->fields.set(fieldName, rhs);
		return Result::ok();
	}
	else if (obj->isClass())
	{
		obj->asClass()->fields.set(fieldName, rhs);
		return Result::ok();
	}

	return fatalError("cannot use field access on this type");
}

Vm::Result Vm::getField(Value& value, ObjString* fieldName)
{
	const auto field = atField(value, fieldName);
	if (field.has_value())
	{
		TRY_PUSH(*field);
		return Result::ok();
	}

	if (value.isObj())
	{
		const auto lhsObj = value.asObj();
		if (lhsObj->isModule() && (lhsObj->asModule()->isLoaded == false))
		{
			return fatalError(
				"partially initialized module has no field '%s' (most likely due to a circular import)",
				fieldName->chars);
		}
	}

	TRY_PUSH(Value::null());
	// Maybe "value of type %t"
	//return fatalError("value has no field '%s'", fieldName->chars);
	return Result::ok();
}

Vm::Result Vm::throwValue(const Value& value)
{
	if (m_exceptionHandlers.isEmpty())
	{
		auto class_ = getClass(value);
		std::optional<std::string_view> exceptionTypeName;
		std::optional<std::string_view> message;
		if (class_.has_value())
		{
			exceptionTypeName = class_->name->chars;
			if (const auto strFunction = class_->fields.get(m_strString); strFunction.has_value())
			{
				auto arguments = value;
				const auto result = callFromVmAndReturnValue(*strFunction, &arguments, 1);
				if (result.type != ResultType::Ok)
				{
					m_callStack.clear();
					return fatalError("%s.$str() failed", class_->name->chars);
				}
				const auto& returnValue = m_stack.top();
				if ((returnValue.isObj() == false) || (returnValue.asObj()->asString() == false))
				{
					m_callStack.clear();
 					return fatalError("%s.$str() has to return values of type 'String'", class_->name->chars);
				}
				message = returnValue.asObj()->asString()->chars;
			}
		}
		m_errorReporter->onUncaughtException(*this, exceptionTypeName, message);
		return Result::fatal();
	}

	if (m_finallyBlockDepth > 0)
	{
		return fatalError("cannot throw exception from finally");
	}

	const auto& handler = m_exceptionHandlers.top();
	auto& catchFrame = *handler.callFrame;

	for (auto frame = m_callStack.crbegin(); (frame != m_callStack.crend()) && (&*frame > &catchFrame); ++frame)
	{
		if (frame->callable->isNativeFunction())
		{
			return Result::exception(value);
		}
		m_callStack.pop();
	}
	
	m_stack.topPtr = handler.stackTopPtrBeforeTry;
	m_instructionPointer = handler.handlerCodeLocation;
	TRY_PUSH(value);

	m_exceptionHandlers.pop();

	return Result::exceptionHandled();
}

std::optional<ObjClass&> Vm::getClass(const Value& value)
{
	// TODO: Implement a class for every build in type. Reuse Function for different types.
	switch (value.type)
	{
	case ValueType::Int: return *m_intType;
	case ValueType::Float: return *m_floatType;
	case ValueType::Null: return *m_nullType;
	case ValueType::Bool: return *m_boolType;
	case ValueType::Obj:
	{
		const auto obj = value.asObj();
		switch (obj->type)
		{
		case ObjType::String: return *m_stringType;
		case ObjType::Class: return *m_typeType;
		case ObjType::Instance: return *value.as.obj->asInstance()->class_;
		case ObjType::NativeInstance: return *value.as.obj->asNativeInstance()->class_;
		default:
			break;
		}
		break;
	}
	}
	return std::nullopt;
}

std::optional<Value&> Vm::atGlobal(ObjString* name)
{
	auto optValue = m_globals->get(name);
	if (optValue.has_value())
		return *optValue;

	return m_builtins.get(name);
}

Vm::ResultWithValue Vm::getGlobal(ObjString* name)
{
	const auto value = atGlobal(name);
	if (value.has_value() == false)
	{
		RETURN_WITHOUT_VALUE(throwErrorWithMsg(m_nameErrorType, "'%s' is not defined", name->chars));
	}
	return ResultWithValue::ok(*value);
}

Vm::Result Vm::setGlobal(ObjString* name, Value& value)
{
	bool doesNotAlreadyExist = m_globals->set(name, value);
	if (doesNotAlreadyExist)
	{
		return throwErrorWithMsg(m_nameErrorType, "'%s' is not defined", name->chars);
	}
	return Result::ok();
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

Vm::Result Vm::importModule(ObjString* name)
{
	// At the end the stack has to contain the module and the return value of the main function which is always null.
	// If a module is already loaded the value has to be pushed anyway.
	if (const auto module = m_modules.get(name); module.has_value())
	{
		TRY_PUSH(*module);
		return Result::ok();
	}
	if (const auto module = m_nativeModulesMains.find(std::string_view(name->chars, name->size));
		module != m_nativeModulesMains.end())
	{
		const auto moduleObj = m_allocator->allocateModule();
		m_modules.set(name, Value(moduleObj));
		const auto main = m_allocator->allocateForeignFunction(name, module->second.main, 0, &moduleObj->globals, module->second.data);
		TRY_PUSH(Value(moduleObj));
		TRY(callValue(Value(main), 0, 0));
		m_stack.pop(); // Pop the return value.
		return Result::ok();
	}
	auto path = std::filesystem::absolute(
		m_sourceInfo->workingDirectory / std::filesystem::path(std::string_view(name->chars, name->size)));
	if (path.has_extension() == false)
		path.replace_extension("voxl");
	auto pathString = m_allocator->allocateStringConstant(path.string()).value;
	if (auto module = m_modules.get(pathString); module.has_value())
	{
		TRY_PUSH(*module);
		return Result::ok();
	}
	SourceInfo sourceInfo;
	// Should this be filename or pathString?
	sourceInfo.displayedFilename = name->chars;
	sourceInfo.workingDirectory = std::move(path.remove_filename());
	auto source = stringFromFile(pathString->chars);
	if (source.has_value() == false)
	{
		return fatalError("couldn't open file %s", pathString->chars);
	}
	// If cannot find search the std library.
	sourceInfo.source = *source;
	auto scannerResult = m_scanner->parse(sourceInfo, *m_errorReporter);
	auto parserResult = m_parser->parse(scannerResult.tokens, sourceInfo, *m_errorReporter);
	if (scannerResult.hadError || parserResult.hadError)
	{
		return fatalError("failed to parse");
	}
	auto compilerResult = m_compiler->compile(parserResult.ast, sourceInfo, *m_errorReporter);
	if (compilerResult.hadError)
	{
		return fatalError("failed to compile");
	}
	TRY_PUSH(Value(compilerResult.module));
	m_modules.set(pathString, Value(compilerResult.module));
	TRY(callFromVmAndReturnValue(Value(compilerResult.program)));
	m_stack.pop();
	return Result::ok();
}

Vm::Result Vm::importAllFromModule(ObjModule* module)
{
	if (module->isLoaded == false)
	{
		return fatalError("cannot use all from partially initialized module");
	}
	for (auto& [key, value] : module->globals)
	{
		// TODO: maybe check if this overrides.
		// Would require the function to return if the key already exits. Right now it sets it and returns false.
		if (isModuleMemberPublic(key))
			m_globals->set(key, value);
		// TODO: Change
		//setGlobal(key, value);
		//setGlobal(key, value);
	}
	return Result::ok();
}

Vm::Result Vm::pushDummyCallFrame()
{
	m_callStack.top().instructionPointerBeforeCall = m_instructionPointer;
	TRY_PUSH_CALL_STACK();
	m_callStack.top().instructionPointerBeforeCall = m_instructionPointer;
	m_callStack.top().callable = nullptr;
	return Result::ok();
}

void Vm::popCallStack()
{
	m_callStack.pop();
	m_instructionPointer = m_callStack.top().instructionPointerBeforeCall;
	auto callable = m_callStack.top().callable;
	if (callable != nullptr)
	{
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
}

bool Vm::isModuleMemberPublic(const ObjString* name)
{
	return (name->size > 0) && (name->chars[0] != '_');
}

Vm::Result Vm::callAndReturnValue(const Value& calle, Value* values, int argCount)
{
	int numberOfValuesToPopOffExceptArgs = 0;
	if (calle.isObj() && (calle.as.obj->isClass() || calle.as.obj->isBoundFunction()))
	{
		numberOfValuesToPopOffExceptArgs = 1;
		TRY_PUSH(calle);
	}

	for (int i = 0; i < argCount; i++)
	{
		TRY_PUSH(values[i]);
	}

	TRY(callValue(calle, argCount, numberOfValuesToPopOffExceptArgs));

	if (calle.isObj())
	{
		// Make this check inside callValue, because this is error prone.
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
		else if (calle.as.obj->isBoundFunction() && calle.as.obj->asBoundFunction()->callable->isNativeFunction())
		{
			shouldCallRun = false;
		}

		if (shouldCallRun == false)
		{
			return Result::ok();
		}
	}

	return run();
}

Vm::Result Vm::throwErrorWithMsg(ObjClass* class_, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const auto result = throwErrorWithMsgImplementation(class_, format, args);
	va_end(args);
	return result;
}

Vm::Result Vm::throwErrorWithMsgImplementation(ObjClass* class_, const char* format, va_list args)
{
	const auto instance = m_allocator->allocateInstance(class_);
	TRY_PUSH(Value(instance)); // GC

	const auto message = formatToTempBuffer(format, args);

	const auto string = m_allocator->allocateString(message);
	instance->fields.set(m_msgString, Value(string));
	m_stack.pop();
	return throwValue(Value(instance));
}

Vm::Result Vm::throwTypeErrorUnsupportedOperandTypesFor(const char* op, const Value& a, const Value& b)
{
	const auto aClass = getClass(a), bClass = getClass(b);
	if ((aClass.has_value() == false) || (bClass.has_value() == false))
	{
		return throwErrorWithMsg(m_typeErrorType, "unsupported operand types for %s", op);
	}

	return throwErrorWithMsg(
		m_typeErrorType, 
		"unsupported operand types for %s: '%s' and '%s'",
		op, aClass->name->chars, bClass->name->chars);
}

Vm::Result Vm::throwTypeErrorExpectedFound(ObjClass* expected, const Value& found)
{
	const auto foundClass = getClass(found);
	if (foundClass.has_value())
	{
		return throwErrorWithMsg(
			m_typeErrorType,
			"expected '%s', found '%s'",
			expected->name->chars, foundClass->name->chars);
	}
	else
	{
		return throwErrorWithMsg(m_typeErrorType, "expected '%s'", expected->name->chars);
	}
}

Vm::Result Vm::equals()
{
	auto a = m_stack.peek(1), b = m_stack.peek(0);
	auto returnValue = [this](bool value)
	{
		m_stack.pop();
		m_stack.pop();
		TRY_PUSH(Value(value));
		return Result::ok();
	};

	if (a.isInt() && a.isFloat())
		return returnValue(a.asInt() == a.asFloat());

	if (a.isFloat() && b.isInt())
		return returnValue(a.asFloat() == static_cast<Float>(b.asInt()));

	if (a.isInt() && b.isInt())
		return returnValue(a.asInt() == b.asInt());

	if (a.isFloat() && b.isFloat())
		return returnValue(a.asFloat() == b.asFloat());

	if (a.isNull() && b.isNull())
		return returnValue(true);
	
	if (a.isBool() && b.isBool())
		return returnValue(a.asBool() == b.asBool());

	if (a.isObj())
	{
		const auto aObj = a.asObj();
		const auto bObj = a.asObj();

		const auto method = getMethod(a, m_eqString);
		if (method.has_value())
		{
			TRY(callFromVmAndReturnValue(*method, m_stack.topPtr - 2));
			if (m_stack.top().isBool() == false)
				TRY(throwTypeErrorExpectedFound(m_boolType, m_stack.top()));
			return Result::ok();
		}

		return returnValue((a.type == b.type) && (aObj == bObj));
	}

	return returnValue(false);
}

Vm::Result Vm::callFromVmAndReturnValue(const Value& calle, Value* values, int argCount)
{
	TRY(pushDummyCallFrame());
	const auto result = callAndReturnValue(calle, values, argCount);
	if (result.type == ResultType::Exception)
		TRY(throwValue(result.exceptionValue));
	TRY(result);
	popCallStack();
	return Result::ok();
}

void Vm::mark(Vm* vm, Allocator& allocator)
{
	for (auto& value : vm->m_stack)
	{
		allocator.addValue(value);
	}

	// Maybe be faster not to add the strings becuase they are constants.
	allocator.addHashTable(vm->m_modules);

#define ADD(obj) \
	if (vm->obj != nullptr) \
		allocator.addObj(vm->obj);
	ADD(m_listType);
	ADD(m_dictType);
	ADD(m_numberType);
	ADD(m_intType);
	ADD(m_floatType);
	ADD(m_boolType);
	ADD(m_typeType);
	ADD(m_nullType);
	ADD(m_listIteratorType);
	ADD(m_stopIterationType);
	ADD(m_stringType);
	ADD(m_typeErrorType);
	ADD(m_nameErrorType);
	ADD(m_zeroDivisionErrorType);
#undef ADD

	allocator.addHashTable(vm->m_builtins);
	for (const auto& frame : vm->m_callStack)
	{
		if (frame.callable != nullptr)
			allocator.addObj(frame.callable);
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

Vm::Result Vm::Result::ok()
{
	return Result(ResultType::Ok);
}

Vm::Result Vm::Result::exception(const Value& value)
{
	Result result(ResultType::Exception);
	result.exceptionValue = value;
	return result;
}

Vm::Result Vm::Result::exceptionHandled()
{
	return Result(ResultType::ExceptionHandled);
}

Vm::Result Vm::Result::fatal()
{
	return Result(ResultType::Fatal);
}

Vm::Result::Result(ResultType type)
	: type(type)
{}

Vm::ResultWithValue Vm::ResultWithValue::ok(const Value& value)
{
	ResultWithValue result(ResultType::Ok);
	result.value = value;
	return result;
}

Vm::ResultWithValue Vm::ResultWithValue::exception(const Value& value)
{
	ResultWithValue result(ResultType::Exception);
	result.value = value;
	return result;
}

Vm::ResultWithValue Vm::ResultWithValue::exceptionHandled()
{
	return ResultWithValue(ResultType::ExceptionHandled);
}

Vm::ResultWithValue Vm::ResultWithValue::fatal()
{
	return ResultWithValue(ResultType::Fatal);
}

Vm::ResultWithValue::ResultWithValue(ResultType type)
	: type(type)
{}