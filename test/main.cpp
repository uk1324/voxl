#include <../test/tests.hpp>
#include <Parsing/Scanner.hpp>
#include <Parsing/Parser.hpp>
#include <Compiling/Compiler.hpp>
#include <Vm/Vm.hpp>
#include <fstream>
#include <sstream>

using namespace Lang;

std::string stringFromFile(std::string_view path)
{
	std::ifstream file(path.data(), std::ios::binary);

	if (file.fail())
	{
		std::cerr << "couldn't open file \"" << path << "\"\n";
		exit(EXIT_FAILURE);
	}

	auto start = file.tellg();
	file.seekg(0, std::ios::end);
	auto end = file.tellg();
	file.seekg(start);
	auto fileSize = end - start;

	std::string result;
	// Pointless memset 0
	result.resize(fileSize);

	file.read(result.data(), fileSize);
	if (file.fail())
	{
		std::cerr << "couldn't read file \"" << path << "\"\n";
		exit(EXIT_FAILURE);
	}

	return result;
}

std::unordered_map<std::string_view, std::string_view> tests = {
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
	{ "exception_from_native_function", "1null" },
};

void testFailed(std::string_view name)
{
	std::cerr << "[FAILED] " << name << '\n';
}

void testFailed(std::string_view name, std::string_view got, std::string_view expected)
{
	std::cerr << "[FAILED] " << name << " | expected \"" << expected << "\" got \"" << got << "\"\n";
}

int main()
{
	hashMapTests();

	std::cout << "Language tests\n";
	Scanner scanner;
	Parser parser;
	Compiler compiler;
	Allocator allocator;
	auto vm = std::make_unique<Vm>(allocator);

	std::stringstream output;
	std::cout.set_rdbuf(output.rdbuf());

	for (const auto& [name, expectedResult] : tests)
	{
		auto filename = std::string("test/tests/") + std::string(name) + std::string(".voxl");
		auto source = stringFromFile(filename);
		SourceInfo sourceInfo{ filename,  source };
		ErrorPrinter errorPrinter(std::cerr, sourceInfo);

		bool hadParsingError = false;

		auto scannerResult = scanner.parse(sourceInfo, errorPrinter);
		hadParsingError &= scannerResult.hadError;

		auto parserResult = parser.parse(scannerResult.tokens, sourceInfo, errorPrinter);
		hadParsingError &= parserResult.hadError;

		if (hadParsingError)
		{
			testFailed(name);
			continue;
		}

		auto compilerResult = compiler.compile(parserResult.ast, errorPrinter, allocator);
		if (compilerResult.hadError)
		{
			testFailed(name);
			continue;
		}

		output.str(std::string());
		vm->reset();
		auto vmResult = vm->execute(compilerResult.program, errorPrinter);
		if (vmResult == Vm::Result::RuntimeError)
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
}