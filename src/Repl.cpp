#include <Repl.hpp>
#include <Parsing/Parser.hpp>
#include <Parsing/Scanner.hpp>
#include <Compiling/Compiler.hpp>
#include <Vm/Vm.hpp>

using namespace Lang;

int Lang::runRepl()
{
	SourceInfo sourceInfo;
	sourceInfo.filename = "<repl>";

	ErrorPrinter errorPrinter(std::cerr, sourceInfo);
	Allocator allocator;
	Parser parser(true);
	Scanner scanner;
	Compiler compiler;
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

			const auto scannerResult = scanner.parse(sourceInfo, errorPrinter);
			if (scannerResult.hadError)
				break;

			const auto parserResult = parser.parse(scannerResult.tokens, sourceInfo, errorPrinter);
			if (parserResult.errorAtEof)
			{
				std::cout << "... ";
				continue;
			}
			else if (parserResult.hadError)
			{
				break;
			}

			const auto compilerResult = compiler.compile(parserResult.ast, errorPrinter, allocator);

			if (compilerResult.hadError)
				break;

			const auto vmResult = vm->execute(compilerResult.program, errorPrinter);
			break;
		}
		source.clear();
	}

	return EXIT_SUCCESS;
}
