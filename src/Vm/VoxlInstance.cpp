#include <Vm/VoxlInstance.hpp>

using namespace Lang;

VOXL_NATIVE_FN(VoxlInstance::init)
{
	auto instance = reinterpret_cast<VoxlInstance*>(args[0].as.obj->asInstance());
	HashTable::init(instance->fields);
	return Value(reinterpret_cast<Obj*>(instance));
}

VOXL_NATIVE_FN(VoxlInstance::get_field)
{
	auto instance = reinterpret_cast<VoxlInstance*>(args[0].as.obj->asInstance());
	const auto& field = args[1];

	auto optResult = instance->fields.get(field.as.obj->asString());
	if (optResult.has_value())
	{
		return **optResult;
	}

	return Value::null();
}

VOXL_NATIVE_FN(VoxlInstance::set_field)
{
	auto instance = reinterpret_cast<VoxlInstance*>(args[1].as.obj->asInstance());
	const auto& value = args[0];
	auto field = args[2].as.obj->asString();

	instance->fields.insert(allocator, field, value);
	return value;
}

void VoxlInstance::mark(VoxlInstance* instance, Allocator& allocator)
{
	allocator.addHashTable(instance->fields);
}

void VoxlInstance::update(VoxlInstance* oldInstance, VoxlInstance* newInstance, Allocator& allocator)
{
	allocator.copyToNewLocation(newInstance->fields, oldInstance->fields);
}
