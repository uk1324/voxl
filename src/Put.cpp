#include <Put.hpp>

using namespace Voxl;

LocalValue Voxl::put(Context& c)
{
	std::cout << c.args(0).value;
	return LocalValue::null(c);
}

LocalValue Voxl::putln(Context& c)
{
	std::cout << c.args(0).value << '\n';
	return LocalValue::null(c);
}