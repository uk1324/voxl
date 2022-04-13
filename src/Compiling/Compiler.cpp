#include "Compiler.hpp"
#include <Compiling/Compiler.hpp>
#include <Asserts.hpp>
#include <iostream>

#define TRY(somethingThatReturnsStatus) \
	do \
	{ \
		if ((somethingThatReturnsStatus) == Status::Error) \
		{ \
			return Status::Error; \
		} \
	} while (false)

using namespace Lang;

Compiler::Compiler()
	: m_hadError(false)
	, m_allocator(nullptr)
	, m_errorPrinter(nullptr)
{}

Compiler::Result Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& ast, ErrorPrinter& errorPrinter, Allocator& allocator)
{
	m_hadError = false;
	m_errorPrinter = &errorPrinter;
	m_allocator = &allocator;

	auto scriptName = m_allocator->allocateString("script");
	auto scriptFunction = m_allocator->allocateFunction(scriptName, 0);
	m_functionByteCodeStack.push_back(&scriptFunction->byteCode);

	for (const auto& stmt : ast)
	{
		if (compile(stmt) == Status::Error)
		{
			break;
		}
	}

	// The return from program is on the last line of the file.
	m_lineNumberStack.push_back(m_errorPrinter->sourceInfo().lineStartOffsets.size());
	emitOp(Op::LoadNull);
	emitOp(Op::Return);
	m_lineNumberStack.pop_back();

	m_functionByteCodeStack.pop_back();

	return Result{ m_hadError, reinterpret_cast<ObjFunction*>(scriptFunction) };
}

