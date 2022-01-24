#pragma once

#include <Lang/Ast/ExprVisitor.hpp>
#include <Lang/Ast/StmtVisitor.hpp>
#include <Lang/TypeChecking/DataType.hpp>
#include <Json/Json.hpp>

namespace Lang
{
	class AstJsonifier : public ExprVisitor, public StmtVisitor
	{
	public:
		Json::Value jsonify(const std::vector<OwnPtr<Stmt>>& ast);

		void visitBinaryExpr(const BinaryExpr& expr) override;
		void visitIntConstantExpr(const IntConstantExpr& expr) override;
		void visitFloatConstantExpr(const FloatConstantExpr& expr) override;
		void visitIdentifierExpr(const IdentifierExpr& expr) override;

		void visitExprStmt(const ExprStmt& stmt) override;
		void visitPrintStmt(const PrintStmt& stmt) override;
		void visitLetStmt(const LetStmt& stmt) override;

	private:
		void toJson(const OwnPtr<Expr>& expr);
		void toJson(const OwnPtr<Stmt>& stmt);
		void toJson(const DataType& type);

		Json::Value dataTypeToJson(const DataType& type);

		void addCommon(Json::Value& node, const Expr& expr);
		void addCommon(Json::Value& node, const Stmt& stmt);

	private:
		// Templated functions can't be virtual so I have to use this for return values.
		// I could use std::any.
		Json::Value m_returnValue; // Return value for toJson().
	};
}