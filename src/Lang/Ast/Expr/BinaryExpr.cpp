#include <Lang/Ast/Expr/BinaryExpr.hpp>
#include <Lang/Ast/ExprVisitor.hpp>

using namespace Lang;

BinaryExpr::BinaryExpr(OwnPtr<Expr> lhs, OwnPtr<Expr> rhs, Token op, size_t start, size_t end)
	: Expr(start, end)
	, lhs(std::move(lhs))
	, rhs(std::move(rhs))
	, op(op)
{}

void BinaryExpr::accept(ExprVisitor& visitor) const
{
	visitor.visitBinaryExpr(*this);
}

void BinaryExpr::accept(NonConstExprVisitor& visitor)
{
	visitor.visitBinaryExpr(*this);
}

ExprType BinaryExpr::type() const
{
	return ExprType::Binary;
}
