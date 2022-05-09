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

Allocator::~Allocator()
{
	auto obj = m_head;
	while (obj != nullptr)
	{
		freeObj(obj);
		obj = obj->next;
	}

	for (const auto& constant : m_constants)
	{
		if (constant.isObj())
		{
			freeObj(constant.as.obj);
			::operator delete(constant.as.obj);
		}
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
	ObjString string;
	string.chars = chars.data();
	string.size = chars.size();
	string.length = length;
	auto result = m_stringPool.find(&string);
	if (result != m_stringPool.end())
	{
		return *result;
	}

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

ObjFunction* Allocator::allocateFunction(ObjString* name, int argCount)
{
	auto obj = reinterpret_cast<ObjFunction*>(allocateObj(sizeof(ObjFunction), ObjType::Function));
	obj->argCount = argCount;
	obj->name = name;
	new (&obj->byteCode) ByteCode();
	return obj;
}

ObjClosure* Allocator::allocateClosure(ObjFunction* function)
{
	auto obj = reinterpret_cast<ObjClosure*>(allocateObj(sizeof(ObjClosure), ObjType::Closure));
	obj->function = function;
	obj->upvalueCount = function->upvalueCount;
	obj->upvalues = reinterpret_cast<ObjUpvalue**>(::operator new(sizeof(ObjUpvalue*) * obj->upvalueCount));
	return obj;
}

ObjUpvalue* Allocator::allocateUpvalue(Value* localVariable)
{
	auto obj = reinterpret_cast<ObjUpvalue*>(allocateObj(sizeof(ObjUpvalue), ObjType::Upvalue));
	obj->location = localVariable;
	obj->value = Value::null();
	return obj;
}

ObjInstanceHead* Allocator::allocateInstance(ObjClass* class_)
{
	auto obj = allocateObj(class_->instanceSize, ObjType::Instance)->asInstance();
	obj->class_ = class_;
	return obj;
}

ObjBoundFunction* Allocator::allocateBoundFunction(ObjFunction* function, const Value& value)
{
	auto obj = allocateObj(sizeof(ObjBoundFunction), ObjType::BoundFunction)->asBoundFunction();
	obj->function = function;
	obj->value = value;
	return obj;
}

Allocator::StringConstant Allocator::allocateStringConstant(std::string_view chars)
{
	return allocateStringConstant(chars, Utf8::strlen(chars.data(), chars.size()));
}

Allocator::StringConstant Allocator::allocateStringConstant(std::string_view chars, size_t length)
{
	ObjString string;
	string.chars = chars.data();
	string.size = chars.size();
	string.length = length;
	auto result = m_stringPool.find(&string);
	if (result != m_stringPool.end())
	{
		return { createConstant(Value(reinterpret_cast<Obj*>(*result))), *result };
	}

	auto obj = reinterpret_cast<ObjString*>(::operator new(sizeof(ObjString) + chars.size() + 1));
	obj->obj.type = ObjType::String;
	obj->obj.newLocation = reinterpret_cast<Obj*>(obj);
	auto data = reinterpret_cast<char*>(obj + 1);
	memcpy(data, chars.data(), chars.size());
	obj->size = chars.size();
	data[chars.size()] = '\0';
	obj->chars = data;
	obj->length = length;
	return { createConstant(Value(reinterpret_cast<Obj*>(obj))), obj };
}

Allocator::FunctionConstant Allocator::allocateFunctionConstant(ObjString* name, int argCount)
{
	auto obj = reinterpret_cast<ObjFunction*>(::operator new(sizeof(ObjFunction)));
	obj->obj.newLocation = reinterpret_cast<Obj*>(obj);
	obj->obj.type = ObjType::Function;
	obj->argCount = argCount;
	obj->name = name;
	new (&obj->byteCode) ByteCode();
	return { createConstant(Value(reinterpret_cast<Obj*>(obj))), obj };
}

ObjNativeFunction* Allocator::allocateForeignFunction(ObjString* name, NativeFunction function, int argCount)
{
	auto obj = reinterpret_cast<ObjNativeFunction*>(allocateObj(sizeof(ObjNativeFunction), ObjType::ForeignFunction));
	obj->obj.type = ObjType::ForeignFunction;
	obj->name = name;
	obj->function = function;
	obj->argCount = argCount;
	return obj;
}

ObjClass* Allocator::allocateClass(ObjString* name, size_t instanceSize, MarkingFunction mark, UpdateFunction update)
{
	auto obj = allocateObj(sizeof(ObjClass), ObjType::Class)->asClass();
	obj->name = name;
	obj->instanceSize = instanceSize;
	obj->mark = mark;
	obj->update = update;

	HashTable::init(obj->fields);
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
			break;
		}

		case ObjType::ForeignFunction:
		{
			auto function = reinterpret_cast<ObjNativeFunction*>(obj);
			auto newFunction = reinterpret_cast<ObjNativeFunction*>(allocateObj(sizeof(ObjNativeFunction), ObjType::ForeignFunction));
			copyObj(newFunction, function, sizeof(ObjNativeFunction));
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
			newClass->name = reinterpret_cast<ObjString*>(copyToNewLocation(reinterpret_cast<Obj*>(class_->name)));
			obj->newLocation = reinterpret_cast<Obj*>(newClass);
			break;
		}

		case ObjType::Instance:
		{
			auto instance = obj->asInstance();
			auto newInstance = allocateObj(instance->class_->instanceSize, ObjType::Instance)->asInstance();
			copyObj(newInstance, instance, instance->class_->instanceSize);
			newInstance->class_ = reinterpret_cast<ObjClass*>(copyToNewLocation(reinterpret_cast<Obj*>(instance->class_)));
			newInstance->class_->update(instance, newInstance, *this);
			obj->newLocation = reinterpret_cast<Obj*>(newInstance);
			break;
		}

		case ObjType::BoundFunction:
		{
			auto function = obj->asBoundFunction();
			auto newFunction = allocateObj(sizeof(ObjBoundFunction), ObjType::BoundFunction)->asBoundFunction();
			copyObj(newFunction, function, sizeof(ObjBoundFunction));
			if (function->value.isObj())
			{
				newFunction->value.as.obj = copyToNewLocation(reinterpret_cast<Obj*>(function->value.as.obj));
			}
			obj->newLocation = reinterpret_cast<Obj*>(newFunction);
			break;
		}

		case ObjType::Closure:
		{
			auto closure = obj->asClosure();
			auto newClosure = allocateObj(sizeof(ObjClosure), ObjType::Closure)->asClosure();
			copyObj(newClosure, closure, sizeof(ObjClosure));
			for (size_t i = 0; i < newClosure->upvalueCount; i++)
			{
				auto& upvalue = newClosure->upvalues[i];
				upvalue = reinterpret_cast<ObjUpvalue*>(copyToNewLocation(reinterpret_cast<Obj*>(upvalue)));
			}
			newClosure->function = reinterpret_cast<ObjFunction*>(copyToNewLocation(reinterpret_cast<Obj*>(closure->function)));
			obj->newLocation = reinterpret_cast<Obj*>(newClosure);
			break;
		}

		case ObjType::Upvalue:
		{
			auto upvalue = obj->asUpvalue();
			auto newUpvalue = allocateObj(sizeof(ObjUpvalue), ObjType::Upvalue)->asUpvalue();
			copyObj(newUpvalue, upvalue, sizeof(ObjUpvalue));
			if (upvalue->value.isObj())
			{
				newUpvalue->value.as.obj = copyToNewLocation(reinterpret_cast<Obj*>(newUpvalue->value.as.obj));
			}
			if (upvalue->location == &upvalue->value)
			{
				newUpvalue->location = &newUpvalue->value;
			}
			obj->newLocation = reinterpret_cast<Obj*>(newUpvalue);
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
				if (bucket.value.isObj())
				{
					bucket.value.as.obj = copyToNewLocation(reinterpret_cast<Obj*>(bucket.value.as.obj));
				}
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
			return;
		}

		case ObjType::ForeignFunction:
		{
			const auto function = reinterpret_cast<ObjNativeFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
			m_markedMemorySize += sizeof(ObjNativeFunction) + WORST_CASE_SIZE_FOR_ALIGNMENT;
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
			addObj(reinterpret_cast<Obj*>(class_->name));
			m_markedMemorySize += sizeof(ObjClass) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::Instance:
		{
			const auto instance = obj->asInstance();
			addObj(reinterpret_cast<Obj*>(instance->class_));
			instance->class_->mark(instance, *this);
			m_markedMemorySize += instance->class_->instanceSize + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::BoundFunction:
		{
			const auto function = obj->asBoundFunction();
			addValue(function->value);
			addObj(reinterpret_cast<Obj*>(function->function));
			m_markedMemorySize += sizeof(ObjBoundFunction) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

		case ObjType::Closure:
		{
			const auto closure = obj->asClosure();
			for (size_t i = 0; i < closure->upvalueCount; i++)
			{
				addObj(reinterpret_cast<Obj*>(closure->upvalues[i]));
			}
			addObj(reinterpret_cast<Obj*>(closure->function));
			m_markedMemorySize += sizeof(ObjClosure) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}
		
		case ObjType::Upvalue:
		{
			const auto upvalue = obj->asUpvalue();
			addValue(upvalue->value);
			m_markedMemorySize += sizeof(ObjUpvalue) + WORST_CASE_SIZE_FOR_ALIGNMENT;
			return;
		}

	}

	ASSERT_NOT_REACHED();
}

size_t Allocator::createConstant(const Value& value)
{
	for (size_t i = 0; i < m_constants.size(); i++)
	{
		const auto& constant = m_constants[i];
		if (value.type == constant.type)
		{
			switch (value.type)
			{
				case ValueType::Int: 
					if (value.as.intNumber == constant.as.intNumber)
						return i;
					break;
				case ValueType::Float:
					if (value.as.floatNumber == constant.as.floatNumber)
						return i;
					break;
				case ValueType::Obj:
					if ((value.as.obj->type == ObjType::String)
						&& (constant.as.obj->type == ObjType::String)
						&& (value.as.obj == constant.as.obj))
						return i;
					break;

				case ValueType::Null:
				case ValueType::Bool:
					// null and bools should be created using instructions.
					ASSERT_NOT_REACHED();
			}
		}
	}

	m_constants.push_back(value);
	return m_constants.size() - 1;
}

void Allocator::runGc()
{
	m_markedObjs.clear();
	m_markedMemorySize = 0;
	for (auto& [function, data, _] : m_markingFunctions)
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
		function(data, data, *this);
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
	return obj->newLocation != nullptr;
}

bool Allocator::hasBeenMoved(Obj* obj)
{
	return (obj->newLocation != nullptr) && (obj->newLocation != reinterpret_cast<Obj*>(1));
}

void Allocator::unregisterMarkingFunction(size_t id)
{
	m_markingFunctions.erase(
		std::find_if(m_markingFunctions.begin(), m_markingFunctions.end(), [id](const MarkingFunctionEntry& entry)
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

const Value& Allocator::getConstant(size_t id) const
{
	return m_constants[id];
}

Allocator::MarkingFunctionHandle::~MarkingFunctionHandle()
{
	allocator.unregisterMarkingFunction(id);
}

Allocator::UpdateFunctionHandle::~UpdateFunctionHandle()
{
	allocator.unregisterUpdateFunction(id);
}
