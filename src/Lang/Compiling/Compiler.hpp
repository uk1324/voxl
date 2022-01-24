#pragma once

#include <Lang/Program.hpp>
#include <Lang/Ast/ExprVisitor.hpp>
#include <Lang/Ast/StmtVisitor.hpp>
#include <Lang/ErrorPrinter.hpp>

#include <unordered_map>

namespace Lang
{
	class Compiler : public StmtVisitor, public ExprVisitor
	{
	public:
		class Result
		{
		public:
			bool hadError;
			Program program;
		};

	private:
		struct Local
		{
		public:
			uint32_t index;
		};

		struct Global
		{
		public:
			uint32_t index;
		};

		struct Scope
		{
		public:
			std::unordered_map<Token, Local> localVariables;
			Opt<Scope*> previousScope;
		};

	public:
		Compiler();

		Result compile(const std::vector<OwnPtr<Stmt>>& ast, ErrorPrinter* errorPrinter);

		void visitExprStmt(const ExprStmt& stmt) override;
		void visitPrintStmt(const PrintStmt& stmt) override;
		void visitLetStmt(const LetStmt& stmt) override;

		void visitBinaryExpr(const BinaryExpr& expr) override;
		void visitIntConstantExpr(const IntConstantExpr& expr) override;
		void visitFloatConstantExpr(const FloatConstantExpr& expr) override;
		void visitIdentifierExpr(const IdentifierExpr& expr) override;

	private:
		uint32_t createConstant(Value value);
		void loadConstant(uint32_t index);
		void emitOp(Op op);
		void emitUint32(uint32_t value);

		void declareVariable(const Token& name);
		void loadVariable(const Token& name);

	private:
		std::unordered_map<Token, Global> m_gobalVariables;
		Opt<Scope*> m_currentScope;

		ErrorPrinter* m_errorPrinter;
		bool m_hadError;

		Program m_program;
	};
}