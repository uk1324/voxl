#pragma once

#include <Value.hpp>
#include <unordered_set>
#include <string_view>

namespace Lang
{
// Could use custom destory function for std::unique_ptr for handles


// Maybe store a public bool that would make it so constants are allocated in a different place making them not needed to be compressed.
// Could just store the constant pool inside the allocator.
// Consants would also need to store the GcNode they could set newLocation that just points to itself so updating pointers works.

class Allocator
{
public:
	struct MarkingFunctionHandle
	{
		~MarkingFunctionHandle();

		Allocator& allocator;
		size_t id;
	};

private:

	struct MarkingFunctionEntry
	{
		MarkingFunction function;
		void* data;
		size_t id;
	};

private:
	// Not sure if I should put the Node inside the Obj struct. Having the struct seperate from the Obj makes it possible to choose a GC at
	// runtime. But there are many differences between different collectors. A simple mark and sweep doesn't need to update pointers.
	// And more complicated collectors probably require a lot more thing to work than this copying collector.

public:
	Allocator();
	~Allocator();

	template<typename T>
	MarkingFunctionHandle registerMarkingFunction(T* data, void (*function)(T*, Allocator&));
	void unregisterMarkingFunction(size_t id);

	Obj* allocateObj(size_t size, ObjType type);

	// Function allocating constants should return const Obj because they shouldn't be marked.
	ObjString* allocateString(std::string_view chars);
	ObjString* allocateString(std::string_view chars, size_t length);
	ObjFunction* allocateFunction(ObjString* name, int argCount);
	ObjClosure* allocateClosure(ObjFunction* function);
	ObjUpvalue* allocateUpvalue(Value* localVariable);
	ObjNativeFunction* allocateForeignFunction(ObjString* name, NativeFunction function, int argCount);
	ObjClass* allocateClass(ObjString* name, size_t instanceSize, MarkingFunction mark);
	ObjInstance* allocateInstance(ObjClass* class_);
	ObjNativeInstance* allocateNativeInstance(ObjClass* class_);
	ObjBoundFunction* allocateBoundFunction(Obj* callable, const Value& value);

	struct StringConstant
	{
		size_t constant;
		ObjString* value;
	}
	allocateStringConstant(std::string_view chars),
	allocateStringConstant(std::string_view chars, size_t length);

	struct FunctionConstant
	{
		size_t index;
		ObjFunction* value;
	} allocateFunctionConstant(ObjString* name, int argCount);

	size_t createConstant(const Value& value);

	void runGc();

	void addObj(Obj* obj);
	void addValue(Value& value);
	void addHashTable(HashTable& hashTable);
	const Value& getConstant(size_t id) const;
	void registerLocal(Obj** obj);
	void unregisterLocal(Obj** obj);
	void registerLocal(Value* value);
	void unregisterLocal(Value* value);

private:
	void markObj(Obj* obj);
	void freeObj(Obj* obj);

private:
	Obj* m_tail;
	Obj* m_head;

	std::vector<MarkingFunctionEntry> m_markingFunctions;
	
	// A stack is used instread of recursion to avoid stack overflow.
	std::vector<Obj*> m_markedObjs;

	std::vector<Value> m_constants;

	std::unordered_set<Obj**> m_localObjs;
	std::unordered_set<Value*> m_localValues;

	struct ObjStringHasher
	{
		size_t operator()(const ObjString* string) const
		{
			return std::hash<std::string_view>()(std::string_view(string->chars, string->size));
		}
	};

	struct ObjStringComparator
	{
		bool operator()(const ObjString* a, const ObjString* b) const
		{
			// TODO: Benchmark this -> Could also compare lengths but it probably wouldn't make it faster becuase that is a rare case.
			return (a->size == b->size) && memcmp(a->chars, b->chars, a->size);
		}
	};

	std::unordered_set<ObjString*, ObjStringHasher, ObjStringComparator> m_stringPool;
};

}

template<typename T>
Lang::Allocator::MarkingFunctionHandle Lang::Allocator::registerMarkingFunction(T* data, void(*function)(T*, Allocator&))
{
	size_t id = m_markingFunctions.empty()
		? 0
		: m_markingFunctions.back().id + 1;

	m_markingFunctions.push_back(MarkingFunctionEntry{ 
		reinterpret_cast<MarkingFunction>(function),
		data,
		id
	});

	return MarkingFunctionHandle{ *this, id };
}