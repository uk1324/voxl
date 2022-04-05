#pragma once

#include <ByteCode.hpp>
#include <Value.hpp>

#include <string_view>

namespace Lang
{

class GcNode
{

};

class Allocator
{
public:
	void* allocate(size_t size);
	void free(void* ptr);

	ObjString* allocateString(std::string_view chars);
	ObjFunction* allocateFunction(ObjString* name, int argumentCount);

private:

};

}