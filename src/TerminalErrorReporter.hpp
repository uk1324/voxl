#pragma once

#include <ErrorReporter.hpp>
#include <Parsing/SourceInfo.hpp>

namespace Voxl
{

class TerminalErrorReporter final : public ErrorReporter
{
public:
	TerminalErrorReporter(std::ostream& errorOut, const SourceInfo& sourceInfo, size_t tabWidth);

	void onScannerError(const SourceLocation& location, std::string_view message) override;
	void onParserError(const Token& token, std::string_view message) override;
	void onCompilerError(const SourceLocation& location, std::string_view message) override;
	void onVmError(const Vm& vm, std::string_view message) override;
	void onUncaughtException(const Vm& vm, std::optional<std::string_view> exceptionTypeName, std::optional<std::string_view> message) override;

private:
	void errorAt(const SourceLocation& location, std::string_view message);
	void printErrorStart(size_t line, size_t offsetInLine, std::string_view message);
	void printRedTildes(size_t count);
	void printStackTrace(const Vm& vm);

	static bool isVisible(char c);
	static std::string_view trimLine(std::string_view line);

private:
	const SourceInfo& m_sourceInfo;
	const size_t m_tabWidth;
	std::ostream& m_out;
};

}