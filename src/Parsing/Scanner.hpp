#pragma once

#include <Parsing/Token.hpp>
#include <Parsing/SourceInfo.hpp>
#include <ErrorReporter.hpp>

namespace Voxl
{

// TODO:
// Parsing can be made faster by making Parser own a Scanner. Instead of generating all the tokens at once Parser
// would call a function nextToken on the scanner. The parser would keep the number of lookahead tokens it needs.
// Another optimization would be to make reading the file faster. This could be done by not loading the whole file 
// into memory at once. Linux provides mmap, Windows has CreateFileMapping and MapViewOfFile. When a file is opened with
// those functions the operating system loads the parts of the file into memory only when they are accessed and unloads
// the unused ones.

class Scanner
{
public:
	class Result
	{
	public:
		bool hadError;
		std::vector<Token>& tokens;
	};

public:
	Scanner();

	Result parse(SourceInfo& sourceInfoToComplete, ErrorReporter& errorPrinter);

private:
	Token token();

	Token number();
	Token keywordOrIdentifier();
	Token string();

	Token makeToken(TokenType type);

	void skipWhitespace();

	[[nodiscard]] Token errorToken(const char* format, ...);
	[[nodiscard]] Token errorTokenAt(size_t start, size_t end, const char* format, ...);
	void errorAt(size_t start, size_t end, const char* format, va_list args);
	void errorAt(size_t start, size_t end, const char* format, ...);
	char peek();
	char peekPrevious();
	char peekNext();
	bool isAtEnd();
	void advance();
	void advanceLine();
	bool match(char c);

	// Making my own functions because the c functions from ctype.h use int as input the results are also based on the current C locale.
	// Negative values might trigger asserts. Negative values happen because of conversion from the default on most compilers
	// signed char. UTF-8 text could trigger the asserts.
	static bool isDigit(char c);
	static bool isIdentifierStartChar(char c);
	static bool isIdentifierChar(char c);

private:
	std::vector<Token> m_tokens;

	SourceInfo* m_sourceInfo;

	// It could be faster to just increment a pointer.
	// But then I would have to perform arithmetic on every token creation to get the offset and length.
	size_t m_currentCharIndex;
	size_t m_tokenStartIndex;

	ErrorReporter* m_errorReporter;

	bool m_hadError;
};

}