#include <Allocator.hpp>
#include <Asserts.hpp>
#include <Debug/DebugOptions.hpp>
#include <Utf8.hpp>
#include <stdlib.h>

using namespace Lang;

Allocator::Allocator()
	: m_head(nullptr)
	, m_tail(nullptr)
	, m_region2Size(4096)
	, m_region1Size(4096)
	, m_allocationThatTriggeredGcSize(0)
	, m_markedMemorySize(0)
{
	m_region1 = reinterpret_cast<uint8_t*>(::operator new(m_region1Size));
	m_region2 = reinterpret_cast<uint8_t*>(::operator new(m_region2Size));
	m_nextAllocation = m_region1;
}

#include <iostream>

Allocator::~Allocator()
{
	auto obj = m_head;
	while (obj != nullptr)
	{
		freeObj(obj);
		obj = obj->next;
	}

	::operator delete(m_region1);
	::operator delete(m_region2);
}

//#define ALLOCATE(type) reinterpret_cast<Obj##type*>(allocateObj(sizeof(Obj##type), ObjType::type));

Obj* Allocator::allocateObj(size_t size, ObjType type)
{
	auto allocation = alignUp(m_nextAllocation, ALIGNMENT);

	if ((allocation + size) > (m_region1 + m_region1Size))
	{
		// Assert that runGc doesn't call itself.
		ASSERT(m_allocationThatTriggeredGcSize == 0);

		m_allocationThatTriggeredGcSize = (allocation - m_nextAllocation) + size;
		runGc();
		// Resetting because the GC may be triggered by calling runGc from outside of here.
		m_allocationThatTriggeredGcSize = 0;
		allocation = alignUp(m_nextAllocation, ALIGNMENT);
	}

	auto obj = reinterpret_cast<Obj*>(allocation);
	obj->type = type;
	obj->newLocation = nullptr;

	if (m_head == nullptr)
	{
		m_head = obj;
		m_head->next = nullptr;
		m_tail = m_head;
	}
	else
	{
		m_tail->next = obj;
		m_tail = obj;
		m_tail->next = nullptr;
	}
	
	m_nextAllocation = allocation + size;
	return obj;
}

ObjAllocation* Allocator::allocateRawMemory(size_t size)
{
	auto data = reinterpret_cast<ObjAllocation*>(allocateObj(sizeof(ObjAllocation) + size, ObjType::Allocation));
	data->size = size;
	return data;
}

ObjString* Allocator::allocateString(std::string_view chars)
{
	return allocateString(chars, Utf8::strlen(chars.data(), chars.size()));
}

ObjString* Allocator::allocateString(std::string_view chars, size_t length)
{
	auto obj = reinterpret_cast<ObjString*>(allocateObj(sizeof(ObjString) + (chars.size() + 1), ObjType::String));
	auto data = reinterpret_cast<char*>(obj) + sizeof(ObjString);
	obj->size = chars.size();
	obj->length = length;
	memcpy(data, chars.data(), obj->size);
	// Null terminating for compatiblity with foreign functions. There maybe be some issue if I wanted to create a string view like Obj
	// It would be simpler to just store everything as not null terminated then but I would still have to somehow maintain the gc roots.
	data[obj->size] = '\0';
	obj->chars = data;
	return obj;
}

ObjFunction* Allocator::allocateFunction(ObjString* name, int argumentCount)
{
	auto obj = reinterpret_cast<ObjFunction*>(allocateObj(sizeof(ObjFunction), ObjType::Function));
	obj->argumentCount = argumentCount;
	obj->name = name;
	new (&obj->byteCode) ByteCode();
	return obj;
}

ObjClass* Allocator::allocateClass(ObjString* name)
{
	auto obj = allocateObj(sizeof(ObjClass), ObjType::Class)->asClass();
	obj->name = name;
	HashTable::init(obj->fields);
	HashTable::init(obj->methods);
	return obj;
}

ObjInstance* Allocator::allocateInstance(ObjClass* class_)
{
	auto obj = allocateObj(sizeof(ObjInstance), ObjType::Instance)->asInstance();
	obj->class_ = class_;
	HashTable::init(obj->fields);
	// Cannot init the fields hash table here because the allocator doesn't know about obj so it would collect it.
	return obj;
}

