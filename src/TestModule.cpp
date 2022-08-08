#include <TestModule.hpp>

static Lang::LocalValue test(Lang::Context& c)
{
	return c.args(0);
}

Lang::LocalValue testModuleMain(Lang::Context& c)
{
	c.createFunction("test", test, 1);
	return Lang::LocalValue::null(c);
}