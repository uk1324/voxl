#include <../test/tests.hpp>

void success(std::string_view testName)
{
	std::cout << "[PASSED] " << testName << '\n';
}

void fail(std::string_view testName, int lineNumber, const char* filename)
{
	std::cerr << "[FAILED] " << testName << ' ' << filename << ':' << lineNumber << ' ';
}