ObjBoundFunction* Allocator::allocateBoundFunction(ObjFunction* function, ObjInstance* instance)
{
	auto obj = allocateObj(sizeof(ObjBoundFunction), ObjType::BoundFunction)->asBoundFunction();
	obj->function = function;
	obj->instance = instance;
	return obj;
}

ObjForeignFunction* Allocator::allocateForeignFunction(ObjString* name, ForeignFunction function)
{
	auto obj = reinterpret_cast<ObjForeignFunction*>(allocateObj(sizeof(ObjForeignFunction), ObjType::ForeignFunction));
	obj->obj.type = ObjType::ForeignFunction;
	obj->name = name;
	obj->function = function;
	return obj;
}

uint8_t* Allocator::alignUp(uint8_t* ptr, size_t alignment)
{
	auto p = reinterpret_cast<uintptr_t>(ptr);
	auto unalignment = p % alignment;
	return (unalignment == 0)
		? ptr
		: reinterpret_cast<uint8_t*>(p + (alignment - unalignment));
}

Obj* Allocator::copyToNewLocation(Obj* obj)
{
	if (hasBeenMoved(obj))
	{
		return obj->newLocation;
	}

	auto copyObj = [](void* dst, void* src, size_t size)
	{
		memcpy(
			reinterpret_cast<uint8_t*>(dst) + sizeof(Obj),
			reinterpret_cast<uint8_t*>(src) + sizeof(Obj),
			size - sizeof(Obj));
	};

	switch (obj->type)
	{
		case ObjType::String:
		{
			auto string = reinterpret_cast<ObjString*>(obj);
			auto size = sizeof(ObjString) + string->size + 1;
			auto newString = reinterpret_cast<ObjString*>(allocateObj(size, ObjType::String));
			copyObj(newString, string, size);
			newString->chars = reinterpret_cast<char*>(newString) + sizeof(ObjString);
			obj->newLocation = reinterpret_cast<Obj*>(newString);
			break;
		}

		case ObjType::Function:
		{
			auto function = reinterpret_cast<ObjFunction*>(obj);
			auto newFunction = reinterpret_cast<ObjFunction*>(allocateObj(sizeof(ObjFunction), ObjType::Function));
			copyObj(newFunction, function, sizeof(ObjFunction));
			newFunction->name = reinterpret_cast<ObjString*>(copyToNewLocation(reinterpret_cast<Obj*>(function->name)));
			obj->newLocation = reinterpret_cast<Obj*>(newFunction);
			for (auto& value : newFunction->byteCode.constants)
			{
				if (value.type == ValueType::Obj)
				{
					value.as.obj = reinterpret_cast<Obj*>(copyToNewLocation(value.as.obj));
				}
			}
			break;
		}

		case ObjType::ForeignFunction:
		{
			auto function = reinterpret_cast<ObjForeignFunction*>(obj);
			auto newFunction = reinterpret_cast<ObjForeignFunction*>(allocateObj(sizeof(ObjForeignFunction), ObjType::ForeignFunction));
			copyObj(newFunction, function, sizeof(ObjForeignFunction));
			newFunction->name = reinterpret_cast<ObjString*>(copyToNewLocation(reinterpret_cast<Obj*>(function->name)));
			obj->newLocation = reinterpret_cast<Obj*>(newFunction);
			break;
		}

		case ObjType::Allocation:
		{
			auto memory = reinterpret_cast<ObjAllocation*>(obj);
			size_t size = sizeof(ObjAllocation) + memory->size;
			auto newMemory = reinterpret_cast<ObjAllocation*>(allocateObj(size, ObjType::Allocation));
			copyObj(newMemory, memory, size);
			obj->newLocation = reinterpret_cast<Obj*>(newMemory);
			break;
		}

		case ObjType::Class:
		{
			auto class_ = obj->asClass();
			auto newClass = allocateObj(sizeof(ObjClass), ObjType::Class)->asClass();
			copyObj(newClass, class_, sizeof(ObjClass));
			copyToNewLocation(newClass->fields, class_->fields);
			copyToNewLocation(newClass->methods, class_->methods);
			newClass->name = reinterpret_cast<ObjString*>(copyToNewLocation(reinterpret_cast<Obj*>(class_->name)));
			obj->newLocation = reinterpret_cast<Obj*>(newClass);
			break;
		}

		case ObjType::Instance:
		{
			auto instance = obj->asInstance();
			auto newInstance = allocateObj(sizeof(ObjInstance), ObjType::Instance)->asInstance();
			copyObj(newInstance, instance, sizeof(ObjInstance));
			copyToNewLocation(newInstance->fields, instance->fields);
			newInstance->class_ = reinterpret_cast<ObjClass*>(copyToNewLocation(reinterpret_cast<Obj*>(instance->class_)));
			obj->newLocation = reinterpret_cast<Obj*>(newInstance);
			break;
		}

		default:
			ASSERT_NOT_REACHED();
	}

	return obj->newLocation;
}

