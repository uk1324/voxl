#pragma once

#include <Ast.hpp>
#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorPrinter.hpp>
#include <unordered_map>

// To store the line numbers of the compiled code I could when calling compile on an expr or stmt store the line number of the start of it
// The I could emit the number as RLE or just normally.

// Could synchronize on global statements because even if an error happend It can try to compile further because globals are executed at runtime.

namespace Lang
{

class Compiler
{
public:
	class Result
	{
	public:
		bool hadError;
		ObjFunction* program;
	};

private:
	struct Local
	{
	public:
		uint32_t index;
	};

	struct Scope
	{
	public:
		std::unordered_map<std::string_view, Local> localVariables;
		int functionDepth;
	};

	enum class [[nodiscard]] Status
	{
		Ok,
		Error
	};

public:
	Compiler();

	Result compile(const std::vector<std::unique_ptr<Stmt>>& ast, ErrorPrinter& errorPrinter, Allocator& allocator);

private:
	Status compile(const std::unique_ptr<Stmt>& stmt);
	Status exprStmt(const ExprStmt& stmt);
	Status printStmt(const PrintStmt& stmt);
	Status letStmt(const LetStmt& stmt);
	Status blockStmt(const BlockStmt& stmt);
	Status fnStmt(const FnStmt& stmt);
	Status retStmt(const RetStmt& stmt);

	Status compile(const std::unique_ptr<Expr>& expr);
	// TODO: perform constant folding
	Status intConstantExpr(const IntConstantExpr& expr);
	Status boolConstantExpr(const BoolConstantExpr& expr);
	Status binaryExpr(const BinaryExpr& expr);
	Status unaryExpr(const UnaryExpr& expr);
	Status identifierExpr(const IdentifierExpr& expr);
	Status callExpr(const CallExpr& expr);

	Status declareVariable(std::string_view name, size_t start, size_t end);
	// Could make a RAII class
	void beginScope();
	void endScope();

	uint32_t createConstant(Value value);
	uint32_t createIdentifierConstant(const Token& name);
	void loadConstant(uint32_t index);
	ByteCode& currentByteCode();
	void emitOp(Op op);
	void emitUint32(uint32_t value);
	size_t emitJump(Op op);
	void patchJump(size_t placeToPatch);

	Status errorAt(size_t start, size_t end, const char* format, ...);
	Status errorAt(const Stmt& stmt, const char* format, ...);
	Status errorAt(const Expr& expr, const char* format, ...);
	Status errorAt(const Token& token, const char* format, ...);

private:
	Allocator* m_allocator;

	std::vector<Scope> m_scopes;

	// Stores the line numbers of the currently compiled things. Because expressions might be on a different lines
	// the the statements they are in a stack has to be used. 
	std::vector<size_t> m_lineNumberStack;

	std::vector<ByteCode*> m_functionByteCodeStack;

	bool m_hadError;
	ErrorPrinter* m_errorPrinter;
};

}