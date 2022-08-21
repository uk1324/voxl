#pragma once

#include <Value.hpp>
#include <Parsing/Token.hpp>
#include <memory>
#include <vector>
#include <optional>

// If I were to store the token instead of just the string view I could easily print additional error information like
// no operator+ for types (not applicable to a dynamically typed language).
// (invalid use of) / (redefinition of) <variable name> previously defined <location>.

namespace Voxl
{

enum class ExprType
{
	IntConstant,
	FloatConstant,
	Binary,
	BoolConstant,
	Null,
	StringConstant,
	Unary,
	Identifier,
	Call,
	Assignment,
	GetField,
	List,
	Dict,
	Lambda,
	Stmt,
};

struct Expr
{
	Expr(size_t start, size_t end, ExprType type);
	virtual ~Expr() = default;

	SourceLocation location() const;
	size_t end() const;

	const size_t start;
	const size_t length;
	const ExprType type;
};

enum class StmtType
{
	Expr,
	VariableDeclaration,
	Block,
	Fn,
	Ret,
	If,
	Loop,
	Break,
	Class,
	Impl,
	Try,
	Throw,
	Match,

	// There isn't a nice way to store all the possiblites in one type.
	// Could use sum types but that would take up more space.
	// Can't use inheritance without adding special constructors that take in the StmtType.
	Use,
	UseAll,
	UseSelective,
};

struct Stmt
{
	Stmt(size_t start, size_t end, StmtType type);
	virtual ~Stmt() = default;

	SourceLocation location() const;
	size_t end() const;

	const size_t start;
	const size_t length;
	const StmtType type;
};

using StmtList = std::vector<std::unique_ptr<Stmt>>;

enum class PtrnType
{
	Class,
	Expr,
	AlwaysTrue,
};

struct Ptrn
{
	Ptrn(size_t start, size_t end, PtrnType type);
	virtual ~Ptrn() = default;

	size_t end() const;
	SourceLocation location() const;

	const size_t start;
	const size_t length;
	const PtrnType type;
};

struct IntConstantExpr final : public Expr
{
	IntConstantExpr(Int value, size_t start, size_t end);

	Int value;
};

struct FloatConstantExpr final : public Expr
{
	FloatConstantExpr(Float value, size_t start, size_t end);

	Float value;
};

struct BoolConstantExpr final : public Expr
{
	BoolConstantExpr(bool value, size_t start, size_t end);

	bool value;
};

struct NullExpr final : public Expr
{
	NullExpr(size_t start, size_t end);
};

struct StringConstantExpr final : public Expr
{
	StringConstantExpr(std::string_view text, size_t length, size_t start, size_t end);

	std::string_view text;
	size_t length;
};

struct BinaryExpr final : public Expr
{
	BinaryExpr(std::unique_ptr<Expr> lhs, TokenType op, std::unique_ptr<Expr> rhs, size_t start, size_t end);

	TokenType op;
	std::unique_ptr<Expr> lhs;
	std::unique_ptr<Expr> rhs;
};

struct UnaryExpr final : public Expr
{
	UnaryExpr(std::unique_ptr<Expr> expr, TokenType op, size_t start, size_t end);
	
	TokenType op;
	std::unique_ptr<Expr> expr;
};

struct IdentifierExpr final : public Expr
{
	IdentifierExpr(std::string_view identifier, size_t start, size_t end);

	std::string_view identifier;
};

struct CallExpr final : public Expr
{
	CallExpr(std::unique_ptr<Expr> calle, std::vector<std::unique_ptr<Expr>> arguments, size_t start, size_t end);

	std::unique_ptr<Expr> calle;
	std::vector<std::unique_ptr<Expr>> arguments;
};

struct AssignmentExpr final : public Expr
{
	AssignmentExpr(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, std::optional<TokenType> op, size_t start, size_t end);

	std::unique_ptr<Expr> lhs;
	std::unique_ptr<Expr> rhs;
	std::optional<TokenType> op;
};

struct GetFieldExpr final : public Expr
{
public:
	GetFieldExpr(std::unique_ptr<Expr> lhs, std::string_view fieldName, size_t start, size_t end);
	~GetFieldExpr() = default;

	std::unique_ptr<Expr> lhs;
	std::string_view fieldName;
};

struct ListExpr final : public Expr
{
	ListExpr(std::vector<std::unique_ptr<Expr>> values, size_t start, size_t end);

	std::vector<std::unique_ptr<Expr>> values;
};

struct DictExpr final : public Expr
{
	struct Pair
	{
		std::unique_ptr<Expr> key;
		std::unique_ptr<Expr> value;
	};

	DictExpr(std::vector<Pair> values, size_t start, size_t end);

	std::vector<Pair> values;
};

struct LambdaExpr final : public Expr
{
	LambdaExpr(std::vector<std::string_view> arguments, StmtList stmts, size_t start, size_t end);

	std::vector<std::string_view> arguments;
	StmtList stmts;
};

struct StmtExpr final : public Expr
{
	StmtExpr(std::unique_ptr<Stmt> stmt, size_t start, size_t end);

	std::unique_ptr<Stmt> stmt;
};

struct ExprStmt final : public Stmt
{
public:
	ExprStmt(std::unique_ptr<Expr> expr, size_t start, size_t end);

