#include <Lang/Allocator.hpp>

#include <stdlib.h>

using namespace Lang;

void* Allocator::allocate(size_t size)
{
	return malloc(size);
}

void Allocator::free(void* ptr)
{
	free(ptr);
}

Obj* Allocator::allocateString(std::string_view chars)
{
	auto obj = reinterpret_cast<ObjString*>(allocate(sizeof(ObjString)));
	obj->obj.type = ObjType::String;
	obj->chars = reinterpret_cast<char*>(allocate(chars.size()));
	obj->length = chars.size();
	memcpy(obj->chars, chars.data(), obj->length);
	return reinterpret_cast<Obj*>(obj);
}
