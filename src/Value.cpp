#include <Value.hpp>

using namespace Lang;

Value::Value(Int value)
	: type(ValueType::Int)
{
	as.intNumber = value;
}

Value::Value(Obj* obj)
	: type(ValueType::Obj)
{
	as.obj = obj;
}

Value Value::null()
{
	Value value;
	value.type = ValueType::Null;
	return value;
}
