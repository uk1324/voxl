#pragma once

#include <Value.hpp>

#include <string_view>

namespace Lang
{

// Maybe store a public bool that would make it so constants are allocated in a different place making them not needed to be compressed.
// Could just store the constant pool inside the allocator.
// Consants would also need to store the GcNode they could set newLocation that just points to itself so updating pointers works.
class Allocator
{
public:
	using RootMarkingFunction = void (*)(void*, Allocator&);
	using UpdateFunction = void (*)(void*);

private:
	// Not sure if I should put the Node inside the Obj struct. Having the struct seperate from the Obj makes it possible to choose a GC at
	// runtime. But there are many differences between different collectors. A simple mark and sweep doesn't need to update pointers.
	// And more complicated collectors probably require a lot more thing to work than this copying collector.
	class Node
	{
	public:
		bool isMarked;
		// Can be nullptr.
		Node* next;
		void* newLocation;
		//Obj* newLocation;
	};

public:
	Allocator();
	~Allocator();

	void registerRootMarkingFunction(void* data, RootMarkingFunction function);
	void registerUpdateFunction(void* data, UpdateFunction function);

	void* allocate(size_t size);

	ObjAllocation* allocateRawMemory(size_t size);
	ObjString* allocateString(std::string_view chars);
	ObjString* allocateString(std::string_view chars, size_t length);
	ObjFunction* allocateFunction(ObjString* name, int argumentCount);
	ObjForeignFunction* allocateForeignFunction(ObjString* name, ForeignFunction function);

	void markObj(Obj* obj);
	void addObj(Obj* obj);
	void addValue(Value& value);

	static void updateValue(Value& value);
	static Obj* newObjLocation(Obj* obj);

private:
	static char* allignUp(char* ptr, size_t alignment);
	static void setNewLocation(void* obj, void* newLocation);

	void* copyToNewLocation(void* obj);

	void runGc();

private:
	size_t m_memorySize;
	Node* m_tail;
	Node* m_head;
	char* m_nextAllocation;
	char* m_memory;

	char* m_newMemory;
	// There will be 2 memory regions the second one can be set to nullptr at the start because you can use free on a nullptr.

	std::vector<std::pair<void*, RootMarkingFunction>> m_rootMarkingFunctions;
	std::vector<std::pair<void*, UpdateFunction>> m_updateFunctions;

	std::vector<Obj*> m_markedObjs;
};

}