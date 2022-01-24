#include <Lang/Ast/Stmt/LetStmt.hpp>
#include <Lang/Ast/StmtVisitor.hpp>

using namespace Lang;

LetStmt::LetStmt(const Token& name, const DataType& type, Opt<OwnPtr<Expr>> initializer, size_t start, size_t end)
	: Stmt(start, end)
	, initializer(std::move(initializer))
	, dataType(type)
	, name(name)
{}

void LetStmt::accept(StmtVisitor& visitor) const
{
	visitor.visitLetStmt(*this);
}

void LetStmt::accept(NonConstStmtVisitor& visitor)
{
	visitor.visitLetStmt(*this);
}

StmtType LetStmt::type() const
{
	return StmtType::VariableDeclaration;
}
