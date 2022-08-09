﻿#include <../test/tests.hpp>
#include <Parsing/Scanner.hpp>
#include <Parsing/Parser.hpp>
#include <Compiling/Compiler.hpp>
#include <Vm/Vm.hpp>
#include <Debug/DebugOptions.hpp>
#include <ReadFile.hpp>
#include <fstream>
#include <sstream>

using namespace Voxl;

std::pair<std::string_view, std::string_view> tests[] = {
	{ "variable_scoping", "0010" },
	{ "numbers_loop", "0123456789" },
	{ "ret", "6" },
	{ "utf-8_strings", u8"Hello 🌎 world" },
	{ "concatenation", "abctrue2false" },
	{ "instance_fields", "null4null" },
	{ "comments", "abc" },
	{ "operator_overloading", "(6, 9)" },
	{ "try_catch", "catchfinally" },
	{ "primes", "2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 " },
	{ "linked_list", "list is empty [1, 2, 3] [3, 2, 1] [] " },
	{ "linked_list_using_closures", "1234" },
	{ "lambdas", "0 1 4 9 16 25 36 49 64 81 100 \n1 1 2 6 24 120 720 5040 40320 362880 3628800 \n" },
	{ "sorting", "0 1 2 3 4 5 6 7 8 9 \n0 1 2 3 4 5 6 7 8 9 \n0 1 2 3 4 5 6 7 8 9 \n0 1 2 3 4 5 6 7 8 9 \n" },
	{ "exception_from_native_function", "10" },
	{ "calling_functions_from_native_functions", "553353" },
	{ "match_class", "XY" },
	{ "list_iterator", "12345" },
	{ "map_iterator", "1 8 27 64 125 " },
	{ "nested_iterators", "(0,0)(0,1)(0,2)(1,0)(1,1)(1,2)(2,0)(2,1)(2,2)" },
	{ "import", "123456123456" },
	{ "import_all", "123456123456" },
};

void testFailed(std::string_view name)
{
	std::cerr << "[FAILED] " << name << '\n';
}

void testFailed(std::string_view name, std::string_view got, std::string_view expected)
{
	std::cerr << "[FAILED] " << name << " | expected \"" << expected << "\" got \"" << got << "\"\n";
}

//#define LOOP_TESTS

int main()
{
	std::stringstream output;
	std::cout.set_rdbuf(output.rdbuf());

#ifdef LOOP_TESTS
	for (;;)
	{
#endif
// TODO: Make a function to register the hash map to the GC.
#ifndef VOXL_DEBUG_STRESS_TEST_GC
	hashMapTests();
#endif 
	std::cout << "Language tests\n";
	Scanner scanner;
	Parser parser;
	Allocator allocator;
	Compiler compiler(allocator);
	auto vm = std::make_unique<Vm>(allocator);

	for (const auto& [name, expectedResult] : tests)
	{
		auto filename = std::string("test/tests/") + std::string(name) + std::string(".voxl");
		auto source = stringFromFile(filename);
		SourceInfo sourceInfo{ filename, std::filesystem::path(filename).parent_path(), source };
		ErrorPrinter errorPrinter(std::cerr, sourceInfo);

		auto scannerResult = scanner.parse(sourceInfo, errorPrinter);
		auto parserResult = parser.parse(scannerResult.tokens, sourceInfo, errorPrinter);

		if (scannerResult.hadError || parserResult.hadError)
		{
			testFailed(name);
			continue;
		}

		auto compilerResult = compiler.compile(parserResult.ast, errorPrinter);
		if (compilerResult.hadError)
		{
			testFailed(name);
			continue;
		}

		output.str(std::string());
		vm->reset();
		auto vmResult = vm->execute(compilerResult.program, compilerResult.module, scanner, parser, compiler, errorPrinter);
		if (vmResult == VmResult::RuntimeError)
		{
			testFailed(name);
			continue;
		}

		auto result = output.str();
		if (result != expectedResult)
		{
			testFailed(name, result, expectedResult);
			continue;
		}

		std::cerr << "[PASSED] " << name << '\n';
	}
#ifdef LOOP_TESTS
	}
#endif
}