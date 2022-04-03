#pragma once

#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorPrinter.hpp>
#include <unordered_map>

namespace Lang
{

class Vm
{
public:
	enum class Result
	{
		Success,
		RuntimeError,
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
	Vm();

public:
	Result run(const ByteCode& program, Allocator& allocator, ErrorPrinter& errorPrinter);

private:
	uint32_t readUint32();
	uint8_t readUint8();

	void printValue(const Value& value);

	const Value& peekStack(size_t depth = 0) const;
	Value& peekStack(size_t depth = 0);
	void popStack();
	void pushStack(Value value);

private:
	// Storing a direct pointer should probably be faster than storing and index.
	const uint8_t* m_instructionPointer;
	std::vector<Value> m_stack;
	std::unordered_map<ObjString*, Value, ObjStringHasher, ObjStringComparator> m_globals;

	Allocator* m_allocator;

	ErrorPrinter* m_errorPrinter;
};

}