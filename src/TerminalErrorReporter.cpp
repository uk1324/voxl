#include <TerminalErrorReporter.hpp>
#include <Vm/Vm.hpp>

#define TERM_COL_RED "\x1B[31m"
#define TERM_COL_GREEN "\x1B[32m"
#define TERM_COL_YELLOW "\x1B[33m"
#define TERM_COL_BLUE "\x1B[34m"
#define TERM_COL_MAGENTA "\x1B[35m"
#define TERM_COL_CYAN "\x1B[36m"
#define TERM_COL_WHITE "\x1B[37m"
#define TERM_COL_RESET "\x1B[0m"

using namespace Voxl;

TerminalErrorReporter::TerminalErrorReporter(std::ostream& errorOut, const SourceInfo& sourceInfo, size_t tabWidth)
	: m_out(errorOut)
	, m_sourceInfo(sourceInfo)
	, m_tabWidth(tabWidth)
{}

void TerminalErrorReporter::onScannerError(const SourceLocation& location, std::string_view message)
{
	const auto startLine = m_sourceInfo.getLine(location.start);
	const auto offsetInLine = location.start - m_sourceInfo.lineStartOffsets[startLine];
	printErrorStart(startLine, offsetInLine, message);

	const auto endLine = m_sourceInfo.getLine(location.end);
	// This has to be calculated because the whole file hasn't been parsed yet.
	// It might be easier if the onScannerError calls were made after finishing scanning. Then all the different errors could
	// be handled by just errorAt().
	auto endLineEnd = m_sourceInfo.lineStartOffsets[endLine];
	for (; (endLineEnd < m_sourceInfo.source.size()) && (m_sourceInfo.source[endLineEnd] != '\n'); endLineEnd++)
		;

	const auto startLineStart = m_sourceInfo.lineStartOffsets[startLine];
	auto current = startLineStart;
	while (current < endLineEnd)
	{
		const auto lineStart = current;
		for (; (current < m_sourceInfo.source.size()) && (m_sourceInfo.source[current] != '\n'); current++)
		{
			m_out << m_sourceInfo.source[current];
		}
		const auto lineEnd = current;
		m_out << '\n';
		// TODO: This doesn't take if there are UTF-8 characters into account.
		// To do this there would need to be a function to check the width of the character. 
		// A loop would accumulate the widths and print the correct amount of characters.
		// For the windows console you could print the character and check how much the cursor moved.
		for (auto i = lineStart; (i < lineEnd) && (i < location.start); i++)
		{
			if (m_sourceInfo.source[i] == '\t')
			{
				for (size_t _ = 0; _ < m_tabWidth; _++)
					m_out << ' ';
			}
			else
			{
				m_out << ' ';
			}
		}
		m_out << TERM_COL_RED;
		for (auto i = location.start; (i < lineEnd) && (i < location.end); i++)
		{
			if (m_sourceInfo.source[i] == '\t')
			{
				for (size_t _ = 0; _ < m_tabWidth; _++)
					m_out << '~';
			}
			else
			{
				m_out << '~';
			}
		}
		m_out << TERM_COL_RESET "\n";
	}
}

void TerminalErrorReporter::onParserError(const Token& token, std::string_view message)
{
	errorAt(token.location(), message);
}

void TerminalErrorReporter::onCompilerError(const SourceLocation& location, std::string_view message)
{
	errorAt(location, message);
}

void TerminalErrorReporter::onVmError(const Vm& vm, std::string_view message)
{
	m_out << "fatal runtime error: " << message << '\n';
	for (auto frame = vm.m_callStack.crbegin(); frame != vm.m_callStack.crend(); ++frame)
	{
		// TODO: Also print native functions. Currently calling a native function doesn't store it in the call frame.
		const auto callable = frame->callable;
		if (callable == nullptr)
			continue;

		if (callable->isFunction())
		{
			const auto function = callable->asFunction();
			const auto instructionOffset = frame->instructionPointerBeforeCall - function->byteCode.code.data();
			const auto lineNumber = function->byteCode.lineNumberAtOffset[instructionOffset] + 1;
			m_out << "line " << lineNumber << " in " << function->name->chars << "()\n";
		}
		else if (callable->isNativeFunction())
		{
			const auto function = callable->asNativeFunction();
			m_out << "in native " << function->name->chars << "()\n";
		}
		else
		{
			ASSERT_NOT_REACHED();
		}
	}
}

void TerminalErrorReporter::onUncaughtException(const Vm&, const Value&, std::string_view)
{
	ASSERT_NOT_REACHED();
}

void TerminalErrorReporter::errorAt(const SourceLocation& location, std::string_view message)
{
	auto start = location.start;
	auto end = location.end;
	const auto startLine = m_sourceInfo.getLine(start);
	const auto endLine = m_sourceInfo.getLine(end);

	printErrorStart(startLine, start - m_sourceInfo.lineStartOffsets[startLine], message);

	for (auto currentLine = startLine; currentLine <= endLine; currentLine++)
	{
		auto lineText = m_sourceInfo.getLineText(currentLine);
		if (trimLine(lineText).size() == 0)
		{
			continue;
		}
		m_out << lineText;
		if (lineText.back() != '\n')
		{
			m_out << '\n';
		}
		auto current = m_sourceInfo.lineStartOffsets[currentLine];
		m_out << TERM_COL_RED;
		// TODO: Find a better way to handle tabs and also other invisible characters.
		for (size_t i = current; i < current + lineText.size(); i++)
		{
			if ((i >= start) && (i < end))
			{
				if (m_sourceInfo.source[i] == '\t')
				{
					for (int _ = 0; _ < 4; _++)
					{
						m_out << '~';
					}
				}
				else
				{
					m_out << '~';
				}
			}
			else
			{
				m_out << ((m_sourceInfo.source[i] == '\t') ? '\t' : ' ');
			}
		}
		m_out << TERM_COL_RESET "\n";
	}
}

void TerminalErrorReporter::printErrorStart(size_t line, size_t offsetInLine, std::string_view message)
{
	m_out << m_sourceInfo.displayedFilename
		<< ':' << line + 1
		<< ':' << offsetInLine + 1
		<< ":" TERM_COL_RED " error: " TERM_COL_RESET
		<< TERM_COL_CYAN << message << TERM_COL_RESET << '\n';
}

void TerminalErrorReporter::printRedTildes(size_t count)
{
	m_out << TERM_COL_RED;
	for (size_t _ = 0; _ < count; _++)
	{
		m_out << '~';
	}
	m_out << TERM_COL_RESET "\n";
}

bool TerminalErrorReporter::isVisible(char c)
{
	return (c >= 0) && (c <= 31);
}

std::string_view TerminalErrorReporter::trimLine(std::string_view line)
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