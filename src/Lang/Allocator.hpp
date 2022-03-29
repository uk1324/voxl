#pragma once

#include <Lang/Obj.hpp>

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

		Obj* allocateString(std::string_view chars);

	private:

	};
}