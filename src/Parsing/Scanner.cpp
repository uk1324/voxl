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
		case '+': return makeToken(TokenType::Plus);
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
		case ':': return makeToken(TokenType::Colon);
		case ',': return makeToken(TokenType::Comma);
		case '"': return string();

		default:
			if (isDigit(c))
				return number();
			if (isAlpha(c))
				return keywordOrIdentifier();

			const auto token = makeToken(TokenType::Error);
			errorAt(token, "illegal character");
			return token;
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
	};

	while (isAlnum(peek()) || (peek() == '_'))
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
	while (isAtEnd() == false)
	{
		if (match('"'))
		{
			return makeToken(TokenType::String);
		}
		
		if (match('\n'))
		{
			advanceLine();
		}

		advance();
	}
	const auto token = makeToken(TokenType::Error);
	errorAt(token, "unterminated string");
	return token;
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

void Scanner::errorAt(const Token& token, const char* format, ...)
{
	m_hadError = true;

	va_list args;
	va_start(args, format);
	const auto tokenStartLine = m_sourceInfo->getLine(token.start);
	m_errorPrinter->printErrorStart(tokenStartLine, m_currentCharIndex - m_sourceInfo->lineStartOffsets.back(), format, args);
	va_end(args);

	// Because the sourceInfo isn't completed yet this needs special handling.
	for (auto currentLine = tokenStartLine; currentLine + 1 < m_sourceInfo->lineStartOffsets.size(); currentLine++)
	{
		auto lineText = m_sourceInfo->source.substr(m_sourceInfo->lineStartOffsets[currentLine], m_sourceInfo->lineStartOffsets[currentLine + 1] - m_sourceInfo->lineStartOffsets[currentLine]);
		lineText = ErrorPrinter::trimLine(lineText);

		if (lineText.length() == 0)
		{
			continue;
		}
		m_errorPrinter->printLine(lineText);
	}

	auto currentLineStart = m_sourceInfo->lineStartOffsets.back();
	auto currentLineEnd = m_currentCharIndex;
	while ((currentLineEnd < m_sourceInfo->source.length()) && (m_sourceInfo->source[currentLineEnd + 1] != '\n'))
	{
		currentLineEnd++;
	}
	
	auto lastLineText = m_sourceInfo->source.substr(currentLineStart, currentLineEnd - currentLineStart);
	lastLineText = ErrorPrinter::trimLine(lastLineText);
	if (lastLineText.length() != 0)
	{
		m_errorPrinter->printLine(lastLineText);
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

// TODO: Support UTF-8
void Scanner::advance()
{
	if (isAtEnd() == false)
		m_currentCharIndex++;
}

void Scanner::advanceLine()
{
	m_sourceInfo->lineStartOffsets.push_back(m_currentCharIndex);
}

bool Scanner::isDigit(char c)
{
	return (c >= '0') && (c <= '9');
}

bool Scanner::isAlpha(char c)
{
	return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

bool Scanner::isAlnum(char c)
{
	return isAlpha(c) || isDigit(c);
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
