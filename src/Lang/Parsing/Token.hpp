#pragma once

#include <string_view>

namespace Lang
{
	enum class TokenType
	{
		// Numbers
		IntNumber,
		FloatNumber,

		// Operators
		Plus,
		Equals,

		// Symbols
		Semicolon,
		Colon,
		LeftParen,
		RightParen,

		// Keywords
		Print,
		Let,
		Int,
		Float,

		// Other
		Identifier,
		String,

		// Special
		Error,
		Eof
	};

	struct Token
	{
	public:
		Token(TokenType type, std::string_view text, size_t start, size_t end);

		bool operator== (const Token& token) const;
		bool operator!= (const Token& token) const;

	public:
		// TODO: Maybe make both const
		TokenType type;
		// This stores basically the same information twice text and start and end in source beacuse I don't want
		// to use source information in the parser outside of error handling but this like IntConstantExpr need to parse the number.
		std::string_view text;
		size_t start;
		size_t end;
	};
}

namespace std {

	template <>
	struct hash<Lang::Token>
	{
		std::size_t operator()(const Lang::Token& token) const
		{
			return std::hash<std::string_view>()(token.text);
		}
	};
}