#include <../test/TestModule.hpp>
#include <Vm/Vm.hpp>
using namespace Voxl;

namespace Test
{

static LocalValue floor(Context& c)
{
	const auto arg = c.args(0);
	if (arg.isInt())
	{
		return arg;
	}
	if (arg.isFloat())
	{
		return LocalValue::intNum(static_cast<Int>(::floor(arg.asFloat())), c);
	}
	throw NativeException(LocalValue::intNum(0, c));
}

static LocalValue invoke(Context& c)
{
	return c.args(0)();
}

static LocalValue get_5(Context& c)
{
	return LocalValue::intNum(5, c);
}

static LocalValue throw_3(Context& c)
{
	throw NativeException(LocalValue::intNum(3, c));
}

static LocalValue imported_get_number(Context& c)
{
	auto module = c.useModule("./imported.voxl", "module");
	return module.get("get_number")();
}

static LocalValue imported_get_number_2(Context& c)
{
	return c.get("get_number")();
}

}

LocalValue testModuleMain(Context& c)
{
	using namespace Test;
	c.createFunction("floor", floor, 1);
	c.createFunction("invoke", invoke, 1);
	c.createFunction("get_5", get_5, 0);
	c.createFunction("throw_3", throw_3, 0);
	c.createFunction("imported_get_number", imported_get_number, 0);

	c.useAllFromModule("imported");
	c.createFunction("imported_get_number_2", imported_get_number_2, 0);

	c.createClass<U8>(
		"U8",
		{
			{ "$init", U8::init, U8::initArgCount },
			{ "get", U8::get, U8::getArgCount },
			{ "set", U8::set, U8::setArgCount }
		});

	return LocalValue::null(c);
}

LocalValue U8::init(Context& c)
{
	auto u8 = c.args(0).asObj<U8>();
	u8->value = c.args(1).asNumber();
	return LocalValue::null(c);
}

LocalValue U8::get(Context& c)
{
	auto u8 = c.args(0).asObj<U8>();
	return LocalValue::intNum(static_cast<Int>(u8->value), c);
}

LocalValue U8::set(Context& c)
{
	auto u8 = c.args(0).asObj<U8>();
	u8->value = c.args(1).asNumber();
	return LocalValue::null(c);
}
