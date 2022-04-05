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
	Equals,

	// Symbols
	Semicolon,
	Colon,
	LeftParen,
	RightParen,
	LeftBrace,
	RightBrace,
	Comma,

	// Keywords
	Print,
	Let,
	Fn,
	Ret,

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
	Token(TokenType type, size_t start, size_t end);

	//bool operator== (const Token& token) const;
	//bool operator!= (const Token& token) const;

public:
	TokenType type;

	size_t start;
	size_t end;

	union
	{
		std::string_view identifier;
		Int intValue;
		Float floatValue;
	};
};

}