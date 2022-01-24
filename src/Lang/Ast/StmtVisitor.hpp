#pragma once

#include <Lang/Ast/Stmt/ExprStmt.hpp>
#include <Lang/Ast/Stmt/PrintStmt.hpp>
#include <Lang/Ast/Stmt/LetStmt.hpp>

namespace Lang
{
	class StmtVisitor
	{
	public:
		 virtual void visitExprStmt(const ExprStmt& stmt) = 0;
		 virtual void visitPrintStmt(const PrintStmt& stmt) = 0;
		 virtual void visitLetStmt(const LetStmt& stmt) = 0;
	};

	class NonConstStmtVisitor
	{
	public:
		virtual void visitExprStmt(ExprStmt& stmt) = 0;
		virtual void visitPrintStmt(PrintStmt& stmt) = 0;
		virtual void visitLetStmt(LetStmt& stmt) = 0;
	};
}