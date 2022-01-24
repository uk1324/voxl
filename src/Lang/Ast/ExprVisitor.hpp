#pragma once

#include <Lang/Ast/Expr/BinaryExpr.hpp>
#include <Lang/Ast/Expr/IntConstantExpr.hpp>
#include <Lang/Ast/Expr/FloatConstantExpr.hpp>
#include <Lang/Ast/Expr/IdentifierExpr.hpp>

namespace Lang
{
	class ExprVisitor
	{
	public:
		virtual void visitBinaryExpr(const BinaryExpr& expr) = 0;
		virtual void visitIntConstantExpr(const IntConstantExpr& expr) = 0;
		virtual void visitFloatConstantExpr(const FloatConstantExpr& expr) = 0;
		virtual void visitIdentifierExpr(const IdentifierExpr& expr) = 0;
	};

	class NonConstExprVisitor
	{
	public:
		virtual void visitBinaryExpr(BinaryExpr& expr) = 0;
		virtual void visitIntConstantExpr(IntConstantExpr& expr) = 0;
		virtual void visitFloatConstantExpr(FloatConstantExpr& expr) = 0;
		virtual void visitIdentifierExpr(IdentifierExpr& expr) = 0;
	};
}