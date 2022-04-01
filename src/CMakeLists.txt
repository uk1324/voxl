add_library(
	voxl-lib 
	"ByteCode.cpp" "ByteCode.hpp" "Debug/Disassembler.hpp" "Debug/Disassembler.cpp" "Value.hpp" "Value.cpp" "Parsing/Scanner.cpp" "Parsing/Scanner.hpp" "Parsing/Token.hpp" "Parsing/Token.cpp" "Compiling/Compiler.hpp" "Compiling/Compiler.cpp" "Parsing/Parser.cpp" "Parsing/Parser.hpp" "Parsing/SourceInfo.hpp" "Parsing/SourceInfo.cpp" "Repl.hpp" "Vm/Vm.hpp" "Vm/Vm.cpp"   "Obj.hpp" "Allocator.hpp" "Allocator.cpp" "Ast.hpp" "Ast.cpp" "ErrorPrinter.hpp" "ErrorPrinter.cpp" "Asserts.hpp")
