#pragma once

#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorPrinter.hpp>
#include <unordered_map>
#include <array>

namespace Lang
{

class Vm
{
public:
	enum class [[nodiscard]] Result
	{
		Success,
		RuntimeError,
	};

	struct CallFrame
	{
		const uint8_t* instructionPointer;
		Value* values;
		// this is pointless the executing function is always at the bottom of the call stack
		// thogh it does use one level of indirection less
		ObjFunction* function;
		int numberOfValuesToPopOffExceptArgs;
		uint32_t absoluteJumpToCatch;
		bool isTrySet;
		ObjUpvalue** upvalues;
		ObjClosure* closure;
	};

private:

public:
	Vm(Allocator& allocator);

public:
	Result execute(ObjFunction* program, ErrorPrinter& errorPrinter);
	void reset();

	void defineNativeFunction(std::string_view name, NativeFunction function, int argCount);

	//Value add(Value lhs, Value rhs);

private:
	Result run();

	uint32_t readUint32();
	uint8_t readUint8();

	const Value& peekStack(size_t depth = 0) const;
	Value& peekStack(size_t depth = 0);
	void popStack();
	void pushStack(Value value);
	CallFrame& callStackTop();
	const CallFrame& callStackTop() const;
	Result fatalError(const char* format, ...);
	Result callValue(Value value, int argCount, int numberOfValuesToPopOffExceptArgs);
	ObjClass* getClassOrNullptr(const Value& value);

private:
	static void mark(Vm* vm, Allocator& allocator);

public:
	HashTable m_globals;
	
	std::array<Value, 1024> m_stack;
	Value* m_stackTop;

	std::array<CallFrame, 100> m_callStack;
	size_t m_callStackSize;

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

	Allocator::MarkingFunctionHandle m_rootMarkingFunctionHandle;
};

}