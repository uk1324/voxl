#pragma once

#include <Ast.hpp>
#include <Allocator.hpp>
#include <ByteCode.hpp>
#include <ErrorReporter.hpp>
#include <unordered_map>

// To store the line numbers of the compiled code I could when calling compile on an expr or stmt store the line number of the start of it
// The I could emit the number as RLE or just normally.

// Could synchronize on global statements because even if an error happend It can try to compile further because globals are executed at runtime.

// TODO: Make a function compile block that would take a StmtList and beginScope compile and endScope.
// Remember too add the line numbers.

namespace Voxl
{

// If there are multiple nested scopes create it may be to allow variables with the same names as the previous scope to be
// redeclared.

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

	enum class ScopeType
	{
		Default,
		Try,
		Catch,
		Finally
	};

	struct Scope
	{
		std::unordered_map<std::string_view, Local> localVariables;
		int functionDepth;
		// Not sure if the scope type should be used in endScope to do things 
		ScopeType type;
		union
		{
			/*
			Recompiling the finally after a jump out of a block doesn't work because to generate the correct code the compiler 
			would need to always be in the same state when the code is compiled. If this is not done the code may be able 
			to access variables with the same names in different scopes. For example
			x : 0;
			try {
				throw 1;
			} catch * => x {
				break;
			} finally {
				put(x);
			}
			The finally will be compiled inside the catch so it will use the caught x instead of the outside x it should.
			Even in this simple example when the break is only inside the catch scope
			I haven't though much about it but I don't think there is an issue with anything else than variables so it may be possible 
			to use it in a language without scopes like python.
			Another implementation could be to compile finally as a closure and call it when needed.
			A better option would be to compile once and copy the code. With the current implementation this is easy because all jump
			are relative so no changes need to be made to the code.
			This could be further improved by only creating one copy of the code per type (break, return, etc.). To do this 
			jumps to the shared code blocks would need to be emmited at the statements.
			*/

			struct  
			{
				ByteCode* finallyBlockByteCode;
			} try_;
			struct
			{
				ByteCode* finallyBlockByteCode;
			} catch_;
		};
	};

	struct Loop
	{
		size_t loopStartLocation;
		// Scope index
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

	struct StatementExpression
	{
		int functionDepth;
	};

	enum class [[nodiscard]] Status
	{
		Ok,
		Error
	};

public:
	Compiler(Allocator& allocator);

	Result compile(const StmtList& ast, const SourceInfo& sourceInfo, ErrorReporter& errorReporter, std::optional<ObjModule*> = std::nullopt);

private:
	// Function is TOS.
	Status compileFunction(
		ObjFunction* function,
		const std::vector<std::string_view>& arguments, 
		const StmtList& stmts, 
		const SourceLocation& location);
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

	// [] -> [result]
	Status compile(const std::unique_ptr<Expr>& expr);
	Status compileBinaryExpr(const std::unique_ptr<Expr>& lhs, TokenType op, const std::unique_ptr<Expr>& rhs);
	// [lhs, rhs] -> [result].
	Status compileBinaryExpr(TokenType op);
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
	Status listExpr(const ListExpr& expr);
	Status dictExpr(const DictExpr& expr);
	Status getFieldExpr(const GetFieldExpr& expr);
	Status lambdaExpr(const LambdaExpr& expr);
	Status stmtExpr(const StmtExpr& expr);

	Status compile(const std::unique_ptr<Ptrn>& ptrn);
	Status classPtrn(const ClassPtrn& ptrn);
	Status exprPtrn(const ExprPtrn& ptrn);
	Status alwaysTruePtrn(const AlwaysTruePtrn& ptrn);

	// Expects the variable initializer to be on top of the stack.
	// [initializer] -> []
	Status createVariableImplementation(std::string_view name, const SourceLocation& location);
	Status createVariable(std::string_view name, const SourceLocation& location);
	Status createSpecialVariable(std::string_view name, const SourceLocation& location);
	// Could make a RAII class
	void beginScope(ScopeType scopeType = ScopeType::Default);
	void endScope();
	Scope& currentScope();
	void popOffLocals(const Scope& scope);
	void scopeCleanUp(const Scope& scope);
	Status cleanUpBeforeJumpingOutOfScope(const Scope& scope, bool popOffLocals);
	static bool canVariableBeCreatedAndAssigned(std::string_view name);
	Status variable(std::string_view name, bool trueIfLoadFalseIfSet);
	Status loadVariable(std::string_view name);
	Status setVariable(std::string_view name, const SourceLocation& location);
	// [value] -> [field]
	Status getField(std::string_view fieldName);
	Status loadConstant(size_t index);
	ByteCode& currentByteCode();
	void emitOp(Op op);
	Status emitOpArg(Op op, size_t arg, const SourceLocation& location);
	void emitUint8(uint8_t value);
	void emitUint32(uint32_t value);
	// Could make a class RAII class that reports an error if the jump was not set at the end of the scope.
	// This would be needed if the compiler could synchronize.
	size_t emitJump(Op op);
	void emitJump(Op op, size_t location);
	void setJumpToHere(size_t placeToPatch);
	void patch(size_t placeToPatch, uint32_t value);
	size_t currentLocation();
	int currentFunctionDepth();

	Status errorAt(const SourceLocation& location, const char* format, ...);

	// If only constants are allocated by the compiler the marking function probably isn't needed.
	static void mark(Compiler* compiler, Allocator& allocator);

public:
	Allocator& m_allocator;

	// If I needed to reduce allocation I could flatten these data structures. 
	// For example converting m_loops into multiple arrays. Each entry would store a depth.
	std::vector<Scope> m_scopes;
	std::vector<Loop> m_loops;
	std::vector<Function> m_functions;
	std::vector<StatementExpression> m_statementExpressions;

	// Stores the line numbers of the currently compiled things. Because expressions might be on a different lines
	// the the statements they are in a stack has to be used. 
	std::vector<size_t> m_lineNumberStack;

	std::vector<ByteCode*> m_functionByteCodeStack;


	ObjModule* m_module;
	Allocator::MarkingFunctionHandle m_rootMarkingFunctionHandle;

	bool m_hadError;
	ErrorReporter* m_errorReporter;
	const SourceInfo* m_sourceInfo;
};

}