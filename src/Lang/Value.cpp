#include <Lang/Value.hpp>

using namespace Lang;

Value::Value(Int value)
	: type(ValueType::Int)
{
	as.intNumber = value;
}
