#pragma once

#include <Lang/Ast/Expr.hpp>
#include <Lang/Parsing/Token.hpp>
#include <Utils/SmartPointers.hpp>

namespace Lang
{
	class BinaryExpr : public Expr
	{
	public:
		BinaryExpr(OwnPtr<Expr> lhs, OwnPtr<Expr> rhs, Token op, size_t start, size_t end);

		void accept(ExprVisitor& visitor) const override;
		void accept(NonConstExprVisitor& visitor) override;
		ExprType type() const override;

	public:
		OwnPtr<Expr> lhs;
		OwnPtr<Expr> rhs;
		Token op;
	};
}