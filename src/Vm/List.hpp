#include <Value.hpp>
#include <Allocator.hpp>

namespace Voxl
{

struct List : public ObjNativeInstance
{
	static LocalValue iter(Context& c);
	static LocalValue push(Context& c);
	static LocalValue get_size(Context& c);
	static LocalValue get_index(Context& c);
	static LocalValue set_index(Context& c);

	void init();
	void push(const Value& value);

	static void init(List* list);
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

	static void init(ListIterator* iterator);
	static void mark(ListIterator* iterator, Allocator& allocator);

	List* list;
	size_t index;
};

}