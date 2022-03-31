#include <Ast.hpp>

using namespace Lang;

Expr::Expr(size_t start, size_t end, ExprType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

IntConstantExpr::IntConstantExpr(Int value, size_t start, size_t end)
	: Expr(start, end, ExprType::IntConstant),
	, value(value)
{}

FloatConstantExpr::FloatConstantExpr(Float value, size_t start, size_t end)
	: Expr(start, end, ExprType::FloatConstant),
	, value(value)
{}

BinaryExpr::BinaryExpr(std::unique_ptr<Expr> lhs, TokenType op, std::unique_ptr<Expr> rhs, size_t start, size_t end)
	: Expr(start, end, ExprType::Binary),
	, lhs(std::move(lhs))
	, op(op)
	, rhs(std::move(rhs))
{}

IdentifierExpr::IdentifierExpr(std::string_view identifier, size_t start, size_t end)
	: Expr(start, end, ExprType::Binary)
	, identifier(identifier)
{}

Stmt::Stmt(size_t start, size_t end, StmtType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

ExprStmt::ExprStmt(std::unique_ptr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end, StmtType::Expr)
	, expr(std::move(expr))
{}

PrintStmt::PrintStmt(std::unique_ptr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end, StmtType::Print)
	, expr(std::move(expr))
{}

LetStmt::LetStmt(std::string_view identifier, std::optional<std::unique_ptr<Expr>> initializer, size_t start, size_t end)
	: Stmt(start, end, StmtType::LetStmt)
	, identifier(identifier)
	, initializer(std::move(initializer))
{}