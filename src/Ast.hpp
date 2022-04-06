#pragma once

#include <Value.hpp>
#include <Parsing/Token.hpp>
#include <memory>
#include <vector>
#include <optional>

// If I were to store the token instead of just the string view I could easily print additional error information like
// no operator+ for types (not applicable to a dynamically typed language).
// (invalid use of) / (redefinition of) <variable name> previously defined <location>.

namespace Lang
{

enum class ExprType
{
	IntConstant,
	FloatConstant,
	BoolConstant,
	Binary,
	Unary,
	Identifier,
	Call
};

class Expr
{
public:
	Expr(size_t start, size_t end, ExprType type);
	virtual ~Expr() = default;

	size_t end() const;

	const size_t start;
	const size_t length;

	const ExprType type;
};

class IntConstantExpr final : public Expr
{
public:
	IntConstantExpr(Int value, size_t start, size_t end);
	~IntConstantExpr() = default;

	Int value;
};

class FloatConstantExpr final : public Expr
{
public:
	FloatConstantExpr(Float value, size_t start, size_t end);
	~FloatConstantExpr() = default;

	Float value;
};

class BoolConstantExpr final : public Expr
{
public:
	BoolConstantExpr(bool value, size_t start, size_t end);

	bool value;
};

class BinaryExpr final : public Expr
{
public:
	BinaryExpr(std::unique_ptr<Expr> lhs, TokenType op, std::unique_ptr<Expr> rhs, size_t start, size_t end);
	~BinaryExpr() = default;

	TokenType op;
	std::unique_ptr<Expr> lhs;
	std::unique_ptr<Expr> rhs;
};

class UnaryExpr final : public Expr
{
public:
	UnaryExpr(std::unique_ptr<Expr> expr, TokenType op, size_t start, size_t end);
	
	TokenType op;
	std::unique_ptr<Expr> expr;
};

class IdentifierExpr final : public Expr
{
public:
	IdentifierExpr(std::string_view identifier, size_t start, size_t end);
	~IdentifierExpr() = default;

	std::string_view identifier;
};

class CallExpr final : public Expr
{
public:
	CallExpr(std::unique_ptr<Expr> calle, std::vector<std::unique_ptr<Expr>> arguments, size_t start, size_t end);
	~CallExpr() = default;

	std::unique_ptr<Expr> calle;
	std::vector<std::unique_ptr<Expr>> arguments;
};

enum class StmtType
{
	Expr,
	Print,
	Let,
	Block,
	Fn,
	Ret,
	If,
};

class Stmt
{
public:
	Stmt(size_t start, size_t end, StmtType type);
	virtual ~Stmt() = default;
	size_t end() const;

	const size_t start;
	const size_t length;

	const StmtType type;
};

class ExprStmt final : public Stmt
{
public:
	ExprStmt(std::unique_ptr<Expr> expr, size_t start, size_t end);
	~ExprStmt() = default;

	std::unique_ptr<Expr> expr;
};

class PrintStmt final : public Stmt
{
public:
	PrintStmt(std::unique_ptr<Expr> expr, size_t start, size_t end);
	~PrintStmt() = default;

	std::unique_ptr<Expr> expr;
};

class LetStmt final : public Stmt
{
public:
	LetStmt(std::string_view identifier, std::optional<std::unique_ptr<Expr>> initializer, size_t start, size_t end);
	~LetStmt() = default;

	std::string_view identifier;
	std::optional<std::unique_ptr<Expr>> initializer;
};

class BlockStmt final : public Stmt
{
public:
	BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts, size_t start, size_t end);
	~BlockStmt() = default;

	std::vector<std::unique_ptr<Stmt>> stmts;
};

class FnStmt final : public Stmt
{
public:
	FnStmt(
		std::string_view name,
		std::vector<std::string_view> arguments,
		std::vector<std::unique_ptr<Stmt>> stmts,
		size_t start,
		size_t end);

	std::string_view name;
	std::vector<std::string_view> arguments;
	std::vector<std::unique_ptr<Stmt>> stmts;
};

class RetStmt final : public Stmt
{
public:
	RetStmt(std::optional<std::unique_ptr<Expr>> returnValue, size_t start, size_t end);

	std::optional<std::unique_ptr<Expr>> returnValue;
};

class IfStmt final : public Stmt
{
public:
	IfStmt(
		std::unique_ptr<Expr> condition,
		std::vector<std::unique_ptr<Stmt>> ifThen,
		std::optional<std::unique_ptr<Stmt>> elseThen,
		size_t start,
		size_t end);

	std::unique_ptr<Expr> condition;
	std::vector<std::unique_ptr<Stmt>> ifThen;
	std::optional<std::unique_ptr<Stmt>> elseThen;
};

}