	std::unique_ptr<Expr> expr;
};

struct VariableDeclarationStmt final : public Stmt
{
public:
	VariableDeclarationStmt(
		std::vector<std::pair<std::string_view, std::optional<std::unique_ptr<Expr>>>> variables, 
		size_t start, 
		size_t end);

	std::vector<std::pair<std::string_view, std::optional<std::unique_ptr<Expr>>>> variables;
};

struct BlockStmt final : public Stmt
{
public:
	BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts, size_t start, size_t end);

	std::vector<std::unique_ptr<Stmt>> stmts;
};

struct FnStmt final : public Stmt
{
	/*struct Argument
	{
		std::string_view name;
		std::unique_ptr<Expr> constantExpr;
	};*/

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

struct RetStmt final : public Stmt
{
	RetStmt(std::optional<std::unique_ptr<Expr>> returnValue, size_t start, size_t end);

	std::optional<std::unique_ptr<Expr>> returnValue;
};

struct IfStmt final : public Stmt
{
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

struct LoopStmt final : public Stmt
{
	LoopStmt(
		std::optional<std::unique_ptr<Stmt>> initStmt,
		std::optional<std::unique_ptr<Expr>> condition,
		std::optional<std::unique_ptr<Expr>> iterationExpr,
		std::vector<std::unique_ptr<Stmt>> block,
		size_t start,
		size_t end);

	std::optional<std::unique_ptr<Stmt>> initStmt;
	std::optional<std::unique_ptr<Expr>> condition;
	std::optional<std::unique_ptr<Expr>> iterationExpr;
	std::vector<std::unique_ptr<Stmt>> block;
};

struct BreakStmt final : public Stmt
{
	BreakStmt(size_t start, size_t end);
};

struct ClassStmt final : public Stmt
{
	ClassStmt(
		std::string_view name, 
		std::optional<std::string_view> superclassName,
		std::vector<std::unique_ptr<FnStmt>> methods, 
		size_t start, 
		size_t end);

	std::string_view name;
	std::optional<std::string_view> superclassName;
	std::vector<std::unique_ptr<FnStmt>> methods;
};

struct ImplStmt final : public Stmt
{
	ImplStmt(std::string_view typeName, std::vector<std::unique_ptr<FnStmt>> methods, size_t start, size_t end);

	std::string_view typeName;
	std::vector<std::unique_ptr<FnStmt>> methods;
};

struct TryStmt final : public Stmt
{
	struct CatchBlock
	{
		std::unique_ptr<Ptrn> pattern;
		std::optional<std::string_view> caughtValueName;
		StmtList block;
	};

	TryStmt(
		StmtList tryBlock,
		std::vector<CatchBlock> catchBlocks,
		std::optional<StmtList> finallyBlock,
		size_t start,
		size_t end);

	StmtList tryBlock; 
	std::vector<CatchBlock> catchBlocks;
	std::optional<StmtList> finallyBlock;
};

struct ThrowStmt final : public Stmt
{
	ThrowStmt(std::unique_ptr<Expr> expr, size_t start, size_t end);

	std::unique_ptr<Expr> expr;
};

struct MatchStmt final : public Stmt 
{
	struct Pair
	{
		std::unique_ptr<Ptrn> pattern;
		std::unique_ptr<Stmt> stmt;
	};

	MatchStmt(std::unique_ptr<Expr> expr, std::vector<Pair> cases, size_t start, size_t end);

	std::unique_ptr<Expr> expr;
	std::vector<Pair> cases;
};

struct UseStmt final : public Stmt
{
	UseStmt(std::string_view path, std::optional<std::string_view>, size_t start, size_t end);

	std::string_view path;
	// Could find the import path stem in the parser and then this wouldn't need to use optional but that seems like
	// something the compiler should do.
	// If I were to use std::filesystem::path::stem() then it would also require multiple pointless allocations.
	std::optional<std::string_view> variableName;
};

struct UseAllStmt final : public Stmt
{
	UseAllStmt(std::string_view path, size_t start, size_t end);

	std::string_view path;
};

struct UseSelectiveStmt final : public Stmt
{
	struct Variable
	{
		std::string_view originalName;
		std::optional<std::string_view> newName;
	};

	UseSelectiveStmt(std::string_view path, std::vector<Variable> variablesToImport, size_t start, size_t end);

	std::string_view path;
	std::vector<Variable> variablesToImport;
};

struct ClassPtrn final : public Ptrn
{
	struct FieldPtrn
	{
		std::string_view name;
		std::unique_ptr<Ptrn> ptrn;
	};
	ClassPtrn(std::string_view className, size_t start, size_t end);
	ClassPtrn(std::string_view className, std::vector<FieldPtrn>, size_t start, size_t end);

	std::string_view className;
	std::vector<FieldPtrn> fieldPtrns;
};

struct ExprPtrn final : public Ptrn
{
	ExprPtrn(std::unique_ptr<Expr> expr, size_t start, size_t end);

	std::unique_ptr<Expr> expr;
};

struct AlwaysTruePtrn final : public Ptrn
{
	AlwaysTruePtrn(size_t start, size_t end);
};

}
