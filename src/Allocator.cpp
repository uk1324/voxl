#include <Allocator.hpp>
#include <Asserts.hpp>
#include <Debug/DebugOptions.hpp>
#include <Utf8.hpp>
#include <stdlib.h>

using namespace Voxl;

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

Obj* Allocator::allocateObjConstant(size_t size, ObjType type)
{
	auto obj = reinterpret_cast<Obj*>(::operator new(size));
	obj->type = type;
	obj->isMarked = true;
	obj->next = nullptr;
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

	auto obj = allocateObj(sizeof(ObjString) + (chars.size() + 1), ObjType::String)->asString();
	auto data = reinterpret_cast<char*>(obj) + sizeof(ObjString);
	obj->size = chars.size();
	obj->length = length;
	memcpy(data, chars.data(), obj->size);
	// Null terminating for compatiblity with foreign functions. There maybe be some issue if I wanted to create a string view like Obj.
	data[obj->size] = '\0';
	obj->chars = data;
	obj->hash = ObjString::hashString(obj->chars, obj->size);
	m_stringPool.insert(obj);
	return obj;
}

//ObjFunction* Allocator::allocateFunction(ObjString* name, int argCount, HashTable* globals)
//{
//	auto obj = allocateObj(sizeof(ObjFunction), ObjType::Function)->asFunction();
//	obj->argCount = argCount;
//	obj->name = name;
//	obj->globals = globals;
//	new (&obj->byteCode) ByteCode();
//	return obj;
//}

ObjClosure* Allocator::allocateClosure(ObjFunction* function)
{
	auto obj = allocateObj(sizeof(ObjClosure), ObjType::Closure)->asClosure();
	obj->function = function;
	obj->upvalueCount = function->upvalueCount;
	obj->upvalues = reinterpret_cast<ObjUpvalue**>(::operator new(sizeof(ObjUpvalue*) * obj->upvalueCount));
	return obj;
}

ObjUpvalue* Allocator::allocateUpvalue(Value* localVariable)
{
	auto obj = allocateObj(sizeof(ObjUpvalue), ObjType::Upvalue)->asUpvalue();
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
	class_->nativeInstanceCount++;
	if (class_->init != nullptr)
		class_->init(obj);
	return obj;
}

ObjBoundFunction* Allocator::allocateBoundFunction(Obj* callable, const Value& value)
{
	auto obj = allocateObj(sizeof(ObjBoundFunction), ObjType::BoundFunction)->asBoundFunction();
	obj->callable= callable;
	obj->value = value;
	return obj;
}

ObjModule* Allocator::allocateModule()
{
	auto obj = allocateObj(sizeof(ObjModule), ObjType::Module)->asModule();
	obj->isLoaded = false;
	new (&obj->globals) HashTable();
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
		return { createConstant(Value(*result)), *result };
	}

	auto obj = allocateObjConstant(sizeof(ObjString) + chars.size() + 1, ObjType::String)->asString();
	//auto data = reinterpret_cast<char*>(obj + 1);
	auto data = reinterpret_cast<char*>(obj) + sizeof(ObjString);
	memcpy(data, chars.data(), chars.size());
	obj->size = chars.size();
	data[chars.size()] = '\0';
	obj->chars = data;
	obj->length = length;
	obj->hash = ObjString::hashString(obj->chars, obj->size);
	m_stringPool.insert(obj);
	return { createConstant(Value(obj)), obj };
}

Allocator::FunctionConstant Allocator::allocateFunctionConstant(ObjString* name, int argCount, HashTable* globals)
{
	auto obj = allocateObjConstant(sizeof(ObjFunction), ObjType::Function)->asFunction();
	obj->argCount = argCount;
	obj->name = name;
	obj->upvalueCount = 0;
	obj->globals = globals;
	new (&obj->byteCode) ByteCode();
	return { createConstant(Value(obj)), obj };
}

ObjNativeFunction* Allocator::allocateForeignFunction(ObjString* name, NativeFunction function, int argCount, HashTable* globals, void* context)
{
	auto obj = allocateObjConstant(sizeof(ObjNativeFunction), ObjType::NativeFunction)->asNativeFunction();
	obj->type = ObjType::NativeFunction;
	obj->name = name;
	obj->function = function;
	obj->argCount = argCount;
	obj->globals = globals;
	obj->context = context;
	return obj;
}

ObjClass* Allocator::allocateClass(ObjString* name)
{
	auto obj = allocateObj(sizeof(ObjClass), ObjType::Class)->asClass();
	obj->name = name;
	obj->mark = nullptr;
	obj->init = nullptr;
	obj->free = nullptr;
	obj->instanceSize = 0;
	obj->nativeInstanceCount = 0;
	new (&obj->superclass) std::optional<ObjClass&>();
	new (&obj->fields) HashTable();
	return obj;
}

