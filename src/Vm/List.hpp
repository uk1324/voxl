#include <Value.hpp>

namespace Lang
{

struct List
{
	static LocalValue init(Context& c);
	static LocalValue iter(Context& c);
	static LocalValue push(Context& c);
	static LocalValue get_size(Context& c);
	static LocalValue get_index(Context& c);
	static LocalValue set_index(Context& c);

	static void free(List* list);
	static void mark(List* list, Allocator& allocator);

	ObjNativeInstance head;
	size_t capacity;
	size_t size;
	Value* data;
};

struct ListIterator
{
	static LocalValue init(Context& c);
	static LocalValue next(Context& c);

	static void mark(ListIterator* iterator, Allocator& allocator);

	ObjNativeInstance head;
	List* list;
	size_t index;
};

}