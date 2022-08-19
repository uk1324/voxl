#include <Vm/Number.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue Number::floor(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::floor(num), c);
}

LocalValue Number::ceil(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::ceil(num), c);
}

LocalValue Number::round(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::round(num), c);
}

LocalValue Number::pow(Context& c)
{
	const auto base = c.args(0).asNumber();
	const auto exponent = c.args(1).asNumber();
	return LocalValue::floatNum(::pow(base, exponent), c);
}

LocalValue Number::sqrt(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::sqrt(num), c);
}

LocalValue Number::is_inf(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::isinf(num), c);
}

LocalValue Number::is_nan(Context& c)
{
	const auto num = c.args(0).asNumber();
	return LocalValue::floatNum(::isnan(num), c);
}

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