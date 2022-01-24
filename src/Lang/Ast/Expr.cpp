#include <Lang/Ast/Expr.hpp>
#include <Utils/Assertions.hpp>

using namespace Lang;

const char* Lang::exprTypeToString(ExprType type)
{
	// Added Expr at end so you can see if something is an expr or stmt in the JSON.
	switch (type)
	{
	case ExprType::IntConstant: return "IntConstantExpr";
	case ExprType::FloatConstant: return "FloatConstantExpr";
	case ExprType::Binary: return "BinaryExpr";
	case ExprType::Identifier: return "IdentifierExpr";
	default:
		ASSERT_NOT_REACHED();
		return "";
	}
}

Expr::Expr(size_t start, size_t end)
	: start(start)
	, length(end - start)
{}