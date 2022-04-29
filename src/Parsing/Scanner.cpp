#include <Parsing/Scanner.hpp>

#include <unordered_map>
#include <iostream>
#include <stdarg.h>

using namespace Lang;

Scanner::Scanner()
	: m_currentCharIndex(0)
	, m_tokenStartIndex(0)
	, m_sourceInfo(nullptr)
	, m_hadError(false)
	, m_errorPrinter(nullptr)
{}

Scanner::Result Scanner::parse(SourceInfo& sourceInfoToComplete, ErrorPrinter& errorPrinter)
{
	m_errorPrinter = &errorPrinter;
	m_sourceInfo = &sourceInfoToComplete;
	m_sourceInfo->lineStartOffsets.push_back(0);
	m_currentCharIndex = 0;
	m_tokenStartIndex = 0;
	m_hadError = false;
	m_tokens.clear();

	while (isAtEnd() == false)
	{
		skipWhitespace();
		if (isAtEnd())
			break;
		m_tokens.push_back(token());
	}

	m_tokens.push_back(Token(TokenType::Eof, m_currentCharIndex, m_currentCharIndex));
	
	// TODO: Could return just a reference to the tokens so the scanner can reuse the memory instead of allocating new every time.
	return Result{ m_hadError, std::move(m_tokens) };
}

Token Scanner::token()
{
	char c = peek();
	advance();

	switch (c)
	{
		case '+':
		{
			if (match('+'))
			{
				return makeToken(TokenType::PlusPlus);
			}
			else
			{
				return makeToken(TokenType::Plus);
			}
		}
		case '&': return match('&')
			? makeToken(TokenType::AndAnd)
			: makeToken(TokenType::And);
		case '|': return match('|')
			? makeToken(TokenType::OrOr)
			: makeToken(TokenType::Or);
		case '-': return makeToken(TokenType::Minus);
		case '!': return makeToken(TokenType::Not);
		case ';': return makeToken(TokenType::Semicolon);
		case '=': return match('=')
			? makeToken(TokenType::EqualsEquals)
			: makeToken(TokenType::Equals);
		case '(': return makeToken(TokenType::LeftParen);
		case ')': return makeToken(TokenType::RightParen);
		case '{': return makeToken(TokenType::LeftBrace);
		case '}': return makeToken(TokenType::RightBrace);
		case '[': return makeToken(TokenType::LeftBracket);
		case ']': return makeToken(TokenType::RightBracket);
		case ':': return makeToken(TokenType::Colon);
		case ',': return makeToken(TokenType::Comma);
		case '.': return makeToken(TokenType::Dot);
		case '"': return string();

		default:
			if (isDigit(c))
				return number();
			if (isIdentifierStartChar(c))
				return keywordOrIdentifier();

			return errorToken("illegal character");
	}
}

Token Scanner::number()
{
	Int number = static_cast<Int>(peekPrevious()) - static_cast<Int>('0');

	while ((isAtEnd() == false) && isDigit(peek()))
	{
		number *= 10;
		number += static_cast<Int>(peek()) - static_cast<Int>('0');
		advance();
	}

	auto token = makeToken(TokenType::IntNumber);
	token.intValue = number;
	return token;
}

Token Scanner::keywordOrIdentifier()
{
	static const std::unordered_map<std::string_view, TokenType> keywords = {
		{ "print", TokenType::Print },
		{ "let", TokenType::Let },
		{ "fn", TokenType::Fn },
		{ "ret", TokenType::Ret },
		{ "true", TokenType::True },
		{ "false", TokenType::False },
		{ "null", TokenType::Null },
		{ "if", TokenType::If },
		{ "else", TokenType::Else },
		{ "loop", TokenType::Loop },
		{ "while", TokenType::While },
		{ "for", TokenType::For },
		// Could rename break to stop and continue to next.
		{ "break", TokenType::Break },
		{ "continue", TokenType::Continue },
		{ "class", TokenType::Class },
	};

	while (isIdentifierChar(peek()))
		advance();

	auto identifier = m_sourceInfo->source.substr(m_tokenStartIndex, m_currentCharIndex - m_tokenStartIndex);
	auto token = makeToken(TokenType::Identifier);

	auto keyword = keywords.find(identifier);

	if (keyword != keywords.end())
		token.type = keyword->second;
	else
		token.identifier = identifier;

	return token;
}

