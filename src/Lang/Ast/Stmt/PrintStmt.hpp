#pragma once

#include <Lang/Ast/Stmt.hpp>
#include <Lang/Ast/Expr.hpp>
#include <Utils/SmartPointers.hpp>

namespace Lang
{
	class PrintStmt : public Stmt
	{
	public:
		PrintStmt(OwnPtr<Expr> expr, size_t start, size_t end);

		void accept(StmtVisitor& visitor) const override;
		void accept(NonConstStmtVisitor& visitor) override;
		StmtType type() const override;

	public:
		OwnPtr<Expr> expr;
	};
}
