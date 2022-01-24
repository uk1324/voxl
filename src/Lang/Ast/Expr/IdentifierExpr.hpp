#pragma once

#include <Lang/Ast/Expr.hpp>
#include <Lang/Parsing/Token.hpp>

namespace Lang
{
	class IdentifierExpr : public Expr
	{
	public:
		IdentifierExpr(const Token& name, size_t start, size_t end);

		void accept(ExprVisitor& visitor) const override;
		void accept(NonConstExprVisitor& visitor) override;
		ExprType type() const override;

	public:
		Token name;
	};
}
