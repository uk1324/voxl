#include <Vm/String.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue String::len(Context& c)
{
	const auto string = c.args(0).asString();
	return LocalValue::intNum(string.len(), c);
}