// Probably don't need to add constants like function names.
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
			const auto function = obj->asFunction();
			addObj(function->name);
			return;
		}

		case ObjType::NativeFunction:
		{
			const auto function = obj->asNativeFunction();
			addObj(function->name);
			return;
		}

		case ObjType::Class:
		{
			const auto class_ = obj->asClass();
			addHashTable(class_->fields);
			addObj(class_->name);
			if (class_->superclass.has_value())
				addObj(&*class_->superclass);
			return;
		}

		case ObjType::Instance:
		{
			const auto instance = obj->asInstance();
			addObj(instance->class_);
			addHashTable(instance->fields);
			return;
		}

		case ObjType::NativeInstance:
		{
			const auto instance = obj->asNativeInstance();
			addObj(instance->class_);
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
				addObj(closure->upvalues[i]);
			}
			addObj(closure->function);
			return;
		}
		
		case ObjType::Upvalue:
		{
			const auto upvalue = obj->asUpvalue();
			addValue(upvalue->value);
			return;
		}

		case ObjType::Module:
		{
			const auto module = obj->asModule();
			addHashTable(module->globals);
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
						/*&& (constant.as.obj->asString()->size == value.as.obj->asString()->size)
						&& (memcmp(constant.as.obj->asString()->chars, value.as.obj->asString()->chars, value.as.obj->asString()->size) == 0))*/
						&& (value.as.obj == constant.as.obj))
					{
						return i;
					}
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

	for (auto& obj : m_localObjs)
	{
		addObj(*obj);
	}
	for (auto& value : m_localValues)
	{
		addValue(*value);
	}

	while (m_markedObjs.empty() == false)
	{
		auto obj = m_markedObjs.back();
		m_markedObjs.pop_back();
		markObj(obj);
	}

	// Remove this
	for (const auto o : m_constants)
	{
		if (o.isObj())
		{
			ASSERT(o.as.obj->isMarked);
		}
	}

	// Can't use erase remove on sets.
	for (auto it = m_stringPool.begin(); it != m_stringPool.end();)
	{
		if ((*it)->isMarked)
			++it;
		else
			it = m_stringPool.erase(it);
	}

	Obj* previous = nullptr;
	auto obj = m_head;

	// Constant's don't need to be added because this only deletes objects created normally.
	while (obj != nullptr)
	{
		if (obj->isMarked || (obj->isClass() && obj->asClass()->nativeInstanceCount > 0))
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
	m_tail = previous;
}

void Allocator::addObj(Obj* obj)
{
	// Allowing nullptrs might make it harder to find bugs when objects are erroneously set to nullptr or
	// for example when using a copying GC in debug mode the memory of the old region is be memset to 0
	// which would cause a segfault if the pointer is used and this assert would trigger if something were to try mark old memory.
	ASSERT(obj != nullptr);
	m_markedObjs.push_back(obj);
}

void Allocator::addValue(Value value)
{
	if (value.isObj())
	{
		addObj(value.asObj());
	}
}

void Allocator::addHashTable(HashTable& hashTable)
{
	for (auto& [key, value] : hashTable)
	{
		addObj(key);
		addValue(value);
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
			auto function = obj->asFunction();
			function->byteCode.~ByteCode();
			break;
		}

		case ObjType::Closure:
		{
			auto closure = obj->asClosure();
			::operator delete(closure->upvalues);
			break;
		}

		case ObjType::NativeInstance:
		{
			auto instance = obj->asNativeInstance();

			instance->class_->nativeInstanceCount--;
			// If this was a copying garbage collector this would be valid.
			// Store free inside instance or use reference counting.
			if (instance->class_->free != nullptr)
				instance->class_->free(instance);
			break;
		}

		case ObjType::Instance:
		{
			auto instance = obj->asInstance();
			instance->fields.~HashTable();
			break;
		}

		case ObjType::Class:
		{
			auto class_ = obj->asClass();
			class_->fields.~HashTable();
			break;
		}

		case ObjType::Module:
		{
			auto module = obj->asModule();
			module->globals.~HashTable();
			break;
		}

		case ObjType::Upvalue:
		case ObjType::NativeFunction:
		case ObjType::BoundFunction:
		case ObjType::String:
			// TODO: Maybe remove from string pool here?
			break;
	}
}

const Value& Allocator::getConstant(size_t id) const
{
	return m_constants[id];
}

void Allocator::registerLocal(Obj** obj)
{
	const auto isNewItem = m_localObjs.insert(obj).second == true;
	ASSERT(isNewItem);
}

void Allocator::unregisterLocal(Obj** obj)
{
	const auto wasDeleted = m_localObjs.erase(obj) == 1;
	ASSERT(wasDeleted);
}

void Allocator::registerLocal(Value* value)
{
	const auto isNewItem = m_localValues.insert(value).second == true;
	ASSERT(isNewItem);
}

void Allocator::unregisterLocal(Value* value)
{
	const auto wasDeleted = m_localValues.erase(value) == 1;
	ASSERT(wasDeleted);
}

Allocator::MarkingFunctionHandle::~MarkingFunctionHandle()
{
	allocator.unregisterMarkingFunction(id);
}