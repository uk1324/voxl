#include <Allocator.hpp>
#include <Asserts.hpp>
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

		default:
			ASSERT_NOT_REACHED();
	}

	return obj->newLocation;
}

void Allocator::runGc()
{
	m_markedObjs.clear();
	m_markedMemorySize = 0;
	std::cout << "marking start\n";
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
	std::cout << "marking end\n";

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

	memset(m_region2, 0, m_region2Size);

	std::cout << "survivors: \n";
	auto n = m_head;
	size_t surviversCount = 0;
	while (n != nullptr)
	{
		std::cout << Value(n) << '\n';
		n = n->next;
		surviversCount++;
	}
	std::cout << "survivors end \n";
}

void Allocator::markObj(Obj* obj)
{
	if (isMarked(obj))
		return;

	std::cout << Value(obj) << '\n';
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
	}

	ASSERT_NOT_REACHED();
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
		value.as.obj = value.as.obj->newLocation;
	}
}

Obj* Allocator::newObjLocation(Obj* value)
{
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
