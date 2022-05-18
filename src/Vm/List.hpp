#include <Value.hpp>

namespace Lang
{

struct List
{
	static VOXL_NATIVE_FN(init);
	static VOXL_NATIVE_FN(push);
	static VOXL_NATIVE_FN(get_size);
	static VOXL_NATIVE_FN(get_index);
	static VOXL_NATIVE_FN(set_index);

	static void free(List* list);
	static void mark(List* list, Allocator& allocator);

	ObjNativeInstance head;
	size_t capacity;
	size_t size;
	Value* data;
};

}