Compiler::Status Compiler::compile(const std::unique_ptr<Stmt>& stmt)
{
#define CASE_STMT_TYPE(stmtType, stmtFunction) \
	case StmtType::stmtType: TRY(stmtFunction(*static_cast<stmtType##Stmt*>(stmt.get()))); break;

	m_lineNumberStack.push_back(m_errorPrinter->sourceInfo().getLine(stmt->start));
	switch (stmt.get()->type)
	{
		CASE_STMT_TYPE(Expr, exprStmt)
		CASE_STMT_TYPE(Print, printStmt)
		CASE_STMT_TYPE(Let, letStmt)
		CASE_STMT_TYPE(Block, blockStmt)
		CASE_STMT_TYPE(Fn, fnStmt)
		CASE_STMT_TYPE(Ret, retStmt)
		CASE_STMT_TYPE(If, ifStmt)
		CASE_STMT_TYPE(Loop, loopStmt)
		CASE_STMT_TYPE(Break, breakStmt)
	}
	m_lineNumberStack.pop_back();

	return Status::Ok;
}

Compiler::Status Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& stmts)
{
	for (const auto& stmt : stmts)
	{
		TRY(compile(stmt));
	}
	return Status::Ok;
}

Compiler::Status Compiler::exprStmt(const ExprStmt& stmt)
{
	TRY(compile(stmt.expr));
	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::printStmt(const PrintStmt& stmt)
{
	TRY(compile(stmt.expr));
	emitOp(Op::Print);
	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::letStmt(const LetStmt& stmt)
{
	// TODO: This is a bad explanation. Fix it.
	// Only the let statement leaves a side effect on the stack by leaving the value of the variable on top of the stack.
	if (stmt.initializer != std::nullopt)
	{
		TRY(compile(*stmt.initializer));
	}
	else
	{
		emitOp(Op::LoadNull);
	}
	// The initializer is evaluated before declaring the variable so a variable from an oter scope with the same name can be used.
	TRY(declareVariable(stmt.identifier, stmt.start, stmt.end()));
	if (m_scopes.size() == 0)
	{
		auto constant = createConstant(Value(reinterpret_cast<Obj*>(m_allocator->allocateString(stmt.identifier))));
		loadConstant(constant);
		emitOp(Op::CreateGlobal);
	}

	return Status::Ok;
}

#include <Debug/Disassembler.hpp>

Compiler::Status Compiler::blockStmt(const BlockStmt& stmt)
{
	beginScope();
	TRY(compile(stmt.stmts));
	endScope();

	return Status::Ok;
}

Compiler::Status Compiler::fnStmt(const FnStmt& stmt)
{
	if ((m_scopes.size() > 0) && (m_scopes.back().functionDepth > 0))
		return errorAt(stmt, "nested functions not allowed"); // declareVariable(stmt.name, stmt.start, stmt.end());

	TRY(declareVariable(stmt.name, stmt.start, stmt.end()));

	auto name = m_allocator->allocateString(stmt.name);
	auto function = m_allocator->allocateFunction(name, static_cast<int>(stmt.arguments.size()));
	auto functionConstant = createConstant(Value(reinterpret_cast<Obj*>(function)));
	loadConstant(functionConstant);
	auto nameConstant = createConstant(Value(reinterpret_cast<Obj*>(name)));
	loadConstant(nameConstant);
	emitOp(Op::CreateGlobal);
	m_functionByteCodeStack.push_back(&function->byteCode);

	beginScope();
	m_scopes.back().functionDepth++;

	for (const auto& argument : stmt.arguments)
	{
		// Could store the token for better error messages about redeclaration.
		TRY(declareVariable(argument, stmt.start, stmt.end()));
	}

	TRY(compile(stmt.stmts));
	emitOp(Op::LoadNull);
	emitOp(Op::Return);

	std::cout << "----" << stmt.name << '\n';
	disassembleByteCode(function->byteCode);

	m_functionByteCodeStack.pop_back();

	endScope();
	return Status::Ok;
}

Compiler::Status Compiler::retStmt(const RetStmt& stmt)
{
	if (stmt.returnValue == std::nullopt)
	{
		emitOp(Op::LoadNull);
	}
	else
	{
		TRY(compile(*stmt.returnValue));
	}
	emitOp(Op::Return);
	return Status::Ok;
}

Compiler::Status Compiler::ifStmt(const IfStmt& stmt)
{
	TRY(compile(stmt.condition));

	auto jumpToElse = emitJump(Op::JumpIfFalse);

	TRY(compile(stmt.ifThen));

	size_t jumpToEndOfElse = 0;
	if (stmt.elseThen.has_value())
	{
		jumpToEndOfElse = emitJump(Op::Jump);
	}

	setJumpToHere(jumpToElse);

	if (stmt.elseThen.has_value())
	{
		TRY(compile(*stmt.elseThen));
		setJumpToHere(jumpToEndOfElse);
	}

	emitOp(Op::PopStack); // Pop the condition.

	return Status::Ok;
}

Compiler::Status Compiler::loopStmt(const LoopStmt& stmt)
{
	auto beginning = currentLocation();
	m_loops.push_back(Loop{ beginning });
	size_t jumpToEnd = 0; // Won't be used if there is no condition.
	if (stmt.condition.has_value())
	{
		TRY(compile(*stmt.condition));
		jumpToEnd = emitJump(Op::JumpIfFalse);
	}

	TRY(compile(stmt.block));

	emitJump(Op::JumpBack, beginning);

	if (stmt.condition.has_value())
	{
		setJumpToHere(jumpToEnd);
	}

	const auto& loop = m_loops.back();
	for (const auto& location : loop.breakJumpLocations)
	{
		setJumpToHere(location);
	}

	m_loops.pop_back();

	return Status::Ok;
}

Compiler::Status Compiler::breakStmt(const BreakStmt& stmt)
{
	if (m_loops.empty())
	{
		return errorAt(stmt, "cannot use break outside of a loop");
	}
	auto jump = emitJump(Op::Jump);
	m_loops.back().breakJumpLocations.push_back(jump);
	return Status::Ok;
}

Compiler::Status Compiler::compile(const std::unique_ptr<Expr>& expr)
{
#define CASE_EXPR_TYPE(exprType, exprFunction) \
	case ExprType::exprType: TRY(exprFunction(*static_cast<exprType##Expr*>(expr.get()))); break;

	m_lineNumberStack.push_back(m_errorPrinter->sourceInfo().getLine(expr->start));
	switch (expr.get()->type)
	{
		CASE_EXPR_TYPE(IntConstant, intConstantExpr)
		CASE_EXPR_TYPE(BoolConstant, boolConstantExpr)
		CASE_EXPR_TYPE(Binary, binaryExpr)
		CASE_EXPR_TYPE(StringConstant, stringConstantExpr)
		CASE_EXPR_TYPE(Unary, unaryExpr)
		CASE_EXPR_TYPE(Identifier, identifierExpr)
		CASE_EXPR_TYPE(Call, callExpr)
		CASE_EXPR_TYPE(Assignment, assignmentExpr)
	}
	m_lineNumberStack.pop_back();

	return Status::Ok;
}

Compiler::Status Compiler::intConstantExpr(const IntConstantExpr& expr)
{
	// TODO: Search if constant already exists.
	auto constant = createConstant(Value(expr.value));
	loadConstant(constant);
	return Status::Ok;
}

Compiler::Status Compiler::boolConstantExpr(const BoolConstantExpr& expr)
{
	if (expr.value)
	{
		emitOp(Op::LoadTrue);
	}
	else
	{
		emitOp(Op::LoadFalse);
	}
	return Status::Ok;
}

Compiler::Status Compiler::stringConstantExpr(const StringConstantExpr& expr)
{
	auto constant = createConstant(Value(reinterpret_cast<Obj*>(m_allocator->allocateString(expr.text))));
	loadConstant(constant);
	return Status::Ok;
}

Compiler::Status Compiler::unaryExpr(const UnaryExpr& expr)
{
	switch (expr.op)
	{
		case TokenType::Minus:
			TRY(compile(expr.expr));
			emitOp(Op::Negate);
			break;

		case TokenType::Not:
			TRY(compile(expr.expr));
			emitOp(Op::Not);
			break;

		default:
			ASSERT_NOT_REACHED();
			return Status::Error;
	}

	return Status::Ok;
}

Compiler::Status Compiler::identifierExpr(const IdentifierExpr& expr)
{
	for (auto it = m_scopes.crbegin(); it != m_scopes.crend(); it++)
	{
		const auto& scope = *it;
		auto local = scope.localVariables.find(expr.identifier);
		if (local != scope.localVariables.end())
		{
			emitOp(Op::LoadLocal);
			emitUint32(local->second.index);
			return Status::Ok;
		}
	}

	auto constant = createConstant(Value(reinterpret_cast<Obj*>(m_allocator->allocateString(expr.identifier))));
	loadConstant(constant);
	emitOp(Op::LoadGlobal);

	return Status::Ok;
}

Compiler::Status Compiler::assignmentExpr(const AssignmentExpr& expr)
{
	TRY(compile(expr.rhs));

	if (expr.lhs->type != ExprType::Identifier)
	{
		return errorAt(expr, "left hand side of assignment must be an identifier");
	}
	const auto lhs = static_cast<IdentifierExpr&>(*expr.lhs.get()).identifier;

	for (auto it = m_scopes.crbegin(); it != m_scopes.crend(); it++)
	{
		const auto& scope = *it;
		auto local = scope.localVariables.find(lhs);
		if (local != scope.localVariables.end())
		{
			emitOp(Op::SetLocal);
			emitUint32(local->second.index);
			return Status::Ok;
		}
	}

	auto constant = createConstant(Value(reinterpret_cast<Obj*>(m_allocator->allocateString(lhs))));
	loadConstant(constant);
	emitOp(Op::SetGlobal);

	return Status::Ok;
}

Compiler::Status Compiler::callExpr(const CallExpr& expr)
{
	TRY(compile(expr.calle));

	for (const auto& argument : expr.arguments)
	{
		TRY(compile(argument));
	}
	emitOp(Op::Call);
	emitUint32(static_cast<uint32_t>(expr.arguments.size()));

	return Status::Ok;
}

Compiler::Status Compiler::declareVariable(std::string_view name, size_t start, size_t end)
{
	if (m_scopes.size() == 0)
		return Status::Ok;

	auto& locals = m_scopes.back().localVariables;
	auto variable = locals.find(name);
	if (variable != locals.end())
	{
		return errorAt(start, end, "redeclaration of variable '%.*s'", name.size(), name.data());
	}
	locals[name] = Local{ static_cast<uint32_t>(locals.size()) };

	return Status::Ok;
}

Compiler::Status Compiler::binaryExpr(const BinaryExpr& expr)
{
	if (expr.op == TokenType::AndAnd)
	{
		TRY(compile(expr.lhs));
		auto placeToPatch = emitJump(Op::JumpIfFalse);
		emitOp(Op::PopStack);
		TRY(compile(expr.rhs));
		setJumpToHere(placeToPatch);

		return Status::Ok;
	}
	if (expr.op == TokenType::OrOr)
	{
		TRY(compile(expr.lhs));
		auto placeToPatch = emitJump(Op::JumpIfTrue);
		emitOp(Op::PopStack);
		TRY(compile(expr.rhs));
		setJumpToHere(placeToPatch);

		return Status::Ok;
	}

	TRY(compile(expr.lhs));
	TRY(compile(expr.rhs));
	// Can't pop here beacause I have to keep the result at the top. The pops will happen in the vm.
	switch (expr.op)
	{
		case TokenType::Plus: emitOp(Op::Add); break;
		case TokenType::EqualsEquals: emitOp(Op::Equals); break;
		default:
			ASSERT_NOT_REACHED();
	}

	return Status::Ok;
}

uint32_t Compiler::createConstant(Value value)
{
	currentByteCode().constants.push_back(value);
	return static_cast<uint32_t>(currentByteCode().constants.size() - 1);
}

void Compiler::loadConstant(uint32_t index)
{
	emitOp(Op::LoadConstant);
	emitUint32(index);
}

ByteCode& Compiler::currentByteCode()
{
	return *m_functionByteCodeStack.back();
}

void Compiler::emitOp(Op op)
{
	currentByteCode().emitOp(op);
	currentByteCode().lineNumberAtOffset.push_back(m_lineNumberStack.back());
}

void Compiler::emitUint32(uint32_t value)
{
	currentByteCode().emitUint32(value);
	for (int i = 0; i < 4; i++)
	{
		currentByteCode().lineNumberAtOffset.push_back(m_lineNumberStack.back());
	}
}

size_t Compiler::emitJump(Op op)
{
	emitOp(op);
	auto placeToPatch = currentByteCode().code.size();
	emitUint32(0);
	return placeToPatch;
}

void Compiler::emitJump(Op op, size_t location)
{
	emitOp(op);
	auto jumpSize = static_cast<uint32_t>(currentLocation() - location + sizeof(uint32_t));
	emitUint32(jumpSize);
}

void Compiler::setJumpToHere(size_t placeToPatch)
{
	auto jumpSize = static_cast<uint32_t>(currentByteCode().code.size() - placeToPatch) - sizeof(uint32_t);

	uint8_t* jumpLocation = &currentByteCode().code[placeToPatch];

	jumpLocation[0] = static_cast<uint8_t>((jumpSize >> 24) & 0xFF);
	jumpLocation[1] = static_cast<uint8_t>((jumpSize >> 16) & 0xFF);
	jumpLocation[2] = static_cast<uint8_t>((jumpSize >> 8) & 0xFF);
	jumpLocation[3] = static_cast<uint8_t>(jumpSize & 0xFF);
}

size_t Compiler::currentLocation()
{
	return currentByteCode().code.size();
}

void Compiler::beginScope()
{
	if (m_scopes.size() == 0)
	{
		m_scopes.push_back(Scope{ {}, 0 });
	}
	else
	{
		m_scopes.push_back(Scope{ {}, m_scopes.back().functionDepth });
	}
}

void Compiler::endScope()
{
	ASSERT(m_scopes.size() > 0);
	m_scopes.pop_back();
}

Compiler::Status Compiler::errorAt(size_t start, size_t end, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(start, end, format, args);
	va_end(args);
	return Status::Error;
}

Compiler::Status Compiler::errorAt(const Stmt& stmt, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(stmt.start, stmt.end(), format, args);
	va_end(args);
	return Status::Error;
}

Compiler::Status Compiler::errorAt(const Expr& expr, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(expr.start, expr.end(), format, args);
	va_end(args);
	return Status::Error;
}

Compiler::Status Compiler::errorAt(const Token& token, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(token.start, token.end, format, args);
	va_end(args);
	return Status::Error;
}
