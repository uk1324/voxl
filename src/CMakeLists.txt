add_library(
	voxl-lib 
	"ByteCode.hpp" "Debug/Disassembler.hpp" "Debug/Disassembler.cpp" "Value.hpp" "Value.cpp" "Parsing/Scanner.cpp" "Parsing/Scanner.hpp" "Parsing/Token.hpp" "Parsing/Token.cpp" "Compiling/Compiler.hpp" "Compiling/Compiler.cpp" "Parsing/Parser.cpp" "Parsing/Parser.hpp" "Parsing/SourceInfo.hpp" "Parsing/SourceInfo.cpp" "Repl.hpp" "Vm/Vm.hpp" "Vm/Vm.cpp" "Allocator.hpp" "Allocator.cpp" "Ast.hpp" "Ast.cpp" "ErrorPrinter.hpp" "ErrorPrinter.cpp" "Asserts.hpp" "Utf8.hpp" "Utf8.cpp" "KeyTraits.cpp" "KeyTraits.hpp" "Vm/List.hpp" "Vm/List.cpp")

if(MSVC)
	target_compile_options(voxl-lib PRIVATE /W4 /w44062 #[[Non exhaustive switch without a deafult]])
#	target_compile_options(voxl-lib PRIVATE /W4 /WX)
else()
	target_compile_options(voxl-lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()