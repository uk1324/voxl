#pragma once

#include <Ast.hpp>
#include <Span.hpp>

namespace Voxl
{

class Vm;

// It would be better to make a sum type that contains all the errors and pass it into the function.
class ErrorReporter
{
public:
	virtual ~ErrorReporter() = default;
	virtual void onScannerError(const SourceLocation& location, std::string_view message) = 0;
	virtual void onParserError(const Token& token, std::string_view message) = 0;
	virtual void onCompilerError(const SourceLocation& location, std::string_view message) = 0;
	virtual void onVmError(const Vm& vm, std::string_view message) = 0;
	virtual void onUncaughtException(const Vm& vm, const Value& value, std::string_view message) = 0;
};

}