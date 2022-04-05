#include <Ast.hpp>

using namespace Lang;

Expr::Expr(size_t start, size_t end, ExprType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

size_t Expr::end() const
{
	return start + length;
}

IntConstantExpr::IntConstantExpr(Int value, size_t start, size_t end)
	: Expr(start, end, ExprType::IntConstant)
	, value(value)
{}

FloatConstantExpr::FloatConstantExpr(Float value, size_t start, size_t end)
	: Expr(start, end, ExprType::FloatConstant)
	, value(value)
{}

BinaryExpr::BinaryExpr(std::unique_ptr<Expr> lhs, TokenType op, std::unique_ptr<Expr> rhs, size_t start, size_t end)
	: Expr(start, end, ExprType::Binary)
	, lhs(std::move(lhs))
	, op(op)
	, rhs(std::move(rhs))
{}

IdentifierExpr::IdentifierExpr(std::string_view identifier, size_t start, size_t end)
	: Expr(start, end, ExprType::Identifier)
	, identifier(identifier)
{}

CallExpr::CallExpr(std::unique_ptr<Expr> calle, std::vector<std::unique_ptr<Expr>> arguments, size_t start, size_t end)
	: Expr(start, end, ExprType::Call)
	, calle(std::move(calle))
	, arguments(std::move(arguments))
{}

Stmt::Stmt(size_t start, size_t end, StmtType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

size_t Stmt::end() const
{
	return start + length;
}

ExprStmt::ExprStmt(std::unique_ptr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end, StmtType::Expr)
	, expr(std::move(expr))
{}

PrintStmt::PrintStmt(std::unique_ptr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end, StmtType::Print)
	, expr(std::move(expr))
{}

LetStmt::LetStmt(std::string_view identifier, std::optional<std::unique_ptr<Expr>> initializer, size_t start, size_t end)
	: Stmt(start, end, StmtType::Let)
	, identifier(identifier)
	, initializer(std::move(initializer))
{}

BlockStmt::BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts, size_t start, size_t end)
	: Stmt(start, end, StmtType::Block)
	, stmts(std::move(stmts))
{}

FnStmt::FnStmt(
	std::string_view name,
	std::vector<std::string_view> arguments,
	std::vector<std::unique_ptr<Stmt>> stmts,
	size_t start,
	size_t end)
	: Stmt(start, end, StmtType::Fn)
	, name(name)
	, arguments(std::move(arguments))
	, stmts(std::move(stmts))
{}

RetStmt::RetStmt(std::optional<std::unique_ptr<Expr>> returnValue, size_t start, size_t end)
	: Stmt(start, end, StmtType::Ret)
	, returnValue(std::move(returnValue))
{}
