#pragma once

#include <Parsing/Token.hpp>
#include <Parsing/SourceInfo.hpp>

// Maybe make every function private and only expose a friend function like std::vector<Token> parseTokens(std::string_view source);
// The problem is that it would need to construct a new object for each parse.
// I could return a std::move(m_tokens);

namespace Lang
{
	class Scanner
	{
	public:
		class Result
		{
		public:
			bool hadError;
			std::vector<Token> tokens;
		};

	public:
		Scanner();

		Result parse(SourceInfo& sourceInfoToComplete);

	private:
		Token token();

		Token number();
		Token keywordOrIdentifier();
		Token string();

		Token makeToken(TokenType type);

		void skipWhitespace();

		//Token errorToken(const char* message);
		void errorAt(const Token& token, const char* format, ...);
		char peek();
		bool isAtEnd();
		void advance();
		void advanceLine();

		// Making my own functions because the c functions from ctype.h use int as input the results are also based on the current C locale.
		// Also negative values might trigger asserts. Negative values happen because of conversion from the default on most compilers
		// signed char. UTF-8 text could trigger the asserts.
		bool isDigit(char c);
		bool isAlpha(char c);
		bool isAlnum(char c);
		bool match(char c);

	private:
		std::vector<Token> m_tokens;

		SourceInfo* m_sourceInfo;

		// It could be faster to just increment a pointer.
		size_t m_currentCharIndex;
		size_t m_tokenStartIndex;

		bool m_hadError;
	};
}