#include <TestModule.hpp>
#include <Vm/Vm.hpp>

using namespace Voxl;

static LocalValue test(Context& c)
{
	return c.args(0);
}

static LocalValue testFromTest(Context& c)
{
	auto module = c.getGlobal("testModule");
	if (module.has_value())
	{
		auto function = (*module).at("test0");
		return function();
	}
	else
	{
		ASSERT_NOT_REACHED();
	}
	return LocalValue::null(c);
}

LocalValue testModuleMain(Context& c)
{
	c.useModule("./test.voxl", "testModule");

	c.createFunction("test", test, 1);
	c.createFunction("testFromTest", testFromTest, 0);

	return LocalValue::null(c);
}