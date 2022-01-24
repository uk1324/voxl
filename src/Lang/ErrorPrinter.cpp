#include <Lang/ErrorPrinter.hpp>
#include <Utils/Assertions.hpp>
#include <Utils/TerminalColors.hpp>

#include <ostream>

using namespace Lang;

ErrorPrinter::ErrorPrinter(std::ostream& out, const SourceInfo& sourceInfo)
	: m_sourceInfo(sourceInfo)
	, m_out(out)
{}

void ErrorPrinter::at(size_t start, size_t end, const char* format, va_list args)
{
	const auto startLine = m_sourceInfo.getLine(start);
	const auto endLine = m_sourceInfo.getLine(end);

	printErrorStart(startLine, start - m_sourceInfo.lineStartOffsets[startLine], format, args);

	for (auto currentLine = startLine; currentLine < endLine; currentLine++)
	{
		auto lineText = trimLine(m_sourceInfo.getLineText(currentLine));
		if (lineText.size() == 0)
		{
			continue;
		}
		printLine(lineText);
	}
}

void ErrorPrinter::printErrorStart(size_t line, size_t charInLine, const char* format, va_list args)
{
	char message[1000];
	const auto bytesWritten = vsnprintf(message, sizeof(message), format, args);
	// Not "<=" because of the terminating null.
	ASSERT(bytesWritten < sizeof(message));

	m_out << m_sourceInfo.filename
		<< ':' << line
		<< ':' << charInLine
		<< ":" TERM_COL_RED " error: " TERM_COL_RESET
		<< TERM_COL_CYAN << message << TERM_COL_RESET << '\n';
}

void ErrorPrinter::printLine(std::string_view line)
{
	m_out << line << '\n';
	m_out << TERM_COL_RED;
	for (size_t i = 0; i < line.size(); i++)
	{
		m_out << '~';
	}
	m_out << TERM_COL_RESET "\n";
}

std::string_view ErrorPrinter::trimLine(std::string_view line)
{
	static const char* WHITESPACE = "\t\n\r\f\v";

	auto prefixOffset = line.find_first_not_of(WHITESPACE);
	if (prefixOffset == std::string_view::npos)
	{
		return line.substr(0, 0);
	}
	line.remove_prefix(prefixOffset);

	auto suffixOffset = line.find_last_not_of(WHITESPACE);
	if (suffixOffset == std::string_view::npos)
	{
		return line.substr(0, 0);
	}
	line.remove_suffix(line.size() - suffixOffset);

	return line;
}

std::ostream& Lang::ErrorPrinter::outStream()
{
	return m_out;
}