#include <Lang/Parsing/Token.hpp>

using namespace Lang;

Token::Token(TokenType type, std::string_view text, size_t start, size_t end)
	: type(type)
	, text(text)
	, start(start)
	, end(end)
{}

bool Token::operator== (const Token& token) const
{
	return token.text == text;
}

bool Token::operator!= (const Token& token) const
{
	return token.text != text;
}