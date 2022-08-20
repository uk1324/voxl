#include <Vm/Errors.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue GenericStringError::init(Context& c)
{
	auto self = c.args(0);
	auto string = c.args(1);
	self.set("msg", string);
	return LocalValue::null(c);
}

LocalValue GenericStringError::str(Context& c)
{
	auto self = c.args(0);
	return self.get("msg");
}