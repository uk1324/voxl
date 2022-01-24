#include <Lang/Ast/Stmt/PrintStmt.hpp>
#include <Lang/Ast/StmtVisitor.hpp>

using namespace Lang;

PrintStmt::PrintStmt(OwnPtr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end)
	, expr(std::move(expr))
{}

void PrintStmt::accept(StmtVisitor& visitor) const
{
	visitor.visitPrintStmt(*this);
}

void PrintStmt::accept(NonConstStmtVisitor& visitor)
{
	visitor.visitPrintStmt(*this);
}

StmtType PrintStmt::type() const
{
	return StmtType::Print;
}
