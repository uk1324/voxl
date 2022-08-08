#include <Ast.hpp>

using namespace Lang;

Expr::Expr(size_t start, size_t end, ExprType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

size_t Expr::end() const
{
	return start + length;
}

Stmt::Stmt(size_t start, size_t end, StmtType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

size_t Stmt::end() const
{
	return start + length;
}

Ptrn::Ptrn(size_t start, size_t end, PtrnType type)
	: start(start)
	, length(end - start)
	, type(type)
{}

size_t Ptrn::end() const
{
	return start + length;
}

IntConstantExpr::IntConstantExpr(Int value, size_t start, size_t end)
	: Expr(start, end, ExprType::IntConstant)
	, value(value)
{}

FloatConstantExpr::FloatConstantExpr(Float value, size_t start, size_t end)
	: Expr(start, end, ExprType::FloatConstant)
	, value(value)
{}

BoolConstantExpr::BoolConstantExpr(bool value, size_t start, size_t end)
	: Expr(start, end, ExprType::BoolConstant)
	, value(value)
{}

NullExpr::NullExpr(size_t start, size_t end)
	: Expr(start, end, ExprType::Null)
{};

StringConstantExpr::StringConstantExpr(std::string_view text, size_t length, size_t start, size_t end)
	: Expr(start, end, ExprType::StringConstant)
	, text(text)
	, length(length)
{}

BinaryExpr::BinaryExpr(std::unique_ptr<Expr> lhs, TokenType op, std::unique_ptr<Expr> rhs, size_t start, size_t end)
	: Expr(start, end, ExprType::Binary)
	, lhs(std::move(lhs))
	, op(op)
	, rhs(std::move(rhs))
{}

UnaryExpr::UnaryExpr(std::unique_ptr<Expr> expr, TokenType op, size_t start, size_t end)
	: Expr(start, end, ExprType::Unary)
	, expr(std::move(expr))
	, op(op)
{}

IdentifierExpr::IdentifierExpr(std::string_view identifier, size_t start, size_t end)
	: Expr(start, end, ExprType::Identifier)
	, identifier(identifier)
{}

CallExpr::CallExpr(std::unique_ptr<Expr> calle, std::vector<std::unique_ptr<Expr>> arguments, size_t start, size_t end)
	: Expr(start, end, ExprType::Call)
	, calle(std::move(calle))
	, arguments(std::move(arguments))
{}

AssignmentExpr::AssignmentExpr(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, std::optional<TokenType> op, size_t start, size_t end)
	: Expr(start, end, ExprType::Assignment)
	, lhs(std::move(lhs))
	, rhs(std::move(rhs))
	, op(op)
{}

GetFieldExpr::GetFieldExpr(std::unique_ptr<Expr> lhs, std::string_view fieldName, size_t start, size_t end)
	: Expr(start, end, ExprType::GetField)
	, lhs(std::move(lhs))
	, fieldName(fieldName)
{}

ArrayExpr::ArrayExpr(std::vector<std::unique_ptr<Expr>> values, size_t start, size_t end)
	: Expr(start, end, ExprType::Array)
	, values(std::move(values))
{}

LambdaExpr::LambdaExpr(std::vector<std::string_view> arguments, StmtList stmts, size_t start, size_t end)
	: Expr(start, end, ExprType::Lambda)
	, arguments(std::move(arguments))
	, stmts(std::move(stmts))
{}

ExprStmt::ExprStmt(std::unique_ptr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end, StmtType::Expr)
	, expr(std::move(expr))
{}

VariableDeclarationStmt::VariableDeclarationStmt(
	std::vector<std::pair<std::string_view, std::optional<std::unique_ptr<Expr>>>> variables,
	size_t start,
	size_t end)
	: Stmt(start, end, StmtType::VariableDeclaration)
	, variables(std::move(variables))
{}

BlockStmt::BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts, size_t start, size_t end)
	: Stmt(start, end, StmtType::Block)
	, stmts(std::move(stmts))
{}

FnStmt::FnStmt(
	std::string_view name,
	std::vector<std::string_view> arguments,
	std::vector<std::unique_ptr<Stmt>> stmts,
	size_t start,
	size_t end)
	: Stmt(start, end, StmtType::Fn)
	, name(name)
	, arguments(std::move(arguments))
	, stmts(std::move(stmts))
{}

RetStmt::RetStmt(std::optional<std::unique_ptr<Expr>> returnValue, size_t start, size_t end)
	: Stmt(start, end, StmtType::Ret)
	, returnValue(std::move(returnValue))
{}

IfStmt::IfStmt(
	std::unique_ptr<Expr> condition,
	std::vector<std::unique_ptr<Stmt>> ifThen,
	std::optional<std::unique_ptr<Stmt>> elseThen,
	size_t start,
	size_t end)
	: Stmt(start, end, StmtType::If)
	, condition(std::move(condition))
	, ifThen(std::move(ifThen))
	, elseThen(std::move(elseThen))
{}

LoopStmt::LoopStmt(
	std::optional<std::unique_ptr<Stmt>> initStmt,
	std::optional<std::unique_ptr<Expr>> condition,
	std::optional<std::unique_ptr<Expr>> iterationExpr,
	std::vector<std::unique_ptr<Stmt>> block,
	size_t start,
	size_t end)
	: Stmt(start, end, StmtType::Loop)
	, initStmt(std::move(initStmt))
	, condition(std::move(condition))
	, iterationExpr(std::move(iterationExpr))
	, block(std::move(block))
{}

BreakStmt::BreakStmt(size_t start, size_t end)
	: Stmt(start, end, StmtType::Break)
{}

ClassStmt::ClassStmt(std::string_view name, std::vector<std::unique_ptr<FnStmt>> methods, size_t start, size_t end)
	: Stmt(start, end, StmtType::Class)
	, name(name)
	, methods(std::move(methods))
{}

ImplStmt::ImplStmt(std::string_view typeName, std::vector<std::unique_ptr<FnStmt>> methods, size_t start, size_t end)
	: Stmt(start, end, StmtType::Impl)
	, typeName(typeName)
	, methods(std::move(methods))
{}

TryStmt::TryStmt(
	StmtList tryBlock,
	std::vector<CatchBlock> catchBlocks,
	std::optional<StmtList> finallyBlock,
	size_t start,
	size_t end)
	: Stmt(start, end, StmtType::Try)
	, tryBlock(std::move(tryBlock))
	, catchBlocks(std::move(catchBlocks))
	, finallyBlock(std::move(finallyBlock))
{}

ThrowStmt::ThrowStmt(std::unique_ptr<Expr> expr, size_t start, size_t end)
	: Stmt(start, end, StmtType::Throw)
	, expr(std::move(expr))
{}

MatchStmt::MatchStmt(std::unique_ptr<Expr> expr, std::vector<Pair> cases, size_t start, size_t end)
	: Stmt(start, end, StmtType::Match)
	, expr(std::move(expr))
	, cases(std::move(cases))
{}

UseStmt::UseStmt(std::string_view path, std::optional<std::string_view> variableName, size_t start, size_t end)
	: Stmt(start, end, StmtType::Use)
	, path(path)
	, variableName(variableName)
{}

UseAllStmt::UseAllStmt(std::string_view path, size_t start, size_t end)
	: Stmt(start, end, StmtType::UseAll)
	, path(path)
{}

UseSelectiveStmt::UseSelectiveStmt(std::string_view path, std::vector<Variable> variablesToImport, size_t start, size_t end)
	: Stmt(start, end, StmtType::UseSelective)
	, path(path)
	, variablesToImport(std::move(variablesToImport))
{}

ClassPtrn::ClassPtrn(std::string_view className, size_t start, size_t end)
	: Ptrn(start, end, PtrnType::Class)
	, className(className)
{}