void Allocator::copyToNewLocation(HashTable& newTable, HashTable& oldTable)
{
	if (oldTable.allocation != nullptr) // Read HashMap::init;
	{
		newTable.allocation = reinterpret_cast<ObjAllocation*>(copyToNewLocation(reinterpret_cast<Obj*>(oldTable.allocation)));
		for (size_t i = 0; i < newTable.capacity(); i++)
		{
			auto& bucket = newTable.data()[i];

			if (newTable.isBucketEmpty(bucket) == false)
			{
				bucket.key = reinterpret_cast<ObjString*>(copyToNewLocation(reinterpret_cast<Obj*>(bucket.key)));
				updateValue(bucket.value);
			}
		}
	}
}

void Allocator::markObj(Obj* obj)
{
	if (isMarked(obj))
		return;

	setMarked(obj);
	static constexpr auto WORST_CASE_SIZE_FOR_ALIGNMENT = ALIGNMENT - 1;
	switch (obj->type)
	{
		case ObjType::String:
		{
			const auto string = reinterpret_cast<ObjString*>(obj);
			m_markedMemorySize += sizeof(ObjString) + string->size + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::Function:
		{
			const auto function = reinterpret_cast<ObjFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
			m_markedMemorySize += sizeof(ObjFunction) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			for (auto& value : function->byteCode.constants)
			{
				addValue(value);
			}
			return;
		}

		case ObjType::ForeignFunction:
		{
			const auto function = reinterpret_cast<ObjForeignFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
			m_markedMemorySize += sizeof(ObjForeignFunction) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::Allocation:
		{
			const auto memory = reinterpret_cast<ObjAllocation*>(obj);
			m_markedMemorySize += sizeof(ObjAllocation) + memory->size + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::Class:
		{
			const auto class_ = obj->asClass();
			addHashTable(class_->fields);
			addHashTable(class_->methods);
			addObj(reinterpret_cast<Obj*>(class_->name));
			m_markedMemorySize += sizeof(ObjClass) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::Instance:
		{
			const auto instance = obj->asInstance();
			addHashTable(instance->fields);
			addObj(reinterpret_cast<Obj*>(instance->class_));
			m_markedMemorySize += sizeof(ObjInstance) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}
	}

	ASSERT_NOT_REACHED();
}

void Allocator::runGc()
{
	m_markedObjs.clear();
	m_markedMemorySize = 0;
	for (auto& [function, data, _] : m_rootMarkingFunctions)
	{
		function(data, *this);
	}

	while (m_markedObjs.empty() == false)
	{
		Obj* obj = m_markedObjs.back();
		m_markedObjs.pop_back();
		markObj(obj);
	}
	const auto requiredSize = m_markedMemorySize + m_allocationThatTriggeredGcSize;
	if (requiredSize > m_region2Size)
	{
		::operator delete(m_region2);
		const auto oldSizeDoubled = m_region2Size * 2;
		m_region2Size = oldSizeDoubled < requiredSize
			? requiredSize
			: oldSizeDoubled;
		m_region2 = reinterpret_cast<uint8_t*>(::operator new(m_region2Size));
	}

	std::swap(m_region1, m_region2);
	std::swap(m_region1Size, m_region2Size);
	m_nextAllocation = m_region1;

	auto obj = m_head;
	m_tail = m_head = nullptr;
	while (obj != nullptr)
	{
		if (isMarked(obj))
		{
			ASSERT(copyToNewLocation(obj) != nullptr);
		}
		else if (obj->newLocation == nullptr)
		{
			freeObj(obj);
		}
		obj = obj->next;
	}

	for (auto& [function, data, _] : m_updateFunctions)
	{
		function(data);
	}

#ifdef VOXL_DEBUG_STRESS_TEST_GC
	memset(m_region2, 0, m_region2Size);
#endif

}

void Allocator::addObj(Obj* obj)
{
	m_markedObjs.push_back(obj);
}

void Allocator::addValue(Value& value)
{
	if (value.type == ValueType::Obj)
	{
		addObj(value.as.obj);
	}
}

void Allocator::addHashTable(HashTable& hashTable)
{
	if (hashTable.allocation != nullptr) // Read HashMap::init.
	{
		for (size_t i = 0; i < hashTable.capacity(); i++)
		{
			auto& bucket = hashTable.data()[i];

			if (hashTable.isBucketEmpty(bucket) == false)
			{
				addObj(reinterpret_cast<Obj*>((bucket.key)));
				addValue(bucket.value);
			}
		}

		addObj(reinterpret_cast<Obj*>(hashTable.allocation));
	}
}

void Allocator::updateHashTable(HashTable& hashTable)
{
	if (hashTable.allocation != nullptr) // Read HashMap::init.
	{
		hashTable.allocation = reinterpret_cast<ObjAllocation*>(newObjLocation(reinterpret_cast<Obj*>(hashTable.allocation)));
		for (size_t i = 0; i < hashTable.capacity(); i++)
		{
			auto& bucket = hashTable.data()[i];

			if (hashTable.isBucketEmpty(bucket) == false)
			{
				bucket.key = reinterpret_cast<ObjString*>(newObjLocation(reinterpret_cast<Obj*>(bucket.key)));
				updateValue(bucket.value);
			}
		}
	}
}

void Allocator::setMarked(Obj* obj)
{
	obj->newLocation = reinterpret_cast<Obj*>(1);
}

bool Allocator::isMarked(Obj* obj)
{
	// TODO??? Maybe just check if is not nullptr.
	// What about constants they may have changing values that need to be marked like classes - static fields and methods.
	// Could use a bitmask.
	// Maybe make constants a special case and update them inside the allocator manually.
	return obj->newLocation == reinterpret_cast<Obj*>(1);
	//return obj->newLocation != nullptr;
}

bool Allocator::hasBeenMoved(Obj* obj)
{
	return (obj->newLocation != nullptr) && (obj->newLocation != reinterpret_cast<Obj*>(1));
}

void Allocator::unregisterRootMarkingFunction(size_t id)
{
	m_rootMarkingFunctions.erase(
		std::find_if(m_rootMarkingFunctions.begin(), m_rootMarkingFunctions.end(), [id](const RootMarkingFunctionEntry& entry)
		{
			return entry.id == id;
		})
	);
}

void Allocator::unregisterUpdateFunction(size_t id)
{
	m_updateFunctions.erase(
		std::find_if(m_updateFunctions.begin(), m_updateFunctions.end(), [id](const UpdateFunctionEntry& entry)
			{
				return entry.id == id;
			})
	);
}

void Allocator::freeObj(Obj* obj)
{
	switch (obj->type)
	{
		case ObjType::Function:
		{
			auto function = reinterpret_cast<ObjFunction*>(obj);
			function->byteCode.~ByteCode();
			break;
		}

		default:
			break;
	}
}

void Allocator::updateValue(Value& value)
{
	if (value.type == ValueType::Obj)
	{
		value.as.obj = newObjLocation(value.as.obj);
	}
}

Obj* Allocator::newObjLocation(Obj* value)
{
	ASSERT(value->newLocation != nullptr); // Didn't add the obj.
	ASSERT(value->newLocation != reinterpret_cast<Obj*>(1)); // Probably forgot to set newLocation in copyToNewLocation.
	return value->newLocation;
}

Allocator::RootMarkingFunctionHandle::~RootMarkingFunctionHandle()
{
	allocator.unregisterRootMarkingFunction(id);
}

Allocator::UpdateFunctionHandle::~UpdateFunctionHandle()
{
	allocator.unregisterUpdateFunction(id);
}
