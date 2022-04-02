#include <Compiling/Compiler.hpp>
#include <Asserts.hpp>
#include <iostream>

#define TRY(somethingThatReturnsStatus) \
	do \
	{ \
		if ((somethingThatReturnsStatus) == Status::Error) \
		{ \
			return Status::Error; \
		} \
	} while (false)

using namespace Lang;

Compiler::Compiler()
	: m_hadError(false)
	, m_allocator(nullptr)
	, m_errorPrinter(nullptr)
{}

Compiler::Result Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& ast, ErrorPrinter& errorPrinter, Allocator& allocator)
{
	m_hadError = false;
	m_errorPrinter = &errorPrinter;
	m_program = Program{};
	m_allocator = &allocator;

	for (const auto& stmt : ast)
	{
		if (compile(stmt) == Status::Error)
		{
			break;
		}
	}

	emitOp(Op::Return);

	return Result{ m_hadError, std::move(m_program) };
}

Compiler::Status Compiler::compile(const std::unique_ptr<Stmt>& stmt)
{
#define CASE_STMT_TYPE(stmtType, stmtFunction) case StmtType::stmtType: return stmtFunction(*static_cast<stmtType##Stmt*>(stmt.get())); break;
	switch (stmt.get()->type)
	{
		CASE_STMT_TYPE(Expr, exprStmt)
		CASE_STMT_TYPE(Print, printStmt)
		CASE_STMT_TYPE(Let, letStmt)
		CASE_STMT_TYPE(Block, blockStmt)
	}

	ASSERT_NOT_REACHED();
	return Status::Error;
}

Compiler::Status Compiler::exprStmt(const ExprStmt& stmt)
{
	TRY(compile(stmt.expr));
	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::printStmt(const PrintStmt& stmt)
{
	TRY(compile(stmt.expr));
	emitOp(Op::Print);
	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::letStmt(const LetStmt& stmt)
{
	// TODO: This is a bad explanation. Fix it.
	// Only the let statement leaves a side effect on the stack by leaving the value of the variable on top of the stack.
	if (stmt.initializer != std::nullopt)
	{
		TRY(compile(*stmt.initializer));
	}
	else
	{
		emitOp(Op::LoadNull);
	}
	// The initializer is evaluated before declaring the variable so a variable from an oter scope with the same name can be used.
	//declareVariable()

	if (m_scopes.size() == 0)
	{
		auto constant = createConstant(Value(m_allocator->allocateString(stmt.identifier)));
		loadConstant(constant);
		emitOp(Op::CreateGlobal);
	}
	else
	{
		auto& locals = m_scopes.back().localVariables;
		auto variable = locals.find(stmt.identifier);
		if (variable != locals.end())
		{
			return errorAt(stmt, "redeclaration of variable '%.*s'", stmt.identifier.size(), stmt.identifier.data());
		}
		locals[stmt.identifier] = Local{ static_cast<uint32_t>(locals.size()) };
	}
	return Status::Ok;
}

Compiler::Status Compiler::blockStmt(const BlockStmt& stmt)
{
	beginScope();
	for (const auto& s : stmt.stmts)
	{
		TRY(compile(s));
	}
	endScope();

	return Status::Ok;
}

Compiler::Status Compiler::compile(const std::unique_ptr<Expr>& expr)
{
#define CASE_EXPR_TYPE(exprType, exprFunction) case ExprType::exprType: return exprFunction(*static_cast<exprType##Expr*>(expr.get())); break;
	switch (expr.get()->type)
	{
		CASE_EXPR_TYPE(IntConstant, intConstantExpr)
		CASE_EXPR_TYPE(Binary, binaryExpr)
		CASE_EXPR_TYPE(Identifier, identifierExpr)
	}

	ASSERT_NOT_REACHED();
	return Status::Ok;
}

Compiler::Status Compiler::intConstantExpr(const IntConstantExpr& expr)
{
	// TODO: Search if constant already exists.
	auto constant = createConstant(Value(expr.value));
	loadConstant(constant);
	return Status::Ok;
}

Compiler::Status Compiler::identifierExpr(const IdentifierExpr& expr)
{
	for (const auto& scope : m_scopes)
	{
		auto local = scope.localVariables.find(expr.identifier);
		if (local != scope.localVariables.end())
		{
			emitOp(Op::LoadLocal);
			emitUint32(local->second.index);
			return Status::Ok;
		}
	}

	auto constant = createConstant(Value(m_allocator->allocateString(expr.identifier)));
	loadConstant(constant);
	emitOp(Op::LoadGlobal);

	return Status::Ok;
}

Compiler::Status Compiler::binaryExpr(const BinaryExpr& expr)
{
	TRY(compile(expr.lhs));
	TRY(compile(expr.rhs));
	// Can't pop here beacause I have to keep the result at the top. The pops will in the vm.
	switch (expr.op)
	{
		case TokenType::Plus: emitOp(Op::Add); break;
		default:
			ASSERT_NOT_REACHED();
	}

	return Status::Ok;
}

uint32_t Compiler::createConstant(Value value)
{
	m_program.constants.push_back(value);
	return static_cast<uint32_t>(m_program.constants.size() - 1);
}

void Compiler::loadConstant(uint32_t index)
{
	emitOp(Op::LoadConstant);
	emitUint32(index);
}

void Compiler::emitOp(Op op)
{
	m_program.code.emitOp(op);
}

void Compiler::emitUint32(uint32_t value)
{
	m_program.code.emitUint32(value);
}

// For the function to work initializer has to already be loaded on top of the stack
//Compiler::Status Compiler::declareVariable(std::string_view name)
//{
//
//	//if (m_currentScope.has_value())
//	//{
//	//	ASSERT((*m_currentScope)->localVariables.find(name) == (*m_currentScope)->localVariables.end());
//	//	(*m_currentScope)->localVariables[name] = Local{ (*m_currentScope)->localVariables.size() };
//	//	return;
//	//}
//
//	//ASSERT(m_gobalVariables.find(name) == m_gobalVariables.end());
//	//m_gobalVariables[name] = Global{ m_gobalVariables.size() };
//}

//Compiler::Status Compiler::loadVariable(std::string_view name)
//{
//
//}

void Compiler::beginScope()
{
	m_scopes.push_back(Scope{ {}, 0 });
}

void Compiler::endScope()
{
	ASSERT(m_scopes.size() > 0);
	m_scopes.pop_back();
}

Compiler::Status Compiler::errorAt(size_t start, size_t end, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(start, end, format, args);
	va_end(args);
	return Status::Error;
}

Compiler::Status Compiler::errorAt(const Stmt& stmt, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(stmt.start, stmt.start + stmt.length, format, args);
	va_end(args);
	return Status::Error;
}

Compiler::Status Compiler::errorAt(const Expr& expr, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(expr.start, expr.start + expr.length, format, args);
	va_end(args);
	return Status::Error;
}

Compiler::Status Compiler::errorAt(const Token& token, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(token.start, token.end, format, args);
	va_end(args);
	return Status::Error;
}
