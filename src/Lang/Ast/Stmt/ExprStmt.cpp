#include <Lang/Ast/Stmt/ExprStmt.hpp>
#include <Lang/Ast/StmtVisitor.hpp>

using namespace Lang;

ExprStmt::ExprStmt(OwnPtr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end)
	, expr(std::move(expr))
{}

void ExprStmt::accept(StmtVisitor& visitor) const
{
	visitor.visitExprStmt(*this);
}

void ExprStmt::accept(NonConstStmtVisitor& visitor)
{
	visitor.visitExprStmt(*this);
}

StmtType ExprStmt::type() const
{
	return StmtType::Expr;
}