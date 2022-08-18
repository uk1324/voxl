#include <Vm/Number.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue Number::sin(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::sin(num), c);
}

LocalValue Number::cos(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::cos(num), c);
}

LocalValue Number::tan(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::tan(num), c);
}
