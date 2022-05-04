#pragma once

#include <string_view>
#include <Value.hpp>

namespace Lang
{

enum class TokenType
{
	// Numbers
	IntNumber,
	FloatNumber,

	// Operators
	Plus,
	Minus,
	Star,
	Slash,
	Percent,
	Not,
	Equals,
	EqualsEquals,
	Less,
	LessEquals,
	More,
	MoreEquals,
	And,
	AndAnd,
	Or,
	OrOr,
	PlusPlus,
	Dot,

	// Symbols
	Semicolon,
	Colon,
	LeftParen,
	RightParen,
	LeftBrace,
	RightBrace,
	Comma,
	LeftBracket,
	RightBracket,

	// Keywords
	Print,
	Let,
	Class,
	Fn,
	Ret,
	True,
	False,
	Null,
	If,
	Else,
	Loop,
	While,
	For,
	Break,
	Continue,
	Try,
	Catch,
	Throw,
	Finally,

	// Other
	Identifier,
	StringConstant,

	// Special
	Error,
	Eof
};

struct Token
{
public:
	Token(TokenType type, size_t start, size_t end);
	Token(const Token& other);
	Token(Token&& other) noexcept;
	~Token();

public:
	TokenType type;

	size_t start;
	size_t end;

	union
	{
		std::string_view identifier;
		Int intValue;
		Float floatValue;
		struct
		{
			std::string text;
			// Number of UTF-8 chars
			size_t length;
		} string;
	};
};

}