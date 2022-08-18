#pragma once

#include <Value.hpp>
#include <Parsing/SourceInfo.hpp>

#include <string_view>

namespace Voxl
{

enum class TokenType
{
	// Operators
	Plus,
	Minus,
	Star,
	Slash,
	SlashSlash,
	Percent,
	PlusEquals,
	MinusEquals,
	StarEquals,
	SlashEquals,
	SlashSlashEquals,
	PercentEquals,
	Not,
	Equals,
	NotEquals,
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
	Arrow,
	ThinArrow,
	At,

	// Keywords
	Class,
	Fn,
	Ret,
	True,
	False,
	Null,
	If,
	Else,
	Elif,
	Loop,
	While,
	For,
	Break,
	Continue,
	Try,
	Catch,
	Throw,
	Finally,
	Impl,
	Match,
	In,
	Use,

	// Numbers
	IntNumber,
	FloatNumber,

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

	SourceLocation location() const;

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