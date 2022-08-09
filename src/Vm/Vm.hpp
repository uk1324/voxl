#pragma once

#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorPrinter.hpp>
#include <StaticStack.hpp>
#include <Parsing/Scanner.hpp>
#include <Parsing/Parser.hpp>
#include <Compiling/Compiler.hpp>
#include <unordered_map>
#include <array>

namespace Voxl
{

enum class VmResult
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
		ObjFunction* function;
		Obj* callable;
		Value* caughtValue;
		int numberOfValuesToPopOffExceptArgs;
		bool isInitializer;
		// TODO: Try catch doesn't pop the block off the stack when an exception happens.
		// To fix this I could store the stack value before try on Op::TryBegin otherwise it would be set to nullptr
		// indicating that try isn't set.

		bool isNativeFunction() const;
		void setNativeFunction();
	};

	struct ExceptionHandler
	{
		Value* stackTopPtrBeforeTry;
		const uint8_t* handlerCodeLocation;
		CallFrame* callFrame;
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
	VmResult execute(
		ObjFunction* program,
		ObjModule* module,
		Scanner& scanner,
		Parser& parser,
		Compiler& compiler,
		ErrorPrinter& errorPrinter);
	void reset();

	void defineNativeFunction(std::string_view name, NativeFunction function, int argCount);
	void createModule(std::string_view name, NativeFunction moduleMain);

	//Value add(Value lhs, Value rhs);
	Value call(const Value& calle, Value* values, int argCount);

private:
	Result run();

	uint32_t readUint32();
	uint8_t readUint8();

	Result fatalError(const char* format, ...);
	Result callObjFunction(ObjFunction* function, int argCount, int numberOfValuesToPopOffExceptArgs);
	Result callValue(Value value, int argCount, int numberOfValuesToPopOffExceptArgs);
	Result throwValue(const Value& value);
	ObjClass* getClassOrNullptr(const Value& value);
	Value typeErrorExpected(ObjClass* type);
	std::optional<Value&> getGlobal(ObjString* name);
	bool setGlobal(ObjString* name, const Value& value);
	void debugPrintStack();

private:
	static void mark(Vm* vm, Allocator& allocator);

public:
	std::unordered_map<std::string_view, NativeFunction> m_nativeModulesMains;

	HashTable m_modules;

	HashTable m_builtins;
	// Don't use directly use getGlobal() and setGlobal() instead.
	HashTable* m_globals;
	
	StaticStack<Value, 1024> m_stack;
	StaticStack<CallFrame, 128> m_callStack;
	StaticStack<ExceptionHandler, 128> m_exceptionHandlers;

	Scanner* m_scanner;
	Parser* m_parser;
	Compiler* m_compiler;

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
	ObjClass* m_listIteratorType;
	ObjClass* m_intType;
	ObjClass* m_stringType;
	ObjClass* m_stopIterationType;
	ObjClass* m_typeErrorType;

	Allocator::MarkingFunctionHandle m_rootMarkingFunctionHandle;
};

}