#include <Parsing/Scanner.hpp>
#include <Parsing/Parser.hpp>
#include <ErrorPrinter.hpp>
#include <Compiling/Compiler.hpp>
#include <Vm/Vm.hpp>
#include <iostream>
#include <fstream>

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

Value print2(Value* args, int argCount)
{
	if (argCount != 0)
	{
		std::cout << args[0] << '\n';
	}

	return Value::null();
}

//NativeFunctionResult add(Value* args, int argCount, Vm& vm, Allocator& allocator)

// TODO: Function shouldn't call end scope because it pops all the values off the stack. This is pointless
// because the function return will pop them anyway.
// Could optimize also be creating an instruction to pop n values of the stack.

// TODO: Make static vector class

// How to synchronize this without semicolons
// function(
//		1 +,
//		2
// )

// When storing marking function in the GC store them in a vector instead of a set because iteration is much more common
// than removal

// TODO: Decide if getting the class of an instance should be build in or should it just be a set field.

// For static methods just wirte
// fn test() [}

// Don't know if i shoud use // for comments or for integer division.

// TODO: Add dead code elimintation and warnings. Expr is just a compile time expression. Expr is just a single identifier load. Expr is just a field access (this might not work if 
// I allow to overload it).
// If a lambda is created but nothing is done with it though does technically have side effects.
// If a local variable is created but never used issue a warning.
// Add Unreachable code detection
// when a break, continue or return is compiled set a flag in the scope and each time a new statement is compiled after that issue a warning.
// This only works if there are no gotos.
// If I decide to add gotos.
// When a label is created just clear the flag.
// When a scope is created save the vector of stmts
// If there is a goto check if it skips over any variable declarations by taking the pointer of the currently compiled statement and check all the statements in the range for declarations.
// Don't know if it makes sense to allow backwards gotos or not it may be useful for state machines.
// Backwards gotos would need to pop the values they skip so they don't get redeclared and mess up the stack.

// TODO make generic function for parsing arguments

// Maybe add elif

VOXL_NATIVE_FN(add)
{
	if ((args[0].type != ValueType::Int) || (args[1].type != ValueType::Int))
	{
		return NativeFunctionResult::exception(Value(Int(5)));
	}
	return Value(args[0].as.intNumber + args[1].as.intNumber);
}

int main()
{

	bool shouldCompile = true;

	std::string filename = "../../../src/test2.voxl";
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

	if (compilerResult.hadError == false)
	{
		auto vm = std::make_unique<Vm>(allocator);
		vm->defineNativeFunction("add", add, 2);
		auto result = vm->execute(compilerResult.program, errorPrinter);
	}
}