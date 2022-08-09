#include <TestModule.hpp>

static Voxl::LocalValue test(Voxl::Context& c)
{
	return c.args(0);
}

Voxl::LocalValue testModuleMain(Voxl::Context& c)
{
	c.createFunction("test", test, 1);
	return Voxl::LocalValue::null(c);
}