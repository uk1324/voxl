#include <Parsing/Scanner.hpp>
//#include <Parsing/Parser.hpp>
//#include <Compiling/Compiler.hpp>
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

int main()
{
	bool shouldCompile = true;

	std::string filename = "src/test.voxl";
	std::string source = stringFromFile(filename);
	SourceInfo sourceInfo;
	sourceInfo.source = source;
	sourceInfo.filename = filename;

	Scanner scanner;
	auto scannerResult = scanner.parse(sourceInfo);
	
	//shouldCompile &= !scannerResult.hadError;

	//Parser parser;
	//auto parserResult = parser.parse(scannerResult.tokens, errorPrinter, sourceInfo);
	//shouldCompile &= !parserResult.hadError;

	//if (shouldCompile == false)
	//{
	//	return EXIT_FAILURE;
	//}

	return 0;

	//Allocator allocator;

	//Compiler compiler;
	//auto compilerResult = compiler.compile(parserResult.ast, errorPrinter, allocator);
	//if (compilerResult.hadError == false)
	//{
	//	Vm vm;
	//	vm.run(compilerResult.program);
	//}
	
}