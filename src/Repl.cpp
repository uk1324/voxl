#include <Repl.hpp>
#include <Parsing/Parser.hpp>
#include <Parsing/Scanner.hpp>
#include <Compiling/Compiler.hpp>
#include <TerminalErrorReporter.hpp>
#include <Vm/Vm.hpp>
#include <iostream>

using namespace Voxl;

int Voxl::runRepl()
{
	SourceInfo sourceInfo;
	sourceInfo.displayedFilename = "<repl>";
	sourceInfo.directory = std::filesystem::current_path();

	TerminalErrorReporter errorReporter(std::cerr, sourceInfo, 4);
	Allocator allocator;
	Parser parser(true);
	Scanner scanner;
	Compiler compiler(allocator);
	auto vm = std::make_unique<Vm>(allocator);

	std::string source;
	std::string text;

	while (std::cin.good())
	{
		std::cout << ">>> ";
		for (;;)
		{
			std::getline(std::cin , text);
			source += text;
			source += '\n';

			sourceInfo.source = source;
			sourceInfo.lineStartOffsets.clear();

			const auto scannerResult = scanner.parse(sourceInfo, errorReporter);
			if (scannerResult.hadError)
				break;

			const auto parserResult = parser.parse(scannerResult.tokens, sourceInfo, errorReporter);
			if (parserResult.errorAtEof)
			{
				std::cout << "... ";
				continue;
			}
			else if (parserResult.hadError)
			{
				break;
			}

			const auto compilerResult = compiler.compile(parserResult.ast, sourceInfo, errorReporter);

			if (compilerResult.hadError)
				break;

			const auto vmResult = vm->execute(compilerResult.program, compilerResult.module, scanner, parser, compiler, sourceInfo, errorReporter);
			break;
		}
		source.clear();
	}

	return EXIT_SUCCESS;
}
