#pragma once

#include <Lang/Ast/Expr.hpp>
#include <Lang/Parsing/Token.hpp>
#include <Lang/Value.hpp>

namespace Lang
{
	class IntConstantExpr : public Expr
	{
	public:
		IntConstantExpr(Token token, size_t start, size_t end);

		void accept(ExprVisitor& visitor) const override;
		void accept(NonConstExprVisitor& visitor) override;
		ExprType type() const override;

	public:
		Int value;
	};
}