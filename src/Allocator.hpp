#pragma once

#include <Value.hpp>
#include <string_view>

namespace Lang
{
// Could use custom destory function for std::unique_ptr for handles


// Maybe store a public bool that would make it so constants are allocated in a different place making them not needed to be compressed.
// Could just store the constant pool inside the allocator.
// Consants would also need to store the GcNode they could set newLocation that just points to itself so updating pointers works.

	template<typename Key, typename Value, typename KeyTraits>
	class HashMap;
	using HashTable = HashMap<ObjString*, Value, ObjStringKeyTraits>;

class Allocator
{
public:
	struct RootMarkingFunctionHandle
	{
		~RootMarkingFunctionHandle();

		Allocator& allocator;
		size_t id;
	};

private:
	using RootMarkingFunction = void (*)(void*, Allocator&);

	struct RootMarkingFunctionEntry
	{
		RootMarkingFunction function;
		void* data;
		size_t id;
	};

public:
	struct UpdateFunctionHandle
	{
		~UpdateFunctionHandle();

		Allocator& allocator;
		size_t id;
	};

private:
	using UpdateFunction = void (*)(void*);

	struct UpdateFunctionEntry
	{
		UpdateFunction function;
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
	RootMarkingFunctionHandle registerRootMarkingFunction(T* data, void (*function)(T*, Allocator&));
	void unregisterRootMarkingFunction(size_t id);
	template<typename T>
	UpdateFunctionHandle registerUpdateFunction(T* data, void (*function)(T*));
	void unregisterUpdateFunction(size_t id);

	Obj* allocateObj(size_t size, ObjType type);

	// Function allocating constants should return const Obj because they shouldn't be marked.
	ObjAllocation* allocateRawMemory(size_t size);
	ObjString* allocateString(std::string_view chars);
	ObjString* allocateString(std::string_view chars, size_t length);
	ObjFunction* allocateFunction(ObjString* name, int argumentCount);
	ObjForeignFunction* allocateForeignFunction(ObjString* name, ForeignFunction function);
	ObjClass* allocateClass(ObjString* name);
	ObjInstance* allocateInstance(ObjClass* class_);
	ObjBoundFunction* allocateBoundFunction(ObjFunction* function, ObjInstance* instance);

	void runGc();

	void addObj(Obj* obj);
	void addValue(Value& value);
	void addHashTable(HashTable& hashTable);
	static void updateHashTable(HashTable& hashTable);
	static void updateValue(Value& value);
	static Obj* newObjLocation(Obj* value);

private:
	static uint8_t* alignUp(uint8_t* ptr, size_t alignment);
	static void setNewLocation(void* obj, void* newLocation);
	static void setMarked(Obj* obj);
	static bool isMarked(Obj* obj);
	static bool hasBeenMoved(Obj* obj);

	void markObj(Obj* obj);
	Obj* copyToNewLocation(Obj* obj);
	void copyToNewLocation(HashTable& newTable, HashTable& oldTable);
	void freeObj(Obj* obj);

private:
	Obj* m_tail;
	Obj* m_head;

	uint8_t* m_region1;
	size_t m_region1Size;
	uint8_t* m_region2;
	size_t m_region2Size;

	uint8_t* m_nextAllocation;

	std::vector<RootMarkingFunctionEntry> m_rootMarkingFunctions;
	std::vector<UpdateFunctionEntry> m_updateFunctions;
	
	size_t m_allocationThatTriggeredGcSize;
	// A stack is used instread of recursion to avoid stack overflow.
	std::vector<Obj*> m_markedObjs;
	size_t m_markedMemorySize;

	static constexpr size_t ALIGNMENT = 8;
};

}

template<typename T>
Lang::Allocator::RootMarkingFunctionHandle Lang::Allocator::registerRootMarkingFunction(T* data, void(*function)(T*, Allocator&))
{
	size_t id = m_rootMarkingFunctions.empty()
		? 0
		: m_rootMarkingFunctions.back().id + 1;

	m_rootMarkingFunctions.push_back(RootMarkingFunctionEntry{ 
		reinterpret_cast<RootMarkingFunction>(function),
		data,
		id
	});

	return RootMarkingFunctionHandle{ *this, id };
}

template<typename T>
Lang::Allocator::UpdateFunctionHandle Lang::Allocator::registerUpdateFunction(T* data, void(*function)(T*))
{
	size_t id = m_updateFunctions.empty()
		? 0
		: m_updateFunctions.back().id + 1;

	m_updateFunctions.push_back(UpdateFunctionEntry{
		reinterpret_cast<UpdateFunction>(function),
		data,
		id
	});

	return UpdateFunctionHandle{ *this, id };
}