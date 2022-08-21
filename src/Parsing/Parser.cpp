#include <Parsing/Parser.hpp>
#include <Format.hpp>

using namespace Voxl;

Parser::Parser()
	: Parser(false)
{}

Parser::Parser(bool ignoreEofErrors)
	: m_tokens(nullptr)
	, m_currentTokenIndex(0)
	, m_sourceInfo(nullptr)
	, m_errorReporter(nullptr)
	, m_hadError(false)
	, m_ignoreEofErrors(ignoreEofErrors)
{}

Parser::Result Parser::parse(const std::vector<Token>& tokens, const SourceInfo& sourceInfo, ErrorReporter& errorReporter)
{
	m_errorReporter = &errorReporter;
	m_tokens = &tokens;
	m_sourceInfo = &sourceInfo;
	m_currentTokenIndex = 0;
	m_hadError = false;

	bool eofError = false;
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
			if (check(TokenType::Eof))
				eofError = true;
			synchronize();
		}
	}

	return Result{ m_hadError, eofError, std::move(ast) };
}

std::unique_ptr<Stmt> Parser::stmt()
{
	// Could shorten this by first advancing the token then switching over the previous token and 
	// going back one token on default and returning exprStmt();
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
	if (match(TokenType::For))
		return forStmt();
	if (match(TokenType::Break))
		return breakStmt();
	if (match(TokenType::Class))
		return classStmt();
	if (match(TokenType::Impl))
		return implStmt();
	if (match(TokenType::Try))
		return tryStmt();
	if (match(TokenType::Throw))
		return throwStmt();
	if (match(TokenType::Match))
		return matchStmt();
	if (match(TokenType::Use))
		return useStmt();
	else
	{
		if (check(TokenType::Identifier) && (peekNext().type == TokenType::Colon))
		{
			return variableDeclarationStmt();
		}

		return exprStmt();
	}

}

std::unique_ptr<Stmt> Parser::exprStmt()
{
	const auto start = peek().start;
	auto expression = expr();
	expect(TokenType::Semicolon, "expected ';'");
	const auto end = peekPrevious().end;

	auto stmt = std::make_unique<ExprStmt>(std::move(expression), start, end);
	return stmt;
}

std::unique_ptr<Stmt> Parser::blockStmt()
{
	auto start = peekPrevious().start;
	StmtList stmts;
	while ((isAtEnd() == false) && (match(TokenType::RightBrace) == false))
	{
		try 
		{
			stmts.push_back(stmt());
		}
		catch (const ParsingError&)
		{
			synchronize();
		}
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

	if (match(TokenType::Else))
	{
		expect(TokenType::LeftBrace, "expected '{'");
		auto elseThen = blockStmt();
		return std::make_unique<IfStmt>(std::move(condition), std::move(ifThen), std::move(elseThen), start, peekPrevious().end);
	}
	else if (match(TokenType::Elif))
	{
		auto elseThen = ifStmt();
		return std::make_unique<IfStmt>(std::move(condition), std::move(ifThen), std::move(elseThen), start, peekPrevious().end);
	}
	else
	{
		return std::make_unique<IfStmt>(std::move(condition), std::move(ifThen), std::nullopt, start, peekPrevious().end);
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

std::unique_ptr<Stmt> Parser::forStmt()
{
	const auto start = peekPrevious().start;
	expect(TokenType::Identifier, "expected variable name");
	const auto& itemNameToken = peekPrevious();
	expect(TokenType::In, "expected 'in'");
	auto expression = expr();
	const auto expressionStart = expression->start;
	const auto expressionEnd = expression->end();
	auto stmts = block();
	const auto end = peekPrevious().end;

	/*
	Desugars to
	{
		.iterator : (<expr>).$iter();
		try {
			<item> : .iterator.$next();
			loop {
				<stmts>

				<item> = .iterator.$next();
			}
		} catch StopIteration {}
	}
	*/

	auto iterator = std::make_unique<CallExpr>(
		std::make_unique<GetFieldExpr>(std::move(expression), "$iter", expressionStart, expressionEnd),
		decltype(CallExpr::arguments)(),
		expressionStart, expressionEnd
	);

	// Using push because std::initializer_list doesn't work with move only types.
	decltype(VariableDeclarationStmt::variables) iteratorVariable;
	iteratorVariable.push_back({ ".iterator", std::move(iterator) });
	auto iteratorDeclaration = std::make_unique<VariableDeclarationStmt>(std::move(iteratorVariable), start, end);

#define NEXT_ITEM \
	std::make_unique<CallExpr>( \
		std::make_unique<GetFieldExpr>( \
			std::make_unique<IdentifierExpr>(".iterator", itemNameToken.start, itemNameToken.end), \
			"$next", \
			itemNameToken.start, itemNameToken.end \
		), \
		decltype(CallExpr::arguments)(), \
		itemNameToken.start, itemNameToken.end \
	)

	decltype(VariableDeclarationStmt::variables) itemVariable;
	itemVariable.push_back({ itemNameToken.identifier, NEXT_ITEM });
	auto itemDeclaration = std::make_unique<VariableDeclarationStmt>(std::move(itemVariable), start, end);

	auto iterExpr = std::make_unique<AssignmentExpr>(
		std::make_unique<IdentifierExpr>(itemNameToken.identifier, itemNameToken.start, itemNameToken.end), 
		NEXT_ITEM, std::nullopt, itemNameToken.start, itemNameToken.end);

	StmtList tryBlock;
	tryBlock.push_back(
		std::make_unique<LoopStmt>(std::move(itemDeclaration), std::nullopt, std::move(iterExpr), std::move(stmts), start, end));

	decltype(TryStmt::catchBlocks) catchBlocks;
	catchBlocks.push_back(TryStmt::CatchBlock{
		std::make_unique<ClassPtrn>("StopIteration", start, end),
		std::nullopt,
		StmtList()
	});
	
	StmtList block;
	block.push_back(std::move(iteratorDeclaration));
	block.push_back(std::make_unique<TryStmt>(std::move(tryBlock), std::move(catchBlocks), std::nullopt, start, end));

	return std::make_unique<BlockStmt>(std::move(block), start, end);
}

std::unique_ptr<Stmt> Parser::breakStmt()
{
	size_t start = peekPrevious().start;
	expect(TokenType::Semicolon, "expected ';'");
	return std::make_unique<BreakStmt>(start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::classStmt()
{
	const auto start = peekPrevious().start;

	expect(TokenType::Identifier, "expected class name");
	auto name = peekPrevious().identifier;

	std::optional<std::string_view> superclassName;
	if (match(TokenType::Less))
	{
		expect(TokenType::Identifier, "expected superclass name");
		superclassName = peekPrevious().identifier;
	}

	expect(TokenType::LeftBrace, "expected '{'");

	decltype(ClassStmt::methods) methods;

	while ((isAtEnd() == false) && (check(TokenType::RightBrace) == false))
	{
		methods.push_back(function(peek().end));
	}
	expect(TokenType::RightBrace, "expected '}'");
	
	return std::make_unique<ClassStmt>(name, superclassName, std::move(methods), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::implStmt()
{
	auto start = peekPrevious().start;
	expect(TokenType::Identifier, "expected type name");
	auto typeName = peekPrevious().identifier;

	expect(TokenType::LeftBrace, "expected '{'");

	decltype(ImplStmt::methods) methods;

	while ((isAtEnd() == false) && (check(TokenType::RightBrace) == false))
	{
		methods.push_back(function(peek().end));
	}
	expect(TokenType::RightBrace, "expected '}'");

	return std::make_unique<ImplStmt>(typeName, std::move(methods), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::tryStmt()
{
	size_t start = peekPrevious().start;
	auto tryBlock = block();

	if (match(TokenType::Finally))
	{
		auto finallyBlock = block();
		return std::make_unique<TryStmt>(
			std::move(tryBlock), 
			decltype(TryStmt::catchBlocks)(), 
			std::move(finallyBlock), 
			start, 
			peekPrevious().end);
	}

	if (check(TokenType::Catch) == false)
	{
		throw errorAt(peek(), "expected a catch block");
	}

	decltype(TryStmt::catchBlocks) catchBlocks;
	while ((isAtEnd() == false) && match(TokenType::Catch))
	{
		auto pattern = ptrn();
		std::optional<std::string_view> caughtValueName;
		if (match(TokenType::Arrow))
		{
			expect(TokenType::Identifier, "expected caught value name");
			caughtValueName = peekPrevious().identifier;
		}
		auto stmts = block();
		catchBlocks.push_back({ std::move(pattern), caughtValueName, std::move(stmts) });
	}

	if (match(TokenType::Finally))
	{
		auto finallyBlock = block();
		return std::make_unique<TryStmt>(
			std::move(tryBlock), 
			std::move(catchBlocks), 
			std::move(finallyBlock), 
			start, 
			peekPrevious().end);
	}

	return std::make_unique<TryStmt>(
		std::move(tryBlock),
		std::move(catchBlocks),
		std::nullopt,
		start,
		peekPrevious().end);
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

std::unique_ptr<Stmt> Parser::matchStmt()
{
	const auto start = peekPrevious().start;

	auto expression = expr();

	expect(TokenType::LeftBrace, "expected '{'");

	decltype (MatchStmt::cases) cases;

	while ((isAtEnd() == false) && (check(TokenType::RightBrace) == false))
	{
		auto pattern = ptrn();
		expect(TokenType::Arrow, "expected '=>'");
		auto statement = stmt();
		cases.push_back({ std::move(pattern), std::move(statement) });
	}
	expect(TokenType::RightBrace, "expected '}'");
	
	return std::make_unique<MatchStmt>(std::move(expression), std::move(cases), start, peekPrevious().end);
}

std::unique_ptr<Stmt> Parser::useStmt()
{
	const auto start = peekPrevious().start;
	expect(TokenType::StringConstant, "expected path string");
	const auto& path = peekPrevious().string.text;

	if (match(TokenType::Semicolon))
	{
		return std::make_unique<UseStmt>(path, std::nullopt, start, peekPrevious().end);
	}

	expect(TokenType::ThinArrow, "expected '->'");
	if (match(TokenType::Identifier))
	{
		const auto variableName = peekPrevious().identifier;
		expect(TokenType::Semicolon, "expected ';'");
		return std::make_unique<UseStmt>(path, variableName, start, peekPrevious().end);
	}
	else if (match(TokenType::Star))
	{
		expect(TokenType::Semicolon, "expected ';'");
		return std::make_unique<UseAllStmt>(path, start, peekPrevious().end);
	}

	expect(TokenType::LeftParen, "expected '('");
	decltype(UseSelectiveStmt::variablesToImport) imports;
	do
	{
		expect(TokenType::Identifier, "expected name of variable to import");
		const auto original= peekPrevious().identifier;
		if (match(TokenType::ThinArrow))
		{
			expect(TokenType::Identifier, "expected import alias name");
			const auto alias = peekPrevious().identifier;
			imports.push_back({ original, alias });
		}
		else
		{
			imports.push_back({ original, std::nullopt });
		}
	} while ((isAtEnd() == false) && match(TokenType::Comma));
	expect(TokenType::RightParen, "expected");
	expect(TokenType::Semicolon, "expected ';'");
	return std::make_unique<UseSelectiveStmt>(path, std::move(imports), start, peekPrevious().end);
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

	while ((isAtEnd() == false) && (check(TokenType::RightBrace) == false))
	{
		try
		{
			stmts.push_back(stmt());
		}
		catch (const ParsingError&)
		{
			synchronize();
		}
	}
	expect(TokenType::RightBrace, "expected '}'");
	
	return stmts;
}

std::unique_ptr<Expr> Parser::expr()
{
	// TODO: Maybe do checking for recursion depth here to avoid stack overflow.
	return stmtExpr();
}

std::unique_ptr<Expr> Parser::stmtExpr()
{
	//if (match(TokenType::At))
	//{
	//	const auto start = peekPrevious().start;
	//	auto statement = stmt();
	//	return std::make_unique<StmtExpr>(std::move(statement), start, peekPrevious().end);
	//}
	return assignment();
}

std::unique_ptr<Expr> Parser::assignment()
{
	const auto start = peek().start;
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
	COMPOUND_OP(PlusPlus)
	COMPOUND_OP(Minus)
	COMPOUND_OP(Star)
	COMPOUND_OP(Slash)
	COMPOUND_OP(Percent)

#undef COMPOUND_OP

	return lhs;
}

#define PARSE_LEFT_RECURSIVE_BINARY_EXPR(matches, lowerPrecedenceFunction) \
	const auto start = peek().start; \
	auto expr = lowerPrecedenceFunction(); \
	while ((matches) && (isAtEnd() == false)) \
	{ \
		TokenType op = peekPrevious().type; \
		auto rhs = lowerPrecedenceFunction(); \
		const auto end = peekPrevious().end; \
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
		auto expr = callOrFieldAccessOrIndex();
		return std::make_unique<UnaryExpr>(std::move(expr), op, start, peekPrevious().end);
	}

	return callOrFieldAccessOrIndex();
}

std::unique_ptr<Expr> Parser::callOrFieldAccessOrIndex()
{
	const auto start = peek().start;
	auto expression = primary();

	for (;;)
	{
		if (match(TokenType::LeftParen))
		{
			std::vector<std::unique_ptr<Expr>> arguments;
			if ((isAtEnd() == false) && (peek().type != TokenType::RightParen))
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
		else if (match(TokenType::LeftBracket))
		{
			auto index = expr();
			expect(TokenType::RightBracket, "expected ']'");
			expression = std::make_unique<BinaryExpr>(
				std::move(expression), 
				TokenType::LeftBracket, 
				std::move(index), 
				start, 
				peekPrevious().end);
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
		const auto start = peekPrevious().start;
		if (match(TokenType::RightBracket))
		{
			// decltype because templates cannot deduce from "{}".
			return std::make_unique<ListExpr>(decltype(ListExpr::values)(), start, peekPrevious().end);
		}
		std::vector<std::unique_ptr<Expr>> values;
		
		for (;;)
		{
			values.push_back(expr());
			if (match(TokenType::RightBracket))
			{
				return std::make_unique<ListExpr>(std::move(values), start, peekPrevious().end);
			}
			expect(TokenType::Comma, "expected ','");
			if (match(TokenType::RightBracket))
			{
				return std::make_unique<ListExpr>(std::move(values), start, peekPrevious().end);
			}
		}
	}
	if (match(TokenType::LeftBrace))
	{
		const auto start = peekPrevious().start;
		if (match(TokenType::RightBrace))
		{
			return std::make_unique<DictExpr>(decltype(DictExpr::values)(), start, peekPrevious().end);
		}
		decltype(DictExpr::values) values;

		for (;;)
		{
			auto key = expr();
			expect(TokenType::Colon, "expected ':'");
			auto value = expr();
			values.push_back({ std::move(key), std::move(value) });
			if (match(TokenType::RightBrace))
			{
				return std::make_unique<DictExpr>(std::move(values), start, peekPrevious().end);
			}
			expect(TokenType::Comma, "expected ','");
			if (match(TokenType::RightBrace))
			{
				return std::make_unique<DictExpr>(std::move(values), start, peekPrevious().end);
			}
		}
	}
	else if (match(TokenType::LeftParen))
	{
		auto expression = expr();
		expect(TokenType::RightParen, "expected ')'");
		return expression;
	}
	else if (match(TokenType::Or) || match(TokenType::OrOr))
	{
		const auto start = peekPrevious().start;
		decltype (LambdaExpr::arguments) arguments;
		if ((peekPrevious().type != TokenType::OrOr) && (match(TokenType::Or) == false))
		{
			do
			{
				expect(TokenType::Identifier, "expected argument name");
				arguments.push_back(peekPrevious().identifier);
			} while (match(TokenType::Comma));
			expect(TokenType::Or, "expected '|'");

		}

		if (check(TokenType::LeftBrace) == false)
		{
			const auto retStart = peek().start;
			auto retVal = expr();
			auto ret = std::make_unique<RetStmt>(std::move(retVal), retStart, peekPrevious().end);
			// Doing it this way because c++ can't infer the types.
			StmtList stmts;
			stmts.push_back(std::move(ret));
			return std::make_unique<LambdaExpr>(
				std::move(arguments), 
				std::move(stmts),
				start,
				peekPrevious().end);
		}

		auto stmts = block();
		return std::make_unique<LambdaExpr>(std::move(arguments), std::move(stmts), start, peekPrevious().end);
	}
	throw errorAt(peek(), "expected expression");
}

std::unique_ptr<Ptrn> Parser::ptrn()
{
	if (match(TokenType::Identifier))
	{
		return classPtrn();
	}
	else if (match(TokenType::LeftBrace))
	{
		const auto start = peekPrevious().start;
		auto expression = expr();
		expect(TokenType::RightBrace, "expected '}'");
		return std::make_unique<ExprPtrn>(std::move(expression), start, peekPrevious().end);
	}
	else if (match(TokenType::Star))
	{
		return std::make_unique<AlwaysTruePtrn>(peekPrevious().start, peekPrevious().end);
	}

	throw errorAt(peek(), "expected pattern");
}

std::unique_ptr<Ptrn> Parser::classPtrn()
{
	const auto& previous = peekPrevious();
	const auto className = previous.identifier;
	const auto start = previous.start;
	if (match(TokenType::LeftParen) == false)
		return std::make_unique<ClassPtrn>(className, start, previous.end);

	decltype (ClassPtrn::fieldPtrns) fieldPtrns;
	do
	{
		expect(TokenType::Identifier, "expected field name");
		const auto fieldName = peekPrevious().identifier;
		expect(TokenType::Equals, "expected '='");
		auto pattern = ptrn();
		fieldPtrns.push_back({ fieldName, std::move(pattern) });
	} while ((isAtEnd() == false) && match(TokenType::Comma));
	expect(TokenType::RightParen, "expected ')'");
	return std::make_unique<ClassPtrn>(className, std::move(fieldPtrns), start, peekPrevious().end);
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
		errorAtImlementation(peek(), format, args);
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
				advance();
				[[fallthrough]];
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

Parser::ParsingError Parser::errorAt(const Token& token, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	errorAtImlementation(token, format, args);
	va_end(args);
	return ParsingError{};
}

void Parser::errorAtImlementation(const Token& token, const char* format, va_list args)
{
	m_hadError = true;
	if ((check(TokenType::Error) || (m_ignoreEofErrors && check(TokenType::Eof))) == false)
	{
		const auto message = formatToTempBuffer(format, args);
		m_errorReporter->onParserError(token, message);
	}
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