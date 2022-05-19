#include <Vm/List.hpp>
#include <Allocator.hpp>

using namespace Lang;

VOXL_NATIVE_FN(List::init)
{
	auto list = reinterpret_cast<List*>(args[0].as.obj);
	list->capacity = 0;
	list->data = 0;
	list->size = 0;
	return Value(reinterpret_cast<Obj*>(list));
}

VOXL_NATIVE_FN(List::push)
{
	auto list = reinterpret_cast<List*>(args[0].as.obj);
	const auto& value = args[1];

	if (list->size + 1 > list->capacity)
	{
		auto oldData = list->data;
		list->capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
		list->data = reinterpret_cast<Value*>(::operator new(sizeof(Value) * list->capacity));
		memcpy(list->data, oldData, sizeof(Value) * list->size);
	}

	list->data[list->size] = value;
	list->size++;

	return Value::null();
}

VOXL_NATIVE_FN(List::get_size)
{
	auto list = reinterpret_cast<List*>(args[0].as.obj);
	return Value(static_cast<Int>(list->size));
}


VOXL_NATIVE_FN(List::get_index)
{
	auto list = reinterpret_cast<List*>(args[0].as.obj);
	const auto& index = args[1];
	if (index.isInt() == false)
	{
		return NativeFunctionResult::exception(Value::null());
	}
	return list->data[index.as.intNumber];
}

VOXL_NATIVE_FN(List::set_index)
{
	const auto& value = args[0];
	auto list = reinterpret_cast<List*>(args[1].as.obj);
	const auto& index = args[2];
	if (index.isInt() == false)
	{
		return NativeFunctionResult::exception(Value::null());
	}
	return list->data[index.as.intNumber] = value;
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
