#include "Parser.hpp"
#include "Parser.hpp"
#include "Parser.hpp"
#include "Parser.hpp"
#include <Parsing/Parser.hpp>

using namespace Lang;

Parser::Parser()
	: m_tokens(nullptr)
	, m_currentTokenIndex(0)
	, m_sourceInfo(nullptr)
	, m_hadError(false)
	, m_errorPrinter(nullptr)
{}

Parser::Result Parser::parse(const std::vector<Token>& tokens, const SourceInfo& sourceInfo, ErrorPrinter& errorPrinter)
{
	m_errorPrinter = &errorPrinter;
	m_tokens = &tokens;
	m_sourceInfo = &sourceInfo;

	std::vector<std::unique_ptr<Stmt>> ast;

	while (isAtEnd() == false)
	{
		try
		{
			if (match(TokenType::Semicolon))
				; // Null statement like this line
			else
				ast.push_back(stmt());
		}
		catch (const ParsingError&)
		{
			
			if (peek().type == TokenType::Error)
			{
				;
			}
			else
			{
			}

			synchronize();
		}
	}

	return Result{ m_hadError, std::move(ast) };
}

std::unique_ptr<Stmt> Parser::stmt()
{
	if (match(TokenType::Print))
		return printStmt();
	if (match(TokenType::Let))
		return letStmt();
	if (match(TokenType::LeftBrace))
		return blockStmt();
	if (match(TokenType::Fn))
		return fnStmt();
	if (match(TokenType::Ret))
		return retStmt();
	else
		return exprStmt();

}

std::unique_ptr<Stmt> Parser::exprStmt()
{
	size_t start = peek().start;
	auto expression = expr();
	expect(TokenType::Semicolon, "expected ';'");
	size_t end = peekPrevious().end;

	auto stmt = std::make_unique<ExprStmt>(std::move(expression), start, end);
	return stmt;
}

std::unique_ptr<Stmt> Parser::printStmt()
{
	auto start = peekPrevious().start;
	expect(TokenType::LeftParen, "expected '('");
	auto expression = expr();
	expect(TokenType::RightParen, "expected ')'");
	expect(TokenType::Semicolon, "expected ';'");
	auto stmt = std::make_unique<PrintStmt>(std::move(expression), start, peekPrevious().end);
	return stmt;
}

std::unique_ptr<Stmt> Parser::letStmt()
{
	const auto start = peekPrevious().start;

	expect(TokenType::Identifier, "expected variable name");
	const auto name = peekPrevious().identifier;

	if (match(TokenType::Semicolon))
	{
		return std::make_unique<LetStmt>(name, std::nullopt, start, peekPrevious().end);
	}
	expect(TokenType::Equals, "expected '='");
	auto initializer = expr();
	expect(TokenType::Semicolon, "expected ';'");

	return std::make_unique<LetStmt>(name, std::move(initializer), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::blockStmt()
{
	auto start = peek().start;
	auto stmts = block();
	auto end = peekPrevious().end;
	return std::make_unique<BlockStmt>(std::move(stmts), start, end);
}

std::unique_ptr<Stmt> Parser::fnStmt()
{
	auto start = peekPrevious().end;

	expect(TokenType::Identifier, "expected function name");
	auto name = peekPrevious().identifier;

	std::vector<std::string_view> arguments;
	expect(TokenType::LeftParen, "expected '('");
	if (peek().type != TokenType::RightParen)
	{
		do
		{
			expect(TokenType::Identifier, "expected function argument name");
			arguments.push_back(peekPrevious().identifier);
		} while (match(TokenType::Comma));
	}
	expect(TokenType::RightParen, "expected ')'");

	expect(TokenType::LeftBrace, "expected '{'");
	auto stmts = block();

	return std::make_unique<FnStmt>(name, std::move(arguments), std::move(stmts), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::retStmt()
{
	auto start = peekPrevious().start;

	std::optional<std::unique_ptr<Expr>> returnValue = std::nullopt;
	if (peek().type != TokenType::Semicolon)
	{
		returnValue = expr();
	}
	expect(TokenType::Semicolon, "expected ';'");

	return std::make_unique<RetStmt>(std::move(returnValue), start, peekPrevious().end);
}

std::vector<std::unique_ptr<Stmt>> Parser::block()
{
	std::vector<std::unique_ptr<Stmt>> stmts;
	while ((isAtEnd() == false) && (match(TokenType::RightBrace) == false))
	{
		stmts.push_back(stmt());
	}

	return stmts;
}

std::unique_ptr<Expr> Parser::expr()
{
	return factor();
}

std::unique_ptr<Expr> Parser::factor()
{
	size_t start = peek().start;
	auto expr = call();

	while (match(TokenType::Plus) && (isAtEnd() == false))
	{
		TokenType op = peekPrevious().type;
		auto rhs = call();
		size_t end = peekPrevious().end;
		expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(rhs), start, end);
	}

	return expr;
}

std::unique_ptr<Expr> Parser::call()
{
	size_t start = peek().start;
	auto calle = primary();

	while (match(TokenType::LeftParen))
	{
		std::vector<std::unique_ptr<Expr>> arguments;
		if (peek().type != TokenType::RightParen)
		{
			do
			{
				arguments.push_back(expr());
			} while (match(TokenType::Comma));
		}
		expect(TokenType::RightParen, "expected ')'");

		calle = std::make_unique<CallExpr>(std::move(calle), std::move(arguments), start, peekPrevious().end);
	}

	return calle;
}


std::unique_ptr<Expr> Parser::primary()
{
	if (match(TokenType::IntNumber))
	{
		return std::make_unique<IntConstantExpr>(peekPrevious().intValue, peekPrevious().start, peekPrevious().end);
	}
	if (match(TokenType::Identifier))
	{
		return std::make_unique<IdentifierExpr>(peekPrevious().identifier, peekPrevious().start, peekPrevious().end);
	}

	// TODO: Check if this error location is good or should it maybe be token.
	throw errorAt(peek(), "expected primary expression");
}

const Token& Parser::peek() const
{
	return (*m_tokens)[m_currentTokenIndex];
}

const Token& Parser::peekPrevious() const
{
	return (*m_tokens)[m_currentTokenIndex - 1];
}

bool Parser::match(TokenType type)
{
	if (peek().type == type)
	{
		advance();
		return true;
	}
	return false;
}

void Parser::expect(TokenType type, const char* format, ...)
{
	if (match(type) == false)
	{
		va_list args;
		va_start(args, format);
		errorAtImplementation(peek().start, peek().end - peek().start, format, args);
		va_end(args);
		throw ParsingError{};
	}
}

void Parser::synchronize()
{
	while (isAtEnd() == false)
	{
		switch (peek().type)
		{
			case TokenType::Semicolon:
				return;

			default:
				advance();
				break;
		}
	}
}

Parser::ParsingError Parser::errorAt(size_t start, size_t end, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	errorAtImplementation(start, end, format, args);
	va_end(args);
	return ParsingError{};
}

Parser::ParsingError Parser::errorAt(const Token& token, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	errorAtImplementation(token.start, token.end, format, args);
	va_end(args);
	return ParsingError{};
}

void Parser::errorAtImplementation(size_t start, size_t end, const char* format, va_list args)
{
	m_hadError = true;
	if (peek().type == TokenType::Error)
	{
		return;
	}
	m_errorPrinter->at(start, end, format, args);
}

void Parser::advance()
{
	if (isAtEnd() == false)
		m_currentTokenIndex++;
}

bool Parser::isAtEnd()
{
	return (peek().type == TokenType::Eof);
}