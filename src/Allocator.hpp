#pragma once

#include <ByteCode.hpp>
#include <Value.hpp>

#include <string_view>

namespace Lang
{

class GcNode
{

};

// Maybe store a public bool that would make it so constants are allocated in a different place making them not needed to be compressed.
// Could just store the constant pool inside the allocator.
class Allocator
{
public:
	void* allocate(size_t size);
	void free(void* ptr);

	ObjString* allocateString(std::string_view chars);
	ObjFunction* allocateFunction(ObjString* name, int argumentCount);
	ObjForeignFunction* allocateForeignFunction(ObjString* name, ForeignFunction function);

private:

};

}