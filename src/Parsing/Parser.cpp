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

	StmtList ast;
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
	if (match(TokenType::While))
		return whileStmt();
	if (match(TokenType::Break))
		return breakStmt();
	if (match(TokenType::Class))
		return classStmt();
	if (match(TokenType::Try))
		return tryStmt();
	if (match(TokenType::Throw))
		return throwStmt();
	else
	{
		if (check(TokenType::Identifier)
			&& ((peekNext().type == TokenType::Semicolon)) || (peekNext().type == TokenType::Colon) || (peekNext().type == TokenType::Comma))
		{
			return variableDeclarationStmt();
		}

		return exprStmt();
	}

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

std::unique_ptr<Stmt> Parser::blockStmt()
{
	auto start = peekPrevious().start;
	StmtList stmts;
	while ((isAtEnd() == false) && (match(TokenType::RightBrace) == false))
	{
		stmts.push_back(stmt());
	}

	return std::make_unique<BlockStmt>(std::move(stmts), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::fnStmt()
{
	return function(peekPrevious().start);
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
	auto stmts = block();
	// loop is just syntax for a while loop without condition.
	return std::make_unique<LoopStmt>(std::nullopt, std::nullopt, std::nullopt, std::move(stmts), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::whileStmt()
{
	size_t start = peekPrevious().start;
	auto condition = expr();
	auto stmts = block();
	return std::make_unique<LoopStmt>(std::nullopt, std::move(condition), std::nullopt, std::move(stmts), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::breakStmt()
{
	size_t start = peekPrevious().start;
	expect(TokenType::Semicolon, "expected ';'");
	return std::make_unique<BreakStmt>(start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::classStmt()
{
	size_t start = peekPrevious().start;

	expect(TokenType::Identifier, "expected class name");
	auto name = peekPrevious().identifier;

	expect(TokenType::LeftBrace, "expected '{'");

	decltype(ClassStmt::methods) methods;

	while ((isAtEnd() == false) && (check(TokenType::RightBrace) == false))
	{
		methods.push_back(function(peek().end));
	}
	expect(TokenType::RightBrace, "expected '}'");
	
	return std::make_unique<ClassStmt>(name, std::move(methods), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::tryStmt()
{
	size_t start = peekPrevious().start;
	auto tryBlock = block();

	if (match(TokenType::Finally))
	{
		auto finallyBlock = block();
		return std::make_unique<TryStmt>(std::move(tryBlock), std::nullopt, std::nullopt, std::move(finallyBlock), start, peekPrevious().end);
	}

	expect(TokenType::Catch, "expected catch block");

	std::optional<std::string_view> caughtValueName;
	if (match(TokenType::LeftParen))
	{
		expect(TokenType::Identifier, "expected caught value name");
		// TODO if (peek().type == TokenType::RightParen) hint("to ignore the caught value remove the parens");
		caughtValueName = peekPrevious().identifier;
		expect(TokenType::RightParen, "expected ')'");
	}
	auto catchBlock = block();

	if (match(TokenType::Finally))
	{
		auto finallyBlock = block();
		return std::make_unique<TryStmt>(std::move(tryBlock), caughtValueName, std::move(catchBlock), std::move(finallyBlock), start, peekPrevious().end);
	}

	return std::make_unique<TryStmt>(std::move(tryBlock), caughtValueName, std::move(catchBlock), std::nullopt, start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::throwStmt()
{
	auto start = peekPrevious().end;
	auto value = expr();
	expect(TokenType::Semicolon, "expected ';'");
	return std::make_unique<ThrowStmt>(std::move(value), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::variableDeclarationStmt()
{
	const auto start = peek().start;

	decltype (VariableDeclarationStmt::variables) nameInitializerPairs;

	do
	{
		expect(TokenType::Identifier, "expected variable name");
		auto name = peekPrevious().identifier;
		std::optional<std::unique_ptr<Expr>> initializer;
		if (match(TokenType::Colon))
		{
			initializer = expr();
		}
		nameInitializerPairs.push_back({ name, std::move(initializer) });
	} while ((isAtEnd() == false) && match(TokenType::Comma));

	expect(TokenType::Semicolon, "expected ';'");
	return std::make_unique<VariableDeclarationStmt>(std::move(nameInitializerPairs), start, peekPrevious().end);
}

std::unique_ptr<FnStmt> Parser::function(size_t start)
{
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
		} while ((isAtEnd() == false) && match(TokenType::Comma));
	}
	expect(TokenType::RightParen, "expected ')'");

	auto stmts = block();

	return std::make_unique<FnStmt>(name, std::move(arguments), std::move(stmts), start, peekPrevious().end);
}

std::vector<std::unique_ptr<Stmt>> Parser::block()
{
	expect(TokenType::LeftBrace, "expected '{'");
	StmtList stmts;
	while ((isAtEnd() == false) && (match(TokenType::RightBrace) == false))
	{
		stmts.push_back(stmt());
	}
	return stmts;
}

std::unique_ptr<Expr> Parser::expr()
{
	// TODO: Maybe do checking for recursion depth here to avoid stack overflow.
	return assignment();
}

std::unique_ptr<Expr> Parser::assignment()
{
	size_t start = peek().start;
	auto lhs = and();

#define COMPOUND_OP(opTokenType) \
	else if (match(TokenType::opTokenType##Equals)) \
	{ \
		auto rhs = assignment(); \
		return std::make_unique<AssignmentExpr>(std::move(lhs), std::move(rhs), TokenType::opTokenType, start, peekPrevious().end); \
	}

	if (match(TokenType::Equals))
	{
		auto rhs = assignment();
		return std::make_unique<AssignmentExpr>(std::move(lhs), std::move(rhs), std::nullopt, start, peekPrevious().end);
	}
	COMPOUND_OP(Plus)
	COMPOUND_OP(Minus)
	COMPOUND_OP(Star)
	COMPOUND_OP(Slash)
	COMPOUND_OP(Percent)

#undef COMPOUND_OP

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
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(match(TokenType::EqualsEquals) || match(TokenType::NotEquals), comparasion)
}

std::unique_ptr<Expr> Parser::comparasion()
{
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(
		match(TokenType::Less) || match(TokenType::LessEquals) || match(TokenType::More) || match(TokenType::MoreEquals), factor)
}

std::unique_ptr<Expr> Parser::factor()
{
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(match(TokenType::Plus) || match(TokenType::PlusPlus) || match(TokenType::Minus), term)
}

std::unique_ptr<Expr> Parser::term()
{
	PARSE_LEFT_RECURSIVE_BINARY_EXPR(match(TokenType::Star) || match(TokenType::Slash) || match(TokenType::Percent), unary)
}

std::unique_ptr<Expr> Parser::unary()
{
	size_t start = peek().start;

	if (match(TokenType::Minus) || match(TokenType::Not))
	{
		auto op = peekPrevious().type;
		auto expr = callOrFieldAccess();
		return std::make_unique<UnaryExpr>(std::move(expr), op, start, peekPrevious().end);
	}

	return callOrFieldAccess();
}

std::unique_ptr<Expr> Parser::callOrFieldAccess()
{
	size_t start = peek().start;
	auto expression = primary();

	for (;;)
	{
		if (match(TokenType::LeftParen))
		{
			std::vector<std::unique_ptr<Expr>> arguments;
			if (peek().type != TokenType::RightParen)
			{
				do
				{
					arguments.push_back(expr());
				} while ((isAtEnd() == false) && match(TokenType::Comma));
			}
			expect(TokenType::RightParen, "expected ')'");
			expression = std::make_unique<CallExpr>(std::move(expression), std::move(arguments), start, peekPrevious().end);
		}
		else if (match(TokenType::Dot))
		{
			expect(TokenType::Identifier, "expected field name");
			const auto name = peekPrevious().identifier;
			expression = std::make_unique<GetFieldExpr>(std::move(expression), name, start, peekPrevious().end);
		}
		else
		{
			return expression;
		}
	}
}

std::unique_ptr<Expr> Parser::primary()
{
	if (match(TokenType::IntNumber))
	{
		return std::make_unique<IntConstantExpr>(peekPrevious().intValue, peekPrevious().start, peekPrevious().end);
	}
	if (match(TokenType::FloatNumber))
	{
		return std::make_unique<FloatConstantExpr>(peekPrevious().floatValue, peekPrevious().start, peekPrevious().end);
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
	if (match(TokenType::Null))
	{
		return std::make_unique<NullExpr>(peekPrevious().start, peekPrevious().end);
	}
	if (match(TokenType::LeftBracket))
	{
		size_t start = peekPrevious().start;
		if (match(TokenType::RightBracket))
		{
			// decltype because templates cannot deduce from "{}".
			return std::make_unique<ArrayExpr>(decltype(ArrayExpr::values)(), start, peekPrevious().end);
		}
		std::vector<std::unique_ptr<Expr>> values;
		
		for (;;)
		{
			values.push_back(expr());
			if (match(TokenType::RightBrace))
			{
				return std::make_unique<ArrayExpr>(std::move(values), start, peekPrevious().end);
			}
			expect(TokenType::Comma, "expected ','");
			if (match(TokenType::RightBrace))
			{
				return std::make_unique<ArrayExpr>(std::move(values), start, peekPrevious().end);
			}
		}
	}
	else if (match(TokenType::LeftParen))
	{
		auto expression = expr();
		expect(TokenType::RightParen, "expected ')'");
		return expression;
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

const Token& Parser::peekNext() const
{
	if (isAtEnd())
		return peek();

	return (*m_tokens)[m_currentTokenIndex + 1];
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

void Parser::expectSemicolon()
{
	expect(TokenType::Semicolon, "expected ';'");
}

void Parser::synchronize()
{
	while (isAtEnd() == false)
	{
		switch (peek().type)
		{
			case TokenType::Semicolon:
			case TokenType::Class:
			case TokenType::Try:
			case TokenType::Throw:
			case TokenType::Fn:
			case TokenType::For:
			case TokenType::Loop:
			case TokenType::While:
			case TokenType::If:
 			case TokenType::Ret:
			case TokenType::Break:
			case TokenType::Continue:
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

bool Parser::isAtEnd() const
{
	return (peek().type == TokenType::Eof);
}