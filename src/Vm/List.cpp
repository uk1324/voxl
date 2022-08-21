#include <Vm/List.hpp>
#include <Vm/Vm.hpp>
#include <Allocator.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue List::iter(Context& c)
{
	auto iteratorType = c.get("_ListIterator");
	return iteratorType(c.args(0));
}

LocalValue List::push(Context& c)
{
	auto list = c.args(0).asObj<List>();
	list->push(c.args(1).value);
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
	return LocalValue(c.args(0).asObj<List>()->data[c.args(1).asInt()] = c.args(2).value, c);
}

void List::push(const Value& value)
{
	if (size + 1 > capacity)
	{
		auto oldData = data;
		capacity = (capacity == 0) ? 8 : capacity * 2;
		data = reinterpret_cast<Value*>(::operator new(sizeof(Value) * capacity));
		memcpy(data, oldData, sizeof(Value) * size);
	}

	data[size] = value;
	size++;
}

void List::init(List* list)
{
	list->capacity = 0;
	list->size = 0;
	list->data = nullptr;
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
	return LocalValue(iterator);
}

LocalValue ListIterator::next(Context& c)
{
	auto iterator = c.args(0).asObj<ListIterator>();
	if (iterator->index >= iterator->list->size) 
	{
		auto stopIterationType = c.get("StopIteration");
		throw NativeException(stopIterationType());
	}
	const auto& result = iterator->list->data[iterator->index];
	iterator->index++;
	return LocalValue(result, c);
}

void ListIterator::construct(ListIterator* iterator)
{
	iterator->list = nullptr;
	iterator->index = 0;
}

void ListIterator::mark(ListIterator* iterator, Allocator& allocator)
{
	if (iterator->list == nullptr)
		return;
	allocator.addObj(reinterpret_cast<Obj*>(iterator));
}
