#include <Allocator.hpp>

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

Obj* Allocator::allocateString(std::string_view chars)
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
	return reinterpret_cast<Obj*>(obj);
}
