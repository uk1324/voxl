#include <../test/tests.hpp>
#include <TerminalColors.hpp>

using namespace Voxl;

void success(std::string_view testName)
{
	std::cout 
		<< TerminalColors::GREEN << "[PASSED] " << TerminalColors::RESET
		<< testName << '\n';
}

void fail(std::string_view testName, int lineNumber, const char* filename)
{
	std::cerr 
		<< TerminalColors::RED << "[FAILED] " << TerminalColors::RESET 
		<< testName << ' ' << filename << ':' << lineNumber << ' ';
}
