#include <Lang/Parsing/Parser.hpp>
#include <Lang/Ast/Expr/BinaryExpr.hpp>
#include <Lang/Ast/Expr/IntConstantExpr.hpp>
#include <Lang/Ast/Expr/IdentifierExpr.hpp>
#include <Lang/Ast/Stmt/ExprStmt.hpp>
#include <Lang/Ast/Stmt/PrintStmt.hpp>
#include <Lang/Ast/Stmt/LetStmt.hpp>
#include <Utils/Assertions.hpp>
#include <Utils/TerminalColors.hpp>

using namespace Lang;

Parser::Parser()
	: m_tokens(nullptr)
	, m_currentTokenIndex(0)
	, m_sourceInfo(nullptr)
{}

Parser::Result Parser::parse(const std::vector<Token>& tokens, ErrorPrinter& errorPrinter, const SourceInfo& sourceInfo)
{
	m_tokens = &tokens;
	m_sourceInfo = &sourceInfo;
	m_currentTokenIndex = 0;
	m_errorPrinter = &errorPrinter;
	m_hadError = false;

	std::vector<OwnPtr<Stmt>> ast;

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
			// if (isAtEnd()) // EOF error // Should there be eof errors

			// Skip errors from scanner.
			if (peek().type == TokenType::Error)
			{
				;
			}
			else
			{
				// GCC multi line error example
				/*
				source>:19:9: error: cannot convert 'X' to 'int'
			   19 |         X(
				  |         ^~
				  |         |
				  |         X
			   20 | 
				  |          
			   21 |         )
				  |         ~
				*/
				
				/*std::cout << TERM_COL_RED << "error: " << TERM_COL_RESET << error.message << '\n'
					<< m_sourceInfo->getLineText(m_sourceInfo->getLine(error.start));*/
			}

			synchronize();
		}
	}

	return Result{ m_hadError, std::move(ast) };
}

OwnPtr<Stmt> Parser::stmt()
{
	if (match(TokenType::Print))
		return printStmt();
	if (match(TokenType::Let))
		return letStmt();
	else
		return exprStmt();

}

OwnPtr<Stmt> Parser::exprStmt()
{
	size_t start = peek().start;
	auto expression = expr();
	expect(TokenType::Semicolon, "expected ';'");
	size_t end = peekPrevious().end;

	auto stmt = std::make_unique<ExprStmt>(std::move(expression), start, end);
	return stmt;
}

OwnPtr<Stmt> Parser::printStmt()
{
	size_t start = peekPrevious().start;
	expect(TokenType::LeftParen, "expected '('");
	auto expression = expr();
	expect(TokenType::RightParen, "expected ')'");
	expect(TokenType::Semicolon, "expected ';'");
	size_t end = peek().end;
	auto stmt = std::make_unique<PrintStmt>(std::move(expression), start, end);
	return stmt;
}

OwnPtr<Stmt> Parser::letStmt()
{
	const auto start = peekPrevious().start;

	expect(TokenType::Identifier, "expected variable name");
	const Token& name = peekPrevious();

	DataType dataType;
	if (match(TokenType::Colon))
	{
		dataType = parseDataType();
	}
	else
	{
		dataType.type = DataTypeType::Unspecified;
	}

	if (match(TokenType::Semicolon))
	{
		return std::make_unique<LetStmt>(name, dataType, std::nullopt, start, peek().end);
	}

	expect(TokenType::Equals, "expected '='");
	auto initializer = expr();
	expect(TokenType::Semicolon, "expected ';'");

	return std::make_unique<LetStmt>(name, dataType, std::move(initializer), start, peek().end);
}

OwnPtr<Expr> Parser::expr()
{
	return factor();
}

OwnPtr<Expr> Parser::factor()
{
	size_t start = peek().start;
	auto expr = primary();

	while (match(TokenType::Plus) && (isAtEnd() == false))
	{
		Token op = peekPrevious();
		auto rhs = primary();
		size_t end = peekPrevious().end;
		expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(rhs), op, start, end);
	}

	return expr;
}

OwnPtr<Expr> Parser::primary()
{
	if (match(TokenType::IntNumber))
	{
		return std::make_unique<IntConstantExpr>(peekPrevious(), peekPrevious().start, peekPrevious().end);
	}
	if (match(TokenType::Identifier))
	{
		return std::make_unique<IdentifierExpr>(peekPrevious(), peekPrevious().start, peekPrevious().end);
	}

	// TODO: Check if this error location is good or should it maybe be token.
	throw errorAt(peek(), "expected primary expression");
}

DataType Parser::parseDataType()
{
	auto start = peek().start;

	if (match(TokenType::Int))
	{
		return DataType(DataTypeType::Int);
	}
	if (match(TokenType::Float))
	{
		return DataType(DataTypeType::Float);
	}

	auto end = peek().start;

	// TODO: Check if this error location is good.
	throw errorAt(start, end, "expected type name");
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