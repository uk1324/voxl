#pragma once

#include <Lang/Parsing/SourceInfo.hpp>
#include <Lang/Parsing/Token.hpp>
#include <Lang/Ast/Expr.hpp>
#include <Lang/Ast/Stmt.hpp>

#include <stdarg.h>

namespace Lang
{
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