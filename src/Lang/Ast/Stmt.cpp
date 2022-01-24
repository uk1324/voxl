#include <Lang/Ast/Stmt.hpp>
#include <Utils/Assertions.hpp>

using namespace Lang;

const char* Lang::stmtTypeToString(StmtType type)
{
	// Added Stmt at end so you can see if something is an expr or stmt in the JSON.
	switch (type)
	{
	case StmtType::Expr: return "ExprStmt";
	case StmtType::Print: return "PrintStmt";
	case StmtType::VariableDeclaration: return "VariableDeclarationStmt";
	default:
		ASSERT_NOT_REACHED();
		return "";
	}
}

Stmt::Stmt(size_t start, size_t end)
	: start(start)
	, length(end - start)
{}