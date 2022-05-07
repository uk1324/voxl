#pragma once
#include <Value.hpp>

namespace Lang
{

struct VoxlInstance
{
	ObjInstanceHead instance;
	HashTable fields;

	static VOXL_NATIVE_FN(init);
	static VOXL_NATIVE_FN(get_field);
	static VOXL_NATIVE_FN(set_field);

	static void mark(VoxlInstance* instance, Allocator& allocator);
	static void update(VoxlInstance* oldInstance, VoxlInstance* newInstance, Allocator& allocator);
};

}