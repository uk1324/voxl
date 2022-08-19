add_library(
	voxl-lib 
	"ByteCode.hpp" "ByteCode.cpp" "Debug/Disassembler.hpp" "Debug/Disassembler.cpp" "Value.hpp" "Value.cpp" "Parsing/Scanner.cpp" "Parsing/Scanner.hpp" "Parsing/Token.hpp" "Parsing/Token.cpp" "Compiling/Compiler.hpp" "Compiling/Compiler.cpp" "Parsing/Parser.cpp" "Parsing/Parser.hpp" "Parsing/SourceInfo.hpp" "Parsing/SourceInfo.cpp" "Vm/Vm.hpp" "Vm/Vm.cpp" "Allocator.hpp" "Allocator.cpp" "Ast.hpp" "Ast.cpp" "Asserts.hpp" "Utf8.hpp" "Utf8.cpp" "Vm/List.hpp" "Vm/List.cpp" "Repl.hpp" "Repl.cpp" "Context.hpp" "Context.cpp" "HashTable.hpp" "HashTable.cpp" "ReadFile.hpp" "ReadFile.cpp" "TestModule.hpp" "TestModule.cpp" "ErrorReporter.hpp" "TerminalErrorReporter.hpp" "TerminalErrorReporter.cpp" "Format.hpp" "Format.cpp" "Span.hpp" "Vm/String.hpp" "Vm/String.cpp" "Vm/Number.hpp" "Vm/Number.cpp" "Vm/Dict.hpp" "Vm/Dict.cpp" "Vm/Errors.cpp" "Vm/Errors.hpp")

if(MSVC)
	target_compile_options(voxl-lib PRIVATE /W4 /w44062 #[[Non exhaustive switch without a deafult]])
#	target_compile_options(voxl-lib PRIVATE /W4 /WX)
else()
	target_compile_options(voxl-lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()