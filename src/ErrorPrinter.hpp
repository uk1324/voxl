#pragma once

#include <Parsing/SourceInfo.hpp>
#include <Parsing/Token.hpp>
#include <Ast.hpp>

#include <stdarg.h>

namespace Lang
{
	// This can't be const because the function modify m_out.
	class ErrorPrinter
	{
	public:
		ErrorPrinter(std::ostream& out, const SourceInfo& sourceInfo);

		void at(size_t start, size_t end, const char* format, va_list args);
		// Doesn't print the lines that the error occured on.
		void printErrorStart(size_t line, size_t charInLine, const char* format, va_list args);
		void printLine(std::string_view line);

	public:
		static std::string_view trimLine(std::string_view line);

		std::ostream& outStream();

	private:
		const SourceInfo& m_sourceInfo;
		std::ostream& m_out;
	};
}