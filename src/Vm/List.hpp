#include <Value.hpp>

namespace Lang
{

struct List : public ObjNativeInstance
{
	static LocalValue init(Context& c);
	static LocalValue iter(Context& c);
	static LocalValue push(Context& c);
	static LocalValue get_size(Context& c);
	static LocalValue get_index(Context& c);
	static LocalValue set_index(Context& c);

	static void free(List* list);
	static void mark(List* list, Allocator& allocator);

	size_t capacity;
	size_t size;
	Value* data;
};

struct ListIterator : public ObjNativeInstance
{
	static LocalValue init(Context& c);
	static LocalValue next(Context& c);

	static void mark(ListIterator* iterator, Allocator& allocator);

	List* list;
	size_t index;
};

}