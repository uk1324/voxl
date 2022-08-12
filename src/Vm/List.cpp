#include <Vm/List.hpp>
#include <Vm/Vm.hpp>
#include <Allocator.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue List::init(Context& c)
{
	auto list = c.args(0).asObj<List>();
	list->capacity = 0;
	list->size = 0;
	list->data = nullptr;
	return LocalValue::null(c);
}

LocalValue List::iter(Context& c)
{
	auto iteratorType = c.getGlobal("_ListIterator");
	ASSERT(iteratorType.has_value());
	return c.call(*iteratorType, c.args(0));
}

LocalValue List::push(Context& c)
{
	auto list = c.args(0).asObj<List>();

	if (list->size + 1 > list->capacity)
	{
		auto oldData = list->data;
		list->capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
		list->data = reinterpret_cast<Value*>(::operator new(sizeof(Value) * list->capacity));
		memcpy(list->data, oldData, sizeof(Value) * list->size);
	}

	list->data[list->size] = c.args(1).value;
	list->size++;
	// TODO: Maybe return the array back to allow chaining though in most languages
	// methods with side effects don't allow chaining. Could also return the inserted element.
	// value : null;
	// x.push(value = <expr>;
	// ---
	// value : x.push(<expr>);
	return LocalValue::null(c);
}

LocalValue List::get_size(Context& c)
{
	auto list = c.args(0).asObj<List>();
	return LocalValue::intNum(list->size, c);
}

LocalValue List::get_index(Context& c)
{
	auto list = c.args(0).asObj<List>();
	return LocalValue(list->data[c.args(1).asInt()], c);
}

LocalValue List::set_index(Context& c)
{
	// TODO: Change the order of arguments.
	return LocalValue(c.args(1).asObj<List>()->data[c.args(2).asInt()] = c.args(0).value, c);
}

void List::free(List* list)
{
	:: operator delete(list->data);
}

void List::mark(List* list, Allocator& allocator)
{
	for (size_t i = 0; i < list->size; i++)
	{
		allocator.addValue(list->data[i]);
	}
}

LocalValue ListIterator::init(Context& c)
{
	auto iterator = c.args(0).asObj<ListIterator>();
	auto list = c.args(1).asObj<List>();
	iterator->list = list.obj;
	iterator->index = 0;
	return LocalValue(iterator);
}

LocalValue ListIterator::next(Context& c)
{
	auto iterator = c.args(0).asObj<ListIterator>();
	if (iterator->index >= iterator->list->size) 
	{
		auto stopIterationType = c.getGlobal("StopIteration");
		ASSERT(stopIterationType.has_value());
		throw NativeException(c.call(*stopIterationType));
	}
	const auto& result = iterator->list->data[iterator->index];
	iterator->index++;
	return LocalValue(result, c);
}

void ListIterator::mark(ListIterator* iterator, Allocator& allocator)
{
	allocator.addObj(reinterpret_cast<Obj*>(iterator));
}
