#include <Parsing/Scanner.hpp>
#include <Parsing/Parser.hpp>
#include <ErrorPrinter.hpp>
#include <Compiling/Compiler.hpp>
#include <Debug/Disassembler.hpp>
#include <Vm/Vm.hpp>

using namespace Lang;

#include <iostream>
#include <fstream>

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

	// Pointless memset
	std::string result;

	result.resize(fileSize);

	file.read(result.data(), fileSize);
	if (file.fail())
	{
		std::cerr << "couldn't read file \"" << path << "\"\n";
		exit(EXIT_FAILURE);
	}

	return result;
}

// If I wanted to I could implement things like scopes and lineNumberStack as a linked list on the call stack though I don't see what
// would be the point.

// TODO Make a test for accessing a variable with the same name from outer in initializer.

#include <filesystem>

Value print2(Value* args, int argCount)
{
	if (argCount != 0)
	{
		std::cout << args[0] << '\n';
	}

	return Value::null();
}

Value add(Value* args, int argCount)
{
	if ((argCount < 2) || (args[0].type != ValueType::Int) || (args[1].type != ValueType::Int))
	{
		return Value::integer(5);
	}
	return Value(args[0].as.intNumber + args[1].as.intNumber);
}

int main()
{
	bool shouldCompile = true;

	std::string filename = "../../../src/test.voxl";
	std::string source = stringFromFile(filename);
	SourceInfo sourceInfo;
	sourceInfo.source = source;
	sourceInfo.filename = filename;
	
	ErrorPrinter errorPrinter(std::cerr, sourceInfo);

	Scanner scanner;
	auto scannerResult = scanner.parse(sourceInfo, errorPrinter);
	shouldCompile &= !scannerResult.hadError;

	Parser parser;
	auto parserResult = parser.parse(scannerResult.tokens, sourceInfo, errorPrinter);
	shouldCompile &= !parserResult.hadError;

	if (shouldCompile == false)
	{
		return EXIT_FAILURE;
	}

	Allocator allocator;

	Compiler compiler;
	auto compilerResult = compiler.compile(parserResult.ast, errorPrinter, allocator);

	std::cout << "----<script>\n";
	disassembleByteCode(compilerResult.program->byteCode);

	if (compilerResult.hadError == false)
	{
		auto vm = std::make_unique<Vm>(allocator);
		vm->createForeignFunction("print2", print2);
		vm->createForeignFunction("add", add);
		auto result = vm->execute(compilerResult.program, errorPrinter);
	}
}