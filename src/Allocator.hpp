#pragma once

#include <Value.hpp>
#include <Obj.hpp>
#include <unordered_set>
#include <string_view>

namespace Voxl
{

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
		MarkingFunctionPtr function;
		void* data;
		size_t id;
	};

private:
	// Not sure if I should put the Node inside the Obj struct. Having the struct seperate from the Obj makes it possible to choose a GC at
	// runtime. But there are many differences between different collectors. A simple mark and sweep doesn't need to update pointers.
	// And more complicated collectors probably require a lot more things to work than a simple copying collector.

public:
	Allocator();
	~Allocator();

	template<typename T>
	MarkingFunctionHandle registerMarkingFunction(T* data, void (*function)(T*, Allocator&));
	void unregisterMarkingFunction(size_t id);

	Obj* allocateObj(size_t size, ObjType type);
	Obj* allocateObjConstant(size_t size, ObjType type);

	ObjString* allocateString(std::string_view chars);
	ObjString* allocateString(std::string_view chars, size_t length);
	//ObjFunction* allocateFunction(ObjString* name, int argCount, HashTable* globals);
	ObjClosure* allocateClosure(ObjFunction* function);
	ObjUpvalue* allocateUpvalue(Value* localVariable);
	ObjNativeFunction* allocateForeignFunction(ObjString* name, NativeFunction function, int argCount, HashTable* globals, void* context);
	ObjClass* allocateClass(ObjString* name);
	template<typename T>
	ObjClass* allocateNativeClass(ObjString* name, InitFunction<T> init, FreeFunction<T> free);
	struct Method
	{
		std::string_view name;
		NativeFunction function;
		int argCount;
	};
	template<typename T>
	ObjClass* allocateNativeClass(
		ObjString* name,
		std::initializer_list<Method> methods,
		HashTable* globals,
		InitFunction<T> init = nullptr,
		FreeFunction<T> free = nullptr,
		void* context = nullptr);
	ObjInstance* allocateInstance(ObjClass* class_);
	ObjNativeInstance* allocateNativeInstance(ObjClass* class_);
	ObjBoundFunction* allocateBoundFunction(Obj* callable, const Value& value);
	ObjModule* allocateModule();

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
	} allocateFunctionConstant(ObjString* name, int argCount, HashTable* globals);

	size_t createConstant(const Value& value);

	void runGc();

	void addObj(Obj* obj);
	void addValue(Value value);
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

	size_t m_bytesAllocated;
	size_t m_bytesAllocatedAfterWhichTheGcRuns;

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
			return (a->size == b->size) && (memcmp(a->chars, b->chars, a->size) == 0);
		}
	};

	std::unordered_set<ObjString*, ObjStringHasher, ObjStringComparator> m_stringPool;
};

}

template<typename T>
Voxl::Allocator::MarkingFunctionHandle Voxl::Allocator::registerMarkingFunction(T* data, void(*function)(T*, Allocator&))
{
	size_t id = m_markingFunctions.empty()
		? 0
		: m_markingFunctions.back().id + 1;

	m_markingFunctions.push_back(MarkingFunctionEntry{ 
		reinterpret_cast<MarkingFunctionPtr>(function),
		data,
		id
	});

	return MarkingFunctionHandle{ *this, id };
}

template<typename T>
Voxl::ObjClass* Voxl::Allocator::allocateNativeClass(ObjString* name, InitFunction<T> init, FreeFunction<T>free)
{
	static_assert(std::is_base_of_v<ObjNativeInstance, T>);
	auto obj = allocateObj(sizeof(ObjClass), ObjType::Class)->asClass();
	obj->name = name;
	// static_cast to prevent ambigous overload.
	obj->mark = reinterpret_cast<MarkingFunctionPtr>(static_cast<void(*)(T*, Allocator&)>(T::mark));
	obj->init = reinterpret_cast<InitFunctionPtr>(init);
	obj->free = reinterpret_cast<FreeFunctionPtr>(free);
	obj->instanceSize = sizeof(T);
	obj->nativeInstanceCount = 0;
	new (&obj->superclass) std::optional<ObjClass&>();
	new (&obj->fields) HashTable();
	return obj;
}

template<typename T>
Voxl::ObjClass* Voxl::Allocator::allocateNativeClass(
	ObjString* name,
	std::initializer_list<Method> methods, 
	HashTable* globals,
	InitFunction<T> init,
	FreeFunction<T> free,
	void* context)
{
	auto class_ = allocateNativeClass<T>(name, init, free);
	// Could make foreign functions constants instead of doing this.
	auto _ = registerMarkingFunction(class_, +[](ObjClass* class_, Allocator& allocator)
		{
			allocator.addObj(class_);
		});

	for (const auto& method : methods)
	{
		const auto methodName = allocateStringConstant(method.name).value;
		const auto methodDisplayedName =
			allocateStringConstant((std::string(name->chars, name->size) + "." + std::string(method.name))).value;
		const auto methodFunction = allocateForeignFunction(
			methodDisplayedName,
			method.function,
			method.argCount,
			globals,
			context);

		bool inserted = class_->fields.insertIfNotSet(methodName, Value(methodFunction));
		ASSERT(inserted);
	}

	return class_;
}