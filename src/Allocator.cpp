#include <Allocator.hpp>
#include <Asserts.hpp>
#include <Debug/DebugOptions.hpp>
#include <Utf8.hpp>
#include <stdlib.h>

using namespace Lang;

Allocator::Allocator()
	: m_head(nullptr)
	, m_tail(nullptr)
{}

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
}

Obj* Allocator::allocateObj(size_t size, ObjType type)
{
#ifdef VOXL_DEBUG_STRESS_TEST_GC
	runGc();
#endif 

	auto obj = reinterpret_cast<Obj*>(::operator new(size));
	obj->type = type;
	obj->isMarked = false;

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
	
	return obj;
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

ObjInstance* Allocator::allocateInstance(ObjClass* class_)
{
	auto obj = allocateObj(sizeof(ObjInstance), ObjType::Instance)->asInstance();
	obj->class_ = class_;
	new (&obj->fields) HashTable();
	return obj;
}

ObjNativeInstance* Allocator::allocateNativeInstance(ObjClass* class_)
{
	auto obj = allocateObj(class_->instanceSize, ObjType::NativeInstance)->asNativeInstance();
	obj->class_ = class_;
	return obj;
}

ObjBoundFunction* Allocator::allocateBoundFunction(Obj* callable, const Value& value)
{
	auto obj = allocateObj(sizeof(ObjBoundFunction), ObjType::BoundFunction)->asBoundFunction();
	obj->callable= callable;
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
	obj->obj.isMarked = false;
	obj->obj.type = ObjType::String;
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
	obj->obj.type = ObjType::Function;
	obj->argCount = argCount;
	obj->name = name;
	obj->obj.isMarked = false;
	obj->upvalueCount = 0;
	new (&obj->byteCode) ByteCode();
	return { createConstant(Value(reinterpret_cast<Obj*>(obj))), obj };
}

ObjNativeFunction* Allocator::allocateForeignFunction(ObjString* name, NativeFunction function, int argCount)
{
	auto obj = reinterpret_cast<ObjNativeFunction*>(allocateObj(sizeof(ObjNativeFunction), ObjType::NativeFunction));
	obj->obj.type = ObjType::NativeFunction;
	obj->name = name;
	obj->function = function;
	obj->argCount = argCount;
	return obj;
}

ObjClass* Allocator::allocateClass(ObjString* name, size_t instanceSize, MarkingFunction mark)
{
	auto obj = allocateObj(sizeof(ObjClass), ObjType::Class)->asClass();
	obj->name = name;
	obj->mark = mark;
	obj->instanceSize = instanceSize;
	new (&obj->fields) HashTable();
	return obj;
}

void Allocator::markObj(Obj* obj)
{
	if (obj->isMarked)
		return;

	obj->isMarked = true;
	switch (obj->type)
	{
		case ObjType::String:
			return;

		case ObjType::Function:
		{
			const auto function = reinterpret_cast<ObjFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
			return;
		}

		case ObjType::NativeFunction:
		{
			const auto function = reinterpret_cast<ObjNativeFunction*>(obj);
			addObj(reinterpret_cast<Obj*>(function->name));
			return;
		}

		case ObjType::Class:
		{
			const auto class_ = obj->asClass();
			addHashTable(class_->fields);
			addObj(reinterpret_cast<Obj*>(class_->name));
			return;
		}

		case ObjType::Instance:
		{
			const auto instance = obj->asInstance();
			addObj(reinterpret_cast<Obj*>(instance->class_));
			addHashTable(instance->fields);
			return;
		}

		case ObjType::NativeInstance:
		{
			const auto instance = obj->asNativeInstance();
			addObj(reinterpret_cast<Obj*>(instance->class_));
			if (instance->class_->mark != nullptr)
			{
				instance->class_->mark(instance, *this);
			}
			return;
		}

		case ObjType::BoundFunction:
		{
			const auto function = obj->asBoundFunction();
			addValue(function->value);
			addObj(function->callable);
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
			return;
		}
		
		case ObjType::Upvalue:
		{
			const auto upvalue = obj->asUpvalue();
			addValue(upvalue->value);
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

	for (auto& [function, data, _] : m_markingFunctions)
	{
		function(data, *this);
	}

	while (m_markedObjs.empty() == false)
	{
		auto obj = m_markedObjs.back();
		m_markedObjs.pop_back();
		markObj(obj);
	}

	// Can't use erase remove on sets.
	for (auto it = m_stringPool.begin(); it != m_stringPool.end();)
	{
		if ((*it)->obj.isMarked)
			++it;
		else
			it = m_stringPool.erase(it);
	}

	Obj* previous = nullptr;
	auto obj = m_head;

	while (obj != nullptr)
	{
		if (obj->isMarked)
		{
			obj->isMarked = false;
			previous = obj;
			obj = obj->next;
		}
		else
		{
			if (previous == nullptr)
			{
				m_head = obj->next;
			}
			else
			{
				previous->next = obj->next;
			}
			auto next = obj->next;
			freeObj(obj);
			::operator delete(obj);
			obj = next;
		}
	}
}

void Allocator::addObj(Obj* obj)
{
	ASSERT(obj != nullptr);
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
	for (size_t i = 0; i < hashTable.capacity(); i++)
	{
		auto& bucket = hashTable.data()[i];

		if (hashTable.isBucketEmpty(bucket) == false)
		{
			addObj(reinterpret_cast<Obj*>((bucket.key)));
			addValue(bucket.value);
		}
	}
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

const Value& Allocator::getConstant(size_t id) const
{
	return m_constants[id];
}

Allocator::MarkingFunctionHandle::~MarkingFunctionHandle()
{
	allocator.unregisterMarkingFunction(id);
}