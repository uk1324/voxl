#include <Compiling/Compiler.hpp>
#include <Asserts.hpp>
#include <iostream>

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
	m_currentScope = std::nullopt;

	for (const auto& stmt : ast)
	{
		try
		{
			this->compile(stmt);
		}
		catch (const Error&)
		{
			break;
		}
	}

	emitOp(Op::Return);

	return Result{ m_hadError, std::move(m_program) };
}

void Compiler::compile(const std::unique_ptr<Stmt>& stmt)
{
#define CASE_STMT_TYPE(stmtType, stmtFunction) case StmtType::stmtType: stmtFunction(*static_cast<stmtType##Stmt*>(stmt.get())); break;
	switch (stmt.get()->type)
	{
		CASE_STMT_TYPE(Expr, exprStmt)
		CASE_STMT_TYPE(Print, printStmt)
	}
}

void Compiler::exprStmt(const ExprStmt& stmt)
{
	compile(stmt.expr);
	emitOp(Op::PopStack);
}

void Compiler::printStmt(const PrintStmt& stmt)
{
	compile(stmt.expr);
	emitOp(Op::Print);
	emitOp(Op::PopStack);
}

void Compiler::compile(const std::unique_ptr<Expr>& expr)
{
#define CASE_EXPR_TYPE(exprType, exprFunction) case ExprType::exprType: exprFunction(*static_cast<exprType##Expr*>(expr.get())); break;
	switch (expr.get()->type)
	{
		CASE_EXPR_TYPE(IntConstant, intConstantExpr)
		CASE_EXPR_TYPE(Binary, binaryExpr)
	}
}

void Compiler::intConstantExpr(const IntConstantExpr& expr)
{
	// TODO: Search if constant already exists.
	auto constant = createConstant(Value(expr.value));
	loadConstant(constant);
}

void Compiler::binaryExpr(const BinaryExpr& expr)
{
	compile(expr.lhs);
	compile(expr.rhs);
	// Can't pop here beacause I have to keep the result at the top. The pops will in the vm.
	switch (expr.op)
	{
		case TokenType::Plus: emitOp(Op::Add); break;
		default:
			ASSERT_NOT_REACHED();
	}
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

void Compiler::declareVariable(const Token& name)
{
	//if (m_currentScope.has_value())
	//{
	//	ASSERT((*m_currentScope)->localVariables.find(name) == (*m_currentScope)->localVariables.end());
	//	(*m_currentScope)->localVariables[name] = Local{ (*m_currentScope)->localVariables.size() };
	//	return;
	//}

	//ASSERT(m_gobalVariables.find(name) == m_gobalVariables.end());
	//m_gobalVariables[name] = Global{ m_gobalVariables.size() };
}

void Compiler::loadVariable(const Token& name)
{

}

Compiler::Error Compiler::errorAt(size_t start, size_t end, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(start, end, format, args);
	//va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Stmt& stmt, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(stmt.start, stmt.start + stmt.length, format, args);
	//va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Expr& expr, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(expr.start, expr.start + expr.length, format, args);
	//va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Token& token, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(token.start, token.end, format, args);
	//va_end(args);
	return Error{};
}
