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

#include <HashMap.hpp>

struct ObjStringKeyTraits
{
	static bool compareKeys(const ObjString* a, const ObjString* b)
	{
		return (a->size == b->size) && (memcmp(a->chars, b->chars, a->size) == 0);
	}

	static size_t hashKey(const ObjString* key)
	{
		return std::hash<std::string_view>()(std::string_view(key->chars, key->size));
	}

	static void setKeyNull(ObjString*& key)
	{
		key = nullptr;
	}

	static void setKeyTombstone(ObjString*& key)
	{
		key = reinterpret_cast<ObjString*>(1);
	}

	static bool isKeyNull(const ObjString* key)
	{
		return key == nullptr;
	}

	static bool isKeyTombstone(const ObjString* key)
	{
		return key == reinterpret_cast<ObjString*>(1);
	}
};

int main()
{
#define put(k, v) map.insert(a, a.allocateString(k), v);
#define get(k) (map.get(a.allocateString(k)));
#define del(k) (map.remove(a.allocateString(k)));

	//Allocator a;

	//HashMap<ObjString*, int, ObjStringKeyTraits> map;
	//map.init(map, a);

	//for (size_t i = 0; i < 10; i++)
	//{
	//	char c[] = { 'a' + i, '\0' };
	//	put(c, i);
	//}
	//map.print();
	//
	//for (size_t i = 0; i < 10; i += 2)
	//{
	//	char c[] = { 'a' + i, '\0' };
	//	del(c);
	//}

	//std::cout << '\n';
	//map.print();

	//return 0;

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
		//vm->createForeignFunction("add", add);
		auto result = vm->execute(compilerResult.program, errorPrinter);
	}
}