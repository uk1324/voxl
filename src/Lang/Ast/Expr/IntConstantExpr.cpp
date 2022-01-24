#include <Lang/Ast/Expr/IntConstantExpr.hpp>
#include <Lang/Ast/ExprVisitor.hpp>
#include <Utils/Assertions.hpp>

#include <charconv>

using namespace Lang;

IntConstantExpr::IntConstantExpr(Token token, size_t start, size_t end)
	: Expr(start, end)
{
	std::from_chars_result result = std::from_chars(token.text.data(), token.text.data() + token.text.length(), value);
	
	// Check if no error
	ASSERT(result.ec == std::errc()); 
	ASSERT(result.ptr == (token.text.data() + token.text.length()));
}

void IntConstantExpr::accept(ExprVisitor& visitor) const
{
	visitor.visitIntConstantExpr(*this);
}

void IntConstantExpr::accept(NonConstExprVisitor& visitor)
{
	visitor.visitIntConstantExpr(*this);
}

ExprType IntConstantExpr::type() const
{
	return ExprType::IntConstant;
}
