#pragma once

#include <Lang/Ast/ExprVisitor.hpp>
#include <Lang/Ast/StmtVisitor.hpp>
#include <Lang/ErrorPrinter.hpp>

#include <vector>
#include <unordered_map>

namespace Lang
{
	class TypeChecker : public NonConstStmtVisitor, public NonConstExprVisitor
	{
	public:
		struct Result
		{
			bool hadError;
		};

	private:
		class Error
		{};

		class Variable
		{
		public:
			DataType dataType;
			bool isInitialized;
		};

		class Scope
		{
		public:
			std::unordered_map<Token, Variable> localVariables;
			Opt<Scope*> previousScope;
		};

	public:
		TypeChecker();

		Result checkAndInferTypes(std::vector<OwnPtr<Stmt>>& ast, ErrorPrinter& errorPrinter);

	private:
		void visitExprStmt(ExprStmt& stmt) override;
		void visitPrintStmt(PrintStmt& stmt) override;
		void visitLetStmt(LetStmt& stmt) override;

		void visitBinaryExpr(BinaryExpr& expr) override;
		void visitIntConstantExpr(IntConstantExpr& expr) override;
		void visitFloatConstantExpr(FloatConstantExpr& expr) override;
		void visitIdentifierExpr(IdentifierExpr& expr) override;

		void declareVariable(const Token& name, const Variable& variable);

		Error errorAt(size_t start, size_t end, const char* format, ...);
		Error errorAt(const Expr& expr, const char* format, ...);
		Error errorAt(const Stmt& stmt, const char* format, ...);
		void errorAtImplementation(size_t start, size_t end, const char* format, va_list args);

	private:
		std::unordered_map<Token, Variable> m_gobalVariables;
		Opt<Scope*> m_currentScope;

		ErrorPrinter* m_errorPrinter;

		bool m_hadError;
	};
}