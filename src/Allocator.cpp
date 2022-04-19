#include <Allocator.hpp>
#include <Asserts.hpp>
#include <Utf8.hpp>
#include <stdlib.h>

using namespace Lang;

Allocator::Allocator()
	: m_memorySize(4096)
{
	m_memory = reinterpret_cast<char*>(malloc(m_memorySize));
	m_newMemory = reinterpret_cast<char*>(malloc(m_memorySize));
	m_nextAllocation = m_memory;
	m_head = nullptr;
	m_tail = m_head;
}

#include <iostream>

Allocator::~Allocator()
{
	std::cout << '\n';
	auto node = m_head;
	while (node != nullptr) 
	{
		Value v(reinterpret_cast<Obj*>(reinterpret_cast<char*>(node) + sizeof(Node)));
 		std::cout << v << '\n';
		node = node->next;
	}
}

void Allocator::registerRootMarkingFunction(void* data, RootMarkingFunction function)
{
	m_rootMarkingFunctions.push_back({ data, function });
}

void Allocator::registerUpdateFunction(void* data, UpdateFunction function)
{
	m_updateFunctions.push_back({ data, function });
}

void* Allocator::allocate(size_t size)
{
	static constexpr size_t ALIGNMENT = 8;

	auto thisAllocation = allignUp(m_nextAllocation, ALIGNMENT);

	if ((thisAllocation + sizeof(Node) + size) > (m_memory + m_memorySize))
	{
		runGc();
	}

	auto allocation = allignUp(m_nextAllocation, ALIGNMENT);
	auto header = reinterpret_cast<Node*>(allocation);
	header->isMarked = false;
	header->newLocation = nullptr;

	if (m_head == nullptr)
	{
		m_head = header;
		m_head->next = nullptr;
		m_tail = m_head;
	}
	else
	{
		m_tail->next = header;
		m_tail = header;
		m_tail->next = nullptr;
	}
	
	m_nextAllocation = allocation + sizeof(Node) + size;
	return allocation + sizeof(Node);
}

ObjString* Allocator::allocateString(std::string_view chars)
{
	return allocateString(chars, Utf8::strlen(chars.data(), chars.size()));
}

ObjString* Allocator::allocateString(std::string_view chars, size_t length)
{
	auto obj = reinterpret_cast<ObjString*>(allocate(sizeof(ObjString) + (chars.size() + 1)));
	obj->obj.type = ObjType::String;
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
	auto obj = reinterpret_cast<ObjFunction*>(allocate(sizeof(ObjFunction)));
	obj->obj.type = ObjType::Function;

	obj->argumentCount = argumentCount;
	obj->name = name;
	new (&obj->byteCode) ByteCode();
	return obj;
}

ObjForeignFunction* Allocator::allocateForeignFunction(ObjString* name, ForeignFunction function)
{
	auto obj = reinterpret_cast<ObjForeignFunction*>(allocate(sizeof(ObjForeignFunction)));
	obj->obj.type = ObjType::ForeignFunction;
	obj->name = name;
	obj->function = function;
	return obj;
}

char* Allocator::allignUp(char* ptr, size_t alignment)
{
	auto p = reinterpret_cast<uintptr_t>(ptr);

	auto unalignment = p % alignment;
	return (unalignment == 0)
		? ptr
		: reinterpret_cast<char*>(p + (alignment - unalignment));
}

void Allocator::setNewLocation(void* obj, void* newLocation)
{
	auto node = reinterpret_cast<Node*>(reinterpret_cast<char*>(obj) - sizeof(Node));
	node->newLocation = reinterpret_cast<void*>(newLocation);
}

void* Allocator::copyToNewLocation(void* obj)
{
	auto header = reinterpret_cast<Node*>(reinterpret_cast<char*>(obj) - sizeof(Node));
	Obj* o = reinterpret_cast<Obj*>(obj);
	if (header->newLocation != nullptr)
	{
		return header->newLocation;
	}

	switch (o->type)
	{
		case ObjType::String:
		{
			auto string = reinterpret_cast<ObjString*>(obj);
			auto size = sizeof(ObjString) + string->size + 1;
			auto newString = reinterpret_cast<ObjString*>(allocate(size));
			memcpy(newString, string, size);
			newString->chars = reinterpret_cast<char*>(newString) + sizeof(ObjString);
			header->newLocation = newString;
			break;
		}

		case ObjType::Function:
		{
			auto function = reinterpret_cast<ObjFunction*>(obj);
			auto newFunction = reinterpret_cast<ObjFunction*>(allocate(sizeof(ObjFunction)));
			memcpy(newFunction, function, sizeof(ObjFunction));
			newFunction->name = reinterpret_cast<ObjString*>(copyToNewLocation(function->name));
			header->newLocation = newFunction;
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
			auto newFunction = reinterpret_cast<ObjForeignFunction*>(allocate(sizeof(ObjForeignFunction)));
			memcpy(newFunction, function, sizeof(ObjForeignFunction));
			newFunction->name = reinterpret_cast<ObjString*>(copyToNewLocation(function->name));
			header->newLocation = newFunction;
			break;
		}

		default:
			ASSERT_NOT_REACHED();
	}
	return header->newLocation;

}

void Allocator::runGc()
{
	m_markedObjs.clear();
	for (auto& [data, function] : m_rootMarkingFunctions)
	{
		function(data, *this);
	}

	while (m_markedObjs.empty() == false)
	{
		Obj* obj = m_markedObjs.back();
		m_markedObjs.pop_back();
		markObj(obj);
	}

	std::swap(m_memory, m_newMemory);
	m_nextAllocation = m_memory;

	auto node = m_head;
	m_head = nullptr;
	m_tail = m_head;
	while (node != nullptr)
	{
		if (node->isMarked)
		{
			ASSERT(copyToNewLocation(reinterpret_cast<char*>(node) + sizeof(Node)) != nullptr);
		}
		node = node->next;
	}

	for (auto& [data, function] : m_updateFunctions)
	{
		function(data);
	}

	memset(m_newMemory, 0, m_memorySize);

	std::cout << "survivors: \n";
	auto n = m_head;
	while (n != nullptr)
	{
		std::cout << Value(reinterpret_cast<Obj*>(reinterpret_cast<char*>(n) + sizeof(Node))) << '\n';
		n = n->next;
	}
	std::cout << "survivors end \n";
}

void Allocator::markObj(Obj* obj)
{
	auto header = reinterpret_cast<Node*>(reinterpret_cast<char*>(obj) - sizeof(Node));
	header->isMarked = true;

	switch (obj->type)
	{
		case ObjType::String:
			return;

		case ObjType::Function:
		{
			auto function = reinterpret_cast<ObjFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
			return;
		}

		case ObjType::ForeignFunction:
		{
			auto function = reinterpret_cast<ObjForeignFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
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

// Add a list of values to update.
void Allocator::updateValue(Value& value)
{
	if (value.type == ValueType::Obj)
	{
		value.as.obj = newObjLocation(value.as.obj);
	}
}

Obj* Allocator::newObjLocation(Obj* obj)
{
	Node* node = reinterpret_cast<Node*>(reinterpret_cast<char*>(obj) - sizeof(Node));
	ASSERT(node->newLocation != nullptr);
	return reinterpret_cast<Obj*>(node->newLocation);
}

