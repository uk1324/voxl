#include <Lang/Ast/Expr/FloatConstantExpr.hpp>
#include <Lang/Ast/ExprVisitor.hpp>
#include <Utils/Assertions.hpp>

#include <charconv>

using namespace Lang;

FloatConstantExpr::FloatConstantExpr(Token token, size_t start, size_t end)
	: Expr(start, end)
{
	std::from_chars_result result = std::from_chars(token.text.data(), token.text.data() + token.text.length(), value);

	// Check if no error
	ASSERT(result.ec == std::errc());
	ASSERT(result.ptr == (token.text.data() + token.text.length()));
}

void FloatConstantExpr::accept(ExprVisitor& visitor) const
{
	visitor.visitFloatConstantExpr(*this);
}

void FloatConstantExpr::accept(NonConstExprVisitor& visitor)
{
	visitor.visitFloatConstantExpr(*this);
}

ExprType FloatConstantExpr::type() const
{
	return ExprType::FloatConstant;
}
