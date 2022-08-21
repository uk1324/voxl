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

static LocalValue numbers(Context& c)
{
	auto a = LocalValue::intNum(5, c), b = LocalValue::floatNum(2.5, c);
	auto put = c.get("put");

	//put(a + b);

	return LocalValue::null(c);
}

LocalValue testModuleMain(Context& c)
{
	c.useModule("./test.voxl", "testModule");

	c.createFunction("test", test, 1);
	c.createFunction("testFromTest", testFromTest, 0);
	c.createFunction("numbers", numbers, 0);

	return LocalValue::null(c);
}