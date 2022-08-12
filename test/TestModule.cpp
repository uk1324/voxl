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
	return c.call(c.args(0));
}

static LocalValue get_5(Context& c)
{
	return LocalValue::intNum(5, c);
}

static LocalValue throw_3(Context& c)
{
	throw NativeException(LocalValue::intNum(3, c));
}

}
LocalValue testModuleMain(Context& c)
{
	using namespace Test;
	c.createFunction("floor", floor, 1);
	c.createFunction("invoke", invoke, 1);
	c.createFunction("get_5", get_5, 0);
	c.createFunction("throw_3", throw_3, 0);
	return LocalValue::null(c);
}
