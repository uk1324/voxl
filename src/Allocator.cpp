#include <Allocator.hpp>
#include <new>
#include <stdlib.h>

using namespace Lang;

void* Allocator::allocate(size_t size)
{
	return malloc(size);
}

void Allocator::free(void* ptr)
{
	::free(ptr);
}

ObjString* Allocator::allocateString(std::string_view chars)
{
	auto obj = reinterpret_cast<ObjString*>(allocate(sizeof(ObjString)));
	obj->obj.type = ObjType::String;
	auto data = reinterpret_cast<char*>(allocate(chars.size() + 1));
	obj->length = chars.size();
	memcpy(data, chars.data(), obj->length);
	// Null terminating for compatiblity with foreign functions. There maybe be some issue if I wanted to create a string view like Obj
	// It would be simpler to just store everything as not null terminated then but I would still have to somehow maintain the gc roots.
	data[obj->length] = '\0';
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
