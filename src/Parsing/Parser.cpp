#include <Parsing/Parser.hpp>

using namespace Lang;

Parser::Parser()
	: m_tokens(nullptr)
	, m_currentTokenIndex(0)
	, m_sourceInfo(nullptr)
	, m_hadError(false)
{}

Parser::Result Parser::parse(const std::vector<Token>& tokens, const SourceInfo& sourceInfo)
{
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
	else
		return exprStmt();

}

//std::unique_ptr<Stmt> Parser::exprStmt()
//{
//	size_t start = peek().start;
//	auto expression = expr();
//	expect(TokenType::Semicolon, "expected ';'");
//	size_t end = peekPrevious().end;
//
//	auto stmt = std::make_unique<ExprStmt>(std::move(expression), start, end);
//	return stmt;
//}
//
//std::unique_ptr<Stmt> Parser::printStmt()
//{
//	size_t start = peekPrevious().start;
//	expect(TokenType::LeftParen, "expected '('");
//	auto expression = expr();
//	expect(TokenType::RightParen, "expected ')'");
//	expect(TokenType::Semicolon, "expected ';'");
//	size_t end = peek().end;
//	auto stmt = std::make_unique<PrintStmt>(std::move(expression), start, end);
//	return stmt;
//}
//
//std::unique_ptr<Stmt> Parser::letStmt()
//{
//	const auto start = peekPrevious().start;
//
//	expect(TokenType::Identifier, "expected variable name");
//	const Token& name = peekPrevious();
//
//	if (match(TokenType::Semicolon))
//	{
//		return std::make_unique<LetStmt>(name, std::nullopt, start, peek().end);
//	}
//	expect(TokenType::Equals, "expected '='");
//	auto initializer = expr();
//	expect(TokenType::Semicolon, "expected ';'");
//
//	return std::make_unique<LetStmt>(name, std::move(initializer), start, peek().end);
//}
//
//std::unique_ptr<Expr> Parser::expr()
//{
//	return factor();
//}
//
//std::unique_ptr<Expr> Parser::factor()
//{
//	size_t start = peek().start;
//	auto expr = primary();
//
//	while (match(TokenType::Plus) && (isAtEnd() == false))
//	{
//		Token op = peekPrevious();
//		auto rhs = primary();
//		size_t end = peekPrevious().end;
//		expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(rhs), op, start, end);
//	}
//
//	return expr;
//}
//
//std::unique_ptr<Expr> Parser::primary()
//{
//	if (match(TokenType::IntNumber))
//	{
//		return std::make_unique<IntConstantExpr>(peekPrevious(), peekPrevious().start, peekPrevious().end);
//	}
//	if (match(TokenType::Identifier))
//	{
//		return std::make_unique<IdentifierExpr>(peekPrevious(), peekPrevious().start, peekPrevious().end);
//	}
//
//	// TODO: Check if this error location is good or should it maybe be token.
//	throw errorAt(peek(), "expected primary expression");
//}
//
//const Token& Parser::peek() const
//{
//	return (*m_tokens)[m_currentTokenIndex];
//}
//
//const Token& Parser::peekPrevious() const
//{
//	return (*m_tokens)[m_currentTokenIndex - 1];
//}
//
//bool Parser::match(TokenType type)
//{
//	if (peek().type == type)
//	{
//		advance();
//		return true;
//	}
//	return false;
//}
//
//void Parser::expect(TokenType type, const char* format, ...)
//{
//	if (match(type) == false)
//	{
//		va_list args;
//		va_start(args, format);
//		errorAtImplementation(peek().start, peek().end - peek().start, format, args);
//		va_end(args);
//		throw ParsingError{};
//	}
//}
//
//void Parser::synchronize()
//{
//	while (isAtEnd() == false)
//	{
//		switch (peek().type)
//		{
//			case TokenType::Semicolon:
//				return;
//
//			default:
//				advance();
//				break;
//		}
//	}
//}
//
//Parser::ParsingError Parser::errorAt(size_t start, size_t end, const char* format, ...)
//{
//	va_list args;
//	va_start(args, format);
//	errorAtImplementation(start, end, format, args);
//	va_end(args);
//	return ParsingError{};
//}
//
//Parser::ParsingError Parser::errorAt(const Token& token, const char* format, ...)
//{
//	va_list args;
//	va_start(args, format);
//	errorAtImplementation(token.start, token.end, format, args);
//	va_end(args);
//	return ParsingError{};
//}
//
//void Parser::errorAtImplementation(size_t start, size_t end, const char* format, va_list args)
//{
//	m_hadError = true;
//	if (peek().type == TokenType::Error)
//	{
//		return;
//	}
//	m_errorPrinter->at(start, end, format, args);
//}
//
//void Parser::advance()
//{
//	if (isAtEnd() == false)
//		m_currentTokenIndex++;
//}
//
//bool Parser::isAtEnd()
//{
//	return (peek().type == TokenType::Eof);
//}