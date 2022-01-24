#include <Lang/Parsing/Scanner.hpp>
#include <Lang/Parsing/Parser.hpp>
#include <Lang/TypeChecking/TypeChecker.hpp>
#include <Lang/Compiling/Compiler.hpp>
#include <Lang/Vm/Vm.hpp>
#include <Lang/Debug/AstJsonifier.hpp>

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

	std::string filename = "src/Lang/test.voxl";
	std::string source = stringFromFile(filename);
	SourceInfo sourceInfo;
	sourceInfo.source = source;
	sourceInfo.filename = filename;

	ErrorPrinter errorPrinter(std::cerr, sourceInfo);

	Scanner scanner;
	auto scannerResult = scanner.parse(sourceInfo, errorPrinter);
	shouldCompile &= !scannerResult.hadError;

	Parser parser;
	auto parserResult = parser.parse(scannerResult.tokens, errorPrinter, sourceInfo);
	shouldCompile &= !parserResult.hadError;
	
	TypeChecker typeChecker;
	auto typeCheckerResult = typeChecker.checkAndInferTypes(parserResult.ast, errorPrinter);
	shouldCompile &= !typeCheckerResult.hadError;

	if (shouldCompile == false)
	{
		return EXIT_FAILURE;
	}

	AstJsonifier json;
	std::cout << json.jsonify(parserResult.ast);

	//Compiler compiler;
	//auto program = compiler.compile(ast);
	//Vm vm;
	//vm.run(program);
	
}