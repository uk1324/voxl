#include <Vm/List.hpp>
#include <Vm/Vm.hpp>
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

VOXL_NATIVE_FN(List::iter)
{
	return vm.call(Value(reinterpret_cast<Obj*>(vm.m_listIteratorType)), args, 1);
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
		throw NativeException(Value::null());
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
		throw NativeException(Value::null());
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

VOXL_NATIVE_FN(ListIterator::init)
{
	auto iterator = reinterpret_cast<ListIterator*>(args[0].as.obj);
	auto list = reinterpret_cast<List*>(args[1].as.obj);
	iterator->list = list;
	iterator->index = 0;
	return Value(reinterpret_cast<Obj*>(iterator));
}

VOXL_NATIVE_FN(ListIterator::next)
{
	auto iterator = reinterpret_cast<ListIterator*>(args[0].as.obj);
	if (iterator->index >= iterator->list->size) 
	{
		throw NativeException(vm.call(Value(reinterpret_cast<Obj*>(vm.m_stopIterationType)), nullptr, 0));
	}
	const auto& result = iterator->list->data[iterator->index];
	iterator->index++;
	return result;
}

void ListIterator::mark(ListIterator* iterator, Allocator& allocator)
{
	allocator.addObj(reinterpret_cast<Obj*>(iterator));
}
