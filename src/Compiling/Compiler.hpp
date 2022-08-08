#pragma once

#include <Ast.hpp>
#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorPrinter.hpp>
#include <unordered_map>

// To store the line numbers of the compiled code I could when calling compile on an expr or stmt store the line number of the start of it
// The I could emit the number as RLE or just normally.

// Could synchronize on global statements because even if an error happend It can try to compile further because globals are executed at runtime.

// TODO: Make a function compile block that would take a StmtList and beginScopel compile and endScope.
// Remember too add the line numbers.

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
		ObjModule* module;
	};

private:
	struct Local
	{
		uint32_t index;
		bool isCaptured;
	};

	struct Scope
	{
		std::unordered_map<std::string_view, Local> localVariables;
		int functionDepth;
	};

	struct Loop
	{
		size_t loopStartLocation;
		size_t scopeDepth;
		std::vector<size_t> breakJumpLocations;
		std::vector<size_t> continueJumpLocations;
	};

	struct Upvalue
	{
		size_t index;
		bool isLocal;
	};

	struct Function
	{
		std::vector<Upvalue> upvalues;
	};

	enum class [[nodiscard]] Status
	{
		Ok,
		Error
	};

public:
	Compiler(Allocator& allocator);

	Result compile(const std::vector<std::unique_ptr<Stmt>>& ast, ErrorPrinter& errorPrinter);

private:
	Status compileFunction(
		ObjFunction* function,
		const std::vector<std::string_view>& arguments, 
		const StmtList& stmts, 
		size_t start, 
		size_t end);
	Status compile(const std::unique_ptr<Stmt>& stmt);
	Status compile(const std::vector<std::unique_ptr<Stmt>>& stmts);
	Status exprStmt(const ExprStmt& stmt);
	Status variableDeclarationStmt(const VariableDeclarationStmt& stmt);
	Status blockStmt(const BlockStmt& stmt);
	Status fnStmt(const FnStmt& stmt);
	Status retStmt(const RetStmt& stmt);
	Status ifStmt(const IfStmt& stmt);
	Status loopStmt(const LoopStmt& stmt);
	Status breakStmt(const BreakStmt& stmt);
	Status compileMethods(std::string_view className, const std::vector<std::unique_ptr<FnStmt>>& methods);
	Status classStmt(const ClassStmt& stmt);
	Status implStmt(const ImplStmt& stmt);
	Status tryStmt(const TryStmt& stmt);
	Status throwStmt(const ThrowStmt& stmt);
	Status matchStmt(const MatchStmt& stmt);
	Status loadModule(std::string_view filePath);
	Status useStmt(const UseStmt& stmt);
	Status useAllStmt(const UseAllStmt& stmt);
	Status useSelectiveStmt(const UseSelectiveStmt& stmt);

	Status compile(const std::unique_ptr<Expr>& expr);
	Status compileBinaryExpr(const std::unique_ptr<Expr>& lhs, TokenType op, const std::unique_ptr<Expr>& rhs);
	// TODO: perform constant folding
	Status intConstantExpr(const IntConstantExpr& expr);
	Status floatConstantExpr(const FloatConstantExpr& expr);
	Status boolConstantExpr(const BoolConstantExpr& expr);
	Status nullExpr(const NullExpr&);
	Status stringConstantExpr(const StringConstantExpr& expr);
	Status binaryExpr(const BinaryExpr& expr);
	Status unaryExpr(const UnaryExpr& expr);
	Status identifierExpr(const IdentifierExpr& expr);
	Status callExpr(const CallExpr& expr);
	Status assignmentExpr(const AssignmentExpr& expr);
	Status arrayExpr(const ArrayExpr& expr);
	Status getFieldExpr(const GetFieldExpr& expr);
	Status lambdaExpr(const LambdaExpr& expr);

	Status compile(const std::unique_ptr<Ptrn>& ptrn);
	Status classPtrn(const ClassPtrn& ptrn);

	// Expects the variable initializer to be on top of the stack.
	Status createVariable(std::string_view name, size_t start, size_t end);
	// Could make a RAII class
	void beginScope();
	void endScope();
	void popOffLocals(const Scope& scope);
	// TODO: Just make function for get and set that will call variable() or making a function resolveVariable
	// would be more complicated and require a new type that could be either global, local, orUpvalue and have an
	// index or a string.
	enum class VariableOp { Get, Set };
	Status variable(std::string_view name, VariableOp op);
	Status loadConstant(size_t index);
	ByteCode& currentByteCode();
	void emitOp(Op op);
	Status emitOpArg(Op op, size_t arg, size_t start, size_t end);
	void emitUint8(uint8_t value);
	void emitUint32(uint32_t value);
	size_t emitJump(Op op);
	void emitJump(Op op, size_t location);
	void setJumpToHere(size_t placeToPatch);
	void patch(size_t placeToPatch, uint32_t value);
	size_t currentLocation();

	Status errorAt(size_t start, size_t end, const char* format, ...);
	Status errorAt(const Stmt& stmt, const char* format, ...);
	Status errorAt(const Expr& expr, const char* format, ...);
	Status errorAt(const Token& token, const char* format, ...);

	// If only constants are allocated by the compiler the marking function probably isn't needed.
	static void mark(Compiler* compiler, Allocator& allocator);

public:
	Allocator& m_allocator;

	// If I needed to reduce allocation I could flatten these data structures. 
	// For example converting m_loops into multiple arrays. Each entry would store a depth.
	std::vector<Scope> m_scopes;
	std::vector<Loop> m_loops;
	std::vector<Function> m_functions;

	// Stores the line numbers of the currently compiled things. Because expressions might be on a different lines
	// the the statements they are in a stack has to be used. 
	std::vector<size_t> m_lineNumberStack;

	std::vector<ByteCode*> m_functionByteCodeStack;

	ObjModule* m_module;
	Allocator::MarkingFunctionHandle m_rootMarkingFunctionHandle;

	bool m_hadError;
	ErrorPrinter* m_errorPrinter;
};

}