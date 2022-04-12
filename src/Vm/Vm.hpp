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
		ObjFunction* function;
	};

private:
	// With string interning I could precompute the hash
	struct ObjStringHasher
	{
		size_t operator()(const ObjString* string) const;
	};

	struct ObjStringComparator
	{
		size_t operator()(const ObjString* a, const ObjString* b) const;
	};


public:
	Vm(Allocator& allocator);

public:
	Result execute(ObjFunction* program, ErrorPrinter& errorPrinter);
	void reset();

	void createForeignFunction(std::string_view name, ForeignFunction function);

private:
	Result run();


	uint32_t readUint32();
	uint8_t readUint8();

	//Result call(ObjFunction* function);

	const Value& peekStack(size_t depth = 0) const;
	Value& peekStack(size_t depth = 0);
	void popStack();
	void pushStack(Value value);
	CallFrame& callStackTop();
	const CallFrame& callStackTop() const;
	Result fatalError(const char* format, ...);

private:
	//// Storing a direct pointer should probably be faster than storing and index.
	//const uint8_t* m_instructionPointer;
	std::unordered_map<ObjString*, Value, ObjStringHasher, ObjStringComparator> m_globals;
	
	std::array<Value, 1024> m_stack;
	Value* m_stackTop;

	std::array<CallFrame, 100> m_callStack;
	size_t m_callStackSize;

	Allocator* m_allocator;

	ErrorPrinter* m_errorPrinter;
};

}