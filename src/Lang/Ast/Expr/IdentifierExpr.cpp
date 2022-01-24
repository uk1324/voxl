#include <Lang/Ast/Expr/IdentifierExpr.hpp>
#include <Lang/Ast/ExprVisitor.hpp>

using namespace Lang;

IdentifierExpr::IdentifierExpr(const Token& name, size_t start, size_t end)
	: name(name)
	, Expr(start, end)
{
}

void IdentifierExpr::accept(ExprVisitor& visitor) const
{
	visitor.visitIdentifierExpr(*this);
}

void IdentifierExpr::accept(NonConstExprVisitor& visitor)
{
	visitor.visitIdentifierExpr(*this);
}

ExprType IdentifierExpr::type() const
{
	return ExprType::Identifier;
}