Token Scanner::string()
{
	std::string result;
	size_t length = 0;

	// Could try to synchronize to the next valid UTF-8 character.
	auto synrchronize = [this]
	{
		// This doesn't handle unterminated strings.
		while ((isAtEnd() == false) && (peek() != '"'))
			advance();
		advance();
	};

	while (isAtEnd() == false)
	{
		if (match('"'))
		{
			// Because makeToken returns a Token it calls the copy or move constructor. 
			// When the copy constructor is called with a value of TokenType::StringConstant
			// it tries to create a copy of the other string but that string is invalid because it hasn't been intialized by 
			// makeToken.
			auto token = makeToken(TokenType::Error);
			token.type = TokenType::StringConstant;
			new (&token.string.text) std::string(std::move(result));
			return token;
		}
		
		if (match('\\'))
		{
			// Probably shouldn't allow escaping invalid UTF-8 characters beacuse then calculating length wouldn't work.
			if (match('\\'))
			{
				result += '\\';
			}
			else if (match('\"'))
			{
				result += '"';
			}
			else if (match('n'))
			{
				result += '\n';
			}
			else if (match('t'))
			{
				result += '\t';
			}
			length++;
		}
		else
		{
			char c = peek();
			advance();

			if (c == '\n')
			{
				advanceLine();
			}

			result += c;

			size_t octets;
			uint32_t codePoint;
			if ((c & 0b1111'1000) == 0b1111'0000)
			{
				codePoint = c & 0b0000'0111;
				octets = 3;
			}
			else if ((c & 0b1111'0000) == 0b1110'0000)
			{
				codePoint = c & 0b0000'1111;
				octets = 2;
			}
			else if ((c & 0b1110'0000) == 0b1100'0000)
			{
				codePoint = c & 0b0001'1111;
				octets = 1;
			}
			else if ((c & 0b1000'0000) == 0b0000'0000)
			{
				codePoint = c;
				octets = 0;
			}
			else
			{
				const auto charStart = m_currentCharIndex - 1;
				synrchronize();
				return errorTokenAt(charStart, charStart + 1, "illegal character");
			}

			for (size_t i = 0; i < octets; i++)
			{
				if ((peek() & 0b1100'0000) != 0b1000'0000)
				{
					size_t charStart = m_currentCharIndex - i - 1;
					synrchronize();
					return errorTokenAt(charStart, charStart + 1 + i, "illegal character");
				}
				codePoint <<= 6;
				codePoint |= peek() & 0b0011'1111;
				result += peek();
				advance();
			}

			if ((codePoint >= 0xD800) && (codePoint <= 0xDFFF))
			{
				size_t charStart = m_currentCharIndex - octets - 1;
				synrchronize();
				return errorTokenAt(charStart, charStart + 1 + octets, "illegal character");
			}

			length++;
		}
	}

	const auto startingQuoteLocation = m_tokenStartIndex;
	return errorTokenAt(startingQuoteLocation, startingQuoteLocation + 1, "unterminated string");
}

Token Scanner::makeToken(TokenType type)
{
	Token token(type, m_tokenStartIndex, m_currentCharIndex);
	m_tokenStartIndex = m_currentCharIndex;
	return token;
}

void Scanner::skipWhitespace()
{
	while (isAtEnd() == false)
	{
		char c = peek();

		switch (c)
		{
			case ' ':
			case '\t':
			case '\r':
			case '\f':
				advance();
				break;

			case '\n':
				advance();
				advanceLine();
				break;

			default:
				m_tokenStartIndex = m_currentCharIndex;
				return;
		}
	}
}

Token Scanner::errorToken(const char* format, ...)
{
	const auto token = makeToken(TokenType::Error);
	va_list args;
	va_start(args, format);
	errorAt(token.start, token.end, format, args);
	va_end(args);
	return token;
}

Token Scanner::errorTokenAt(size_t start, size_t end, const char* format, ...)
{
	const auto token = makeToken(TokenType::Error);
	va_list args;
	va_start(args, format);
	errorAt(start, end, format, args);
	va_end(args);
	return token;
}

void Scanner::errorAt(size_t start, size_t end, const char* format, va_list args)
{
	m_hadError = true;

	const auto startLine = m_sourceInfo->getLine(start);
	m_errorPrinter->printErrorStart(startLine, m_currentCharIndex - m_sourceInfo->lineStartOffsets.back(), format, args);

	const auto endLine = m_sourceInfo->getLine(end);
	auto endLineEnd = m_sourceInfo->lineStartOffsets[endLine];
	for (; (endLineEnd < m_sourceInfo->source.size()) && (m_sourceInfo->source[endLineEnd] != '\n'); endLineEnd++)
		;

	const auto startLineStart = m_sourceInfo->lineStartOffsets[startLine];
	auto current = startLineStart;
	while (current < endLineEnd)
	{
		while ((current < m_sourceInfo->source.size()) && (m_sourceInfo->source[current] != '\n'))
		{
			m_errorPrinter->outStream() << m_sourceInfo->source[current];
			current++;
		}
		m_errorPrinter->outStream() << '\n';
		for (size_t i = 0; i < (start - startLineStart); i++)
		{
			m_errorPrinter->outStream() << ' ';
		}
		m_errorPrinter->printRedTildes(end - start);
	}
}

char Scanner::peek()
{
	return m_sourceInfo->source[m_currentCharIndex];
}

char Scanner::peekPrevious()
{
	return m_sourceInfo->source[m_currentCharIndex - 1];
}

bool Scanner::isAtEnd()
{
	return m_currentCharIndex >= m_sourceInfo->source.size();
}

void Scanner::advance()
{
	if (isAtEnd() == false)
		m_currentCharIndex++;
}

void Scanner::advanceLine()
{
	m_sourceInfo->lineStartOffsets.push_back(m_currentCharIndex);
}

bool Scanner::match(char c)
{
	if (peek() == c)
	{
		advance();
		return true;
	}
	return false;
}

bool Scanner::isDigit(char c)
{
	return (c >= '0') && (c <= '9');
}

bool Scanner::isIdentifierStartChar(char c)
{
	return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || (c == '_') || (c == '$');
}

bool Scanner::isIdentifierChar(char c)
{
	return isIdentifierChar(c) || isDigit(c);
}