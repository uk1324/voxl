#pragma once

#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorPrinter.hpp>
#include <StaticStack.hpp>
#include <unordered_map>
#include <array>

namespace Lang
{

enum class [[nodiscard]] VmResult
{
	Success,
	RuntimeError,
};

class Vm
{
private:
	struct FatalException {};

	// Order members to reduce the size.
	struct CallFrame
	{
		const uint8_t* instructionPointer;
		Value* values;
		ObjUpvalue** upvalues;
		// TODO: Could set to nullptr for native functions.
		ObjFunction* function;
		int numberOfValuesToPopOffExceptArgs;
		uint32_t absoluteJumpToCatch;
		bool isTrySet;
		// TODO: Try catch doesn't pop the block off the stack when an exception happens.
		// To fix this I could store the stack value before try on Op::TryBegin otherwise it would be set to nullptr
		// indicating that try isn't set.

		bool isNativeFunction();
		void setNativeFunction();
	};

	enum class ResultType
	{
		Ok,
		Exception,
		Fatal,
	};

	struct Result
	{
		[[nodiscard]] static Result ok();
		[[nodiscard]] static Result exception(const Value& value);
		[[nodiscard]] static Result fatal();

		ResultType type;
		Value value;

	private:
		Result(ResultType type);
	};

public:
	Vm(Allocator& allocator);

public:
	VmResult execute(ObjFunction* program, ErrorPrinter& errorPrinter);
	void reset();

	void defineNativeFunction(std::string_view name, NativeFunction function, int argCount);

	//Value add(Value lhs, Value rhs);
	Value call(Value& calle, Value* values, int argCount);

private:
	Result run();

	uint32_t readUint32();
	uint8_t readUint8();

	Result fatalError(const char* format, ...);
	Result callObjFunction(ObjFunction* function, int argCount, int numberOfValuesToPopOffExceptArgs);
	Result callValue(Value value, int argCount, int numberOfValuesToPopOffExceptArgs);
	Result throwValue(const Value& value);
	ObjClass* getClassOrNullptr(const Value& value);

private:
	static void mark(Vm* vm, Allocator& allocator);

public:
	HashTable m_globals;
	
	StaticStack<Value, 1024> m_stack;
	StaticStack<CallFrame, 100> m_callStack;

	Allocator* m_allocator;

	ErrorPrinter* m_errorPrinter;

	std::vector<ObjUpvalue*> m_openUpvalues;

	ObjString* m_initString;
	ObjString* m_addString;
	ObjString* m_subString;
	ObjString* m_mulString;
	ObjString* m_divString;
	ObjString* m_modString;
	ObjString* m_ltString;
	ObjString* m_leString;
	ObjString* m_gtString;
	ObjString* m_geString;
	ObjString* m_getIndexString;
	ObjString* m_setIndexString;
	ObjClass* m_listType;

	Allocator::MarkingFunctionHandle m_rootMarkingFunctionHandle;
};

}