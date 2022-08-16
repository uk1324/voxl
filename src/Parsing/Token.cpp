#include <Parsing/Token.hpp>

using namespace Voxl;

Token::Token(TokenType type, size_t start, size_t end)
	: type(type)
	, start(start)
	, end(end)
{}

Token::Token(const Token& other)
{
	// Don't know if this is safe and it probably isn't the fastest way because most types don't take up the whole space
	// and also it copies the padding.
	memcpy(this, &other, sizeof(Token));

	switch (other.type)
	{
		case TokenType::StringConstant:
			new (&string.text) std::string(other.string.text);
			break;

		default:
			break;
	}
}

Token::Token(Token&& other) noexcept
{
	memcpy(this, &other, sizeof(Token));

	switch (other.type)
	{
		case TokenType::StringConstant:
			new (&string.text) std::string(std::move(other.string.text));
			break;

		default:
			break;
	}
}

Token::~Token()
{
	switch (type)
	{
		case TokenType::StringConstant:
			string.text.~basic_string();
			break;

		default:
			break;
	}
}

SourceLocation Token::location() const
{
	return SourceLocation(start, end);
}
