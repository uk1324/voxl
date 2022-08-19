#include <TestModule.hpp>
#include <Vm/Vm.hpp>

using namespace Voxl;

static LocalValue test(Context& c)
{
	return c.args(0);
}

static LocalValue testFromTest(Context& c)
{
	auto module = c.get("testModule");
	auto function = module.get("test0");
	return function();
}

LocalValue testModuleMain(Context& c)
{
	c.useModule("./test.voxl", "testModule");

	c.createFunction("test", test, 1);
	c.createFunction("testFromTest", testFromTest, 0);

	return LocalValue::null(c);
}