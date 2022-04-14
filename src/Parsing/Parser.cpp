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
	m_currentTokenIndex = 0;

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
	// Could shorten this by first advancing the token then switching over the previous token and 
	// going back one token on default and returning exprStmt();
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
	if (match(TokenType::If))
		return ifStmt();
	if (match(TokenType::Loop))
		return loopStmt();
	if (match(TokenType::Break))
		return breakStmt();
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

std::unique_ptr<Stmt> Parser::ifStmt()
{
	auto start = peekPrevious().start;
	auto condition = expr();

	expect(TokenType::LeftBrace, "expected '{'");
	auto ifThen = block();

	if (match(TokenType::Else) == false)
	{
		return std::make_unique<IfStmt>(std::move(condition), std::move(ifThen), std::nullopt, start, peekPrevious().end);
	}

	if (match(TokenType::If))
	{
		auto elseThen = ifStmt();
		return std::make_unique<IfStmt>(std::move(condition), std::move(ifThen), std::move(elseThen), start, peekPrevious().end);
	}
	else
	{
		expect(TokenType::LeftBrace, "expected '{'");
		auto elseThen = blockStmt();
		return std::make_unique<IfStmt>(std::move(condition), std::move(ifThen), std::move(elseThen), start, peekPrevious().end);
	}
}

std::unique_ptr<Stmt> Parser::loopStmt()
{
	size_t start = peekPrevious().start;
	expect(TokenType::LeftBrace, "expected '{'");
	auto stmts = block();
	// loop is just syntax for a while loop without condition.
	return std::make_unique<LoopStmt>(std::nullopt, std::nullopt, std::nullopt, std::move(stmts), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::breakStmt()
{
	size_t start = peekPrevious().start;
	expect(TokenType::Semicolon, "expected ';'");
	return std::make_unique<BreakStmt>(start, peekPrevious().end);
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
	return assignment();
}

std::unique_ptr<Expr> Parser::assignment()
{
	size_t start = peek().start;
	auto lhs = and();

	if (match(TokenType::Equals))
	{
		auto rhs = assignment();
		return std::make_unique<AssignmentExpr>(std::move(lhs), std::move(rhs), start, peekPrevious().end);
	}

	return lhs;
}

#define PARSE_LEFT_RECURSIVE_BINARY_EXPR(matches, lowerPrecedenceFunction) \
	size_t start = peek().start; \
	auto expr = lowerPrecedenceFunction(); \
	while ((matches) && (isAtEnd() == false)) \
	{ \
		TokenType op = peekPrevious().type; \
		auto rhs = lowerPrecedenceFunction(); \
		size_t end = peekPrevious().end; \
		expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(rhs), start, end); \
	} \
	return expr;

std::unique_ptr<Expr> Parser::and()
{
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(match(TokenType::AndAnd), or)
}

std::unique_ptr<Expr> Parser:: or()
{
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(match(TokenType::OrOr), equality)
}

std::unique_ptr<Expr> Parser::equality()
{
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(match(TokenType::EqualsEquals), factor)
}

std::unique_ptr<Expr> Parser::factor()
{
	size_t start = peek().start;
	auto expr = unary();

	while (match(TokenType::Plus) && (isAtEnd() == false))
	{
		TokenType op = peekPrevious().type;
		auto rhs = unary();
		size_t end = peekPrevious().end;
		expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(rhs), start, end);
	}

	return expr;
}

std::unique_ptr<Expr> Parser::unary()
{
	size_t start = peek().start;

	if (match(TokenType::Minus) || match(TokenType::Not))
	{
		auto op = peekPrevious().type;
		auto expr = call();
		return std::make_unique<UnaryExpr>(std::move(expr), op, start, peekPrevious().end);
	}

	return call();
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
	if (match(TokenType::StringConstant))
	{
		return std::make_unique<StringConstantExpr>(peekPrevious().string.text, peekPrevious().string.length, peekPrevious().start, peekPrevious().end);
	}
	if (match(TokenType::True))
	{
		return std::make_unique<BoolConstantExpr>(true, peekPrevious().start, peekPrevious().end);
	}
	if (match(TokenType::False))
	{
		return std::make_unique<BoolConstantExpr>(false, peekPrevious().start, peekPrevious().end);
	}

	// TODO: Check if this error location is good or should it maybe be token.
	throw errorAt(peek(), "expected expression");
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

bool Parser::check(TokenType type)
{
	return (peek().type == type);
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