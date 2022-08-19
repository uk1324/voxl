#pragma once

#include <Allocator.hpp>
#include <ByteCode.hpp>
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

class Context;
class LocalValue;

class Vm
{
	friend class Context;
	friend class LocalValue;
private:
	struct FatalException {};

public:
	// Order members to reduce the size.
	struct CallFrame
	{
		const uint8_t* instructionPointerBeforeCall;
		Value* values;
		ObjUpvalue** upvalues;
		Obj* callable;
		int numberOfValuesToPopOffExceptArgs;
		bool isInitializer;
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
		ExceptionHandled,
	};

	struct Result
	{
		[[nodiscard]] static Result ok();
		[[nodiscard]] static Result exception(const Value& value);
		[[nodiscard]] static Result exceptionHandled();
		[[nodiscard]] static Result fatal();

		ResultType type;
		Value exceptionValue;

		Result(ResultType type);
	};

	struct ResultWithValue
	{
		[[nodiscard]] static ResultWithValue ok(const Value& value);
		[[nodiscard]] static ResultWithValue exception(const Value& value);
		[[nodiscard]] static ResultWithValue exceptionHandled();
		[[nodiscard]] static ResultWithValue fatal();

		ResultType type;
		Value value;

		ResultWithValue(ResultType type);
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
		const SourceInfo& sourceInfo,
		ErrorReporter& errorReporter);
	void reset();

	void defineNativeFunction(std::string_view name, NativeFunction function, int argCount);
	void createModule(std::string_view name, NativeFunction moduleMain);

private:
	Result run();

	uint32_t readUint32();
	uint8_t readUint8();

	Result fatalError(const char* format, ...);
	Result callObjFunction(ObjFunction* function, int argCount, int numberOfValuesToPopOffExceptArgs, bool isInitializer);
	Result callValue(Value value, int argCount, int numberOfValuesToPopOffExceptArgs, bool isInitializer = false);
	// Not using const Value& becuase then get would need to be const and to do this getField would need to be const
	// so it would just need to be copy pasted. Or it could reutrn an index.
	std::optional<Value> atField(Value& value, ObjString* fieldName);
	std::optional<Value> getMethod(Value& value, ObjString* methodName);
	Result setField(const Value& lhs, ObjString* fieldName, const Value& rhs);
	// Returns on stack.
	Result getField(Value& value, ObjString* fieldName);
	Result throwValue(const Value& value);
	std::optional<ObjClass&> getClass(const Value& value);
	Value typeErrorExpected(ObjClass* type);
	std::optional<Value&> atGlobal(ObjString* name);
	ResultWithValue getGlobal(ObjString* name);
	Result setGlobal(ObjString* name, Value& value);
	void debugPrintStack();
	// If Result::Ok then on return Module is TOS.
	Result importModule(ObjString* name);
	Vm::Result pushDummyCallFrame();
	void popCallStack();
	static bool isModuleMemberPublic(const ObjString* name);
	// The call frame has to be either a native function or a dummy call frame before calling. Returns on the stack.
	Result callAndReturnValue(const Value& calle, Value* values = nullptr, int argCount = 0);
	Value callFromNativeFunction(const Value& calle, Value* values = nullptr, int argCount = 0);
	// Returns on the stack.
	Result callFromVmAndReturnValue(const Value& calle, Value* values = nullptr, int argCount = 0);
	Result throwNameError(const char* format, ...);

	//Result add()


private:
	static void mark(Vm* vm, Allocator& allocator);

public:
	std::unordered_map<std::string_view, NativeFunction> m_nativeModulesMains;

	HashTable m_modules;

	HashTable m_builtins;
	// Don't use directly use getGlobal() and setGlobal() instead.
	HashTable* m_globals;
	const uint8_t* m_instructionPointer;
	
	StaticStack<Value, 1024> m_stack;
	StaticStack<CallFrame, 128> m_callStack;
	StaticStack<ExceptionHandler, 128> m_exceptionHandlers;
	size_t m_finallyBlockDepth;
	Scanner* m_scanner;
	Parser* m_parser;
	Compiler* m_compiler;

	Allocator* m_allocator;

	ErrorReporter* m_errorReporter;
	const SourceInfo* m_sourceInfo;

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
	ObjString* m_strString;
	ObjString* m_emptyString;
	ObjString* m_msgString;

	ObjClass* m_listType;
	ObjClass* m_listIteratorType;
	ObjClass* m_numberType;
	ObjClass* m_intType;
	ObjClass* m_floatType;
	ObjClass* m_stringType;
	ObjClass* m_stopIterationType;
	ObjClass* m_typeErrorType;
	ObjClass* m_nameErrorType;

	Allocator::MarkingFunctionHandle m_rootMarkingFunctionHandle;
};

}