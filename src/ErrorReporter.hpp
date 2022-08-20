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
	// TODO: It might be better to provide a iterator that would allow accessing the stack trace and provide so implementation independent information
	// The iterator would store a Span and an index to the CallFrames.
	virtual void onVmError(const Vm& vm, std::string_view message) = 0;
	virtual void onUncaughtException(const Vm& vm, std::optional<std::string_view> exceptionTypeName, std::optional<std::string_view> message) = 0;
};

}