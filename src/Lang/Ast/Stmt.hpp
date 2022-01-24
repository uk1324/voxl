#pragma once

namespace Lang
{
	enum class StmtType
	{
		Expr,
		Print,
		VariableDeclaration,
	};

	const char* stmtTypeToString(StmtType type);

	class StmtVisitor;
	class NonConstStmtVisitor;

	class Stmt
	{
	public:
		Stmt(size_t start, size_t end);
		virtual ~Stmt() = default;

		virtual void accept(StmtVisitor& visitor) const = 0;
		virtual void accept(NonConstStmtVisitor& visitor) = 0;
		virtual StmtType type() const = 0;

	public:
		size_t start;
		size_t length;
	};
}