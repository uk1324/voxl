#include "Compiler.hpp"
#include <Compiling/Compiler.hpp>
#include <Debug/DebugOptions.hpp>
#include <Asserts.hpp>
#include <iostream>

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
	#include <Debug/Disassembler.hpp>
#endif

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
	emitOp(Op::Return);
	m_lineNumberStack.pop_back();

	m_functionByteCodeStack.pop_back();

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
	std::cout << "----<script>\n";
	disassembleByteCode(scriptFunction->byteCode);
#endif

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
		CASE_STMT_TYPE(Class, classStmt)
		CASE_STMT_TYPE(Try, tryStmt)
		CASE_STMT_TYPE(Throw, throwStmt)
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

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
	std::cout << "----" << stmt.name << '\n';
	disassembleByteCode(function->byteCode);
#endif

	endScope();
	m_functionByteCodeStack.pop_back();

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

	auto jumpToElse = emitJump(Op::JumpIfFalseAndPop);

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
		jumpToEnd = emitJump(Op::JumpIfFalseAndPop);
	}

	beginScope();
	TRY(compile(stmt.block));
	endScope();

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

Compiler::Status Compiler::tryStmt(const TryStmt& stmt)
{
	ASSERT(stmt.catchBlock.has_value() || stmt.finallyBlock.has_value());
	
	emitOp(Op::TryBegin);
	const auto tryJumpToCatch = currentLocation();
	emitUint32(0);
	TRY(compile(stmt.tryBlock));
	emitOp(Op::TryEnd);

	const auto jumpToEndOfCatch = emitJump(Op::Jump);

	if (stmt.catchBlock.has_value())
	{
		patch(tryJumpToCatch, static_cast<int32_t>(currentLocation()));

		beginScope();
		// Always declare a variable so the VM catch doesn't have to handle it as a special case.
		auto name = stmt.caughtValueName.has_value() ? *stmt.caughtValueName : "";
		TRY(declareVariable(name, stmt.start, stmt.end()));

		TRY(compile(*stmt.catchBlock));

		endScope();
	}
	setJumpToHere(jumpToEndOfCatch);

	if (stmt.finallyBlock.has_value())
	{
		TRY(compile(*stmt.finallyBlock));
	}

	return Status::Ok;
}

Compiler::Status Compiler::throwStmt(const ThrowStmt& stmt)
{
	TRY(compile(stmt.expr));
	emitOp(Op::Throw);
	return Status::Ok;
}

Compiler::Status Compiler::classStmt(const ClassStmt& stmt)
{
	if ((m_scopes.size() > 0) && (m_scopes.back().functionDepth > 0))
		return errorAt(stmt, "nested functions not allowed");

	auto className = m_allocator->allocateString(stmt.name);
	auto classNameConstant = createConstant(Value(reinterpret_cast<Obj*>(className)));

	TRY(declareVariable(stmt.name, stmt.start, stmt.end()));

	loadConstant(classNameConstant);
	emitOp(Op::CreateClass);

	for (const auto& method : stmt.methods)
	{
		auto nameWithPrefix = m_allocator->allocateString(std::string(stmt.name) + '.' + std::string(method->name));
		auto function = m_allocator->allocateFunction(nameWithPrefix, static_cast<int>(method->arguments.size() + 1));
		auto functionConstant = createConstant(Value(reinterpret_cast<Obj*>(function)));
		m_functionByteCodeStack.push_back(&function->byteCode);

		beginScope();
		m_scopes.back().functionDepth++;

		TRY(declareVariable("this", method->start, method->end()));
		for (const auto& argument : method->arguments)
		{
			// Could store the token for better error messages about redeclaration.
			TRY(declareVariable(argument, stmt.start, stmt.end()));
		}
		TRY(compile(method->stmts));

		emitOp(Op::LoadNull);
		emitOp(Op::Return);

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
		std::cout << "----" << stmt.name << '.' << method->name << '\n';
		disassembleByteCode(function->byteCode);
#endif

		endScope();
		m_functionByteCodeStack.pop_back();

		auto nameConstant = createStringConstant(method->name);
		loadConstant(functionConstant);
		loadConstant(nameConstant);
		emitOp(Op::StoreMethod);
	}

	loadConstant(classNameConstant);
	emitOp(Op::CreateGlobal);

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
		CASE_EXPR_TYPE(FloatConstant, floatConstantExpr)
		CASE_EXPR_TYPE(BoolConstant, boolConstantExpr)
		CASE_EXPR_TYPE(Binary, binaryExpr)
		CASE_EXPR_TYPE(StringConstant, stringConstantExpr)
		CASE_EXPR_TYPE(Unary, unaryExpr)
		CASE_EXPR_TYPE(Identifier, identifierExpr)
		CASE_EXPR_TYPE(Call, callExpr)
		CASE_EXPR_TYPE(Assignment, assignmentExpr)
		CASE_EXPR_TYPE(Array, arrayExpr)
		CASE_EXPR_TYPE(GetField, getFieldExpr)
	}
	m_lineNumberStack.pop_back();

	return Status::Ok;
}

Compiler::Status Compiler::compileBinaryExpr(const std::unique_ptr<Expr>& lhs, TokenType op, const std::unique_ptr<Expr>& rhs)
{
	if (op == TokenType::AndAnd)
	{
		TRY(compile(lhs));
		auto placeToPatch = emitJump(Op::JumpIfFalse);
		emitOp(Op::PopStack);
		TRY(compile(rhs));
		setJumpToHere(placeToPatch);

		return Status::Ok;
	}
	if (op == TokenType::OrOr)
	{
		TRY(compile(lhs));
		auto placeToPatch = emitJump(Op::JumpIfTrue);
		emitOp(Op::PopStack);
		TRY(compile(rhs));
		setJumpToHere(placeToPatch);

		return Status::Ok;
	}

	TRY(compile(lhs));
	TRY(compile(rhs));
	switch (op)
	{
		case TokenType::Plus: emitOp(Op::Add); break;
		case TokenType::PlusPlus: emitOp(Op::Concat); break;
		case TokenType::Minus: emitOp(Op::Subtract); break;
		case TokenType::Star: emitOp(Op::Multiply); break;
		case TokenType::Slash: emitOp(Op::Divide); break;
		case TokenType::Percent: emitOp(Op::Modulo); break;
		case TokenType::EqualsEquals: emitOp(Op::Equals); break;
		case TokenType::Less: emitOp(Op::Less); break;
		case TokenType::LessEquals: emitOp(Op::LessEqual); break;
		case TokenType::More: emitOp(Op::More); break;
		case TokenType::MoreEquals: emitOp(Op::MoreEqual); break;
		default:
			ASSERT_NOT_REACHED();
	}

	return Status::Ok;
}

Compiler::Status Compiler::intConstantExpr(const IntConstantExpr& expr)
{
	auto constant = createConstant(Value(expr.value));
	loadConstant(constant);
	return Status::Ok;
}

Compiler::Status Compiler::floatConstantExpr(const FloatConstantExpr& expr)
{
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

Compiler::Status Compiler::binaryExpr(const BinaryExpr& expr)
{
	return compileBinaryExpr(expr.lhs, expr.op, expr.rhs);
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
	if (expr.op.has_value())
	{
		TRY(compileBinaryExpr(expr.lhs, *expr.op, expr.rhs));
	}
	else
	{
		TRY(compile(expr.rhs));
	}

	if (expr.lhs->type == ExprType::Identifier)
	{
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
	else if (expr.lhs->type == ExprType::GetField)
	{
		const auto lhs = static_cast<GetFieldExpr*>(expr.lhs.get());
		TRY(compile(lhs->lhs));
		auto fieldName = createStringConstant(lhs->fieldName);
		loadConstant(fieldName);
		emitOp(Op::SetProperty);
		return Status::Ok;
	}

	return errorAt(expr, "invalid left side of assignment");
}

Compiler::Status Compiler::callExpr(const CallExpr& expr)
{
	// TODO: Add an optimization for calling on instance fields.
	TRY(compile(expr.calle));

	for (const auto& argument : expr.arguments)
	{
		TRY(compile(argument));
	}
	emitOp(Op::Call);
	emitUint32(static_cast<uint32_t>(expr.arguments.size()));

	return Status::Ok;
}

Compiler::Status Compiler::arrayExpr(const ArrayExpr& expr)
{
	ASSERT_NOT_REACHED();
	/*for (const auto& value : expr.values)
	{
		TRY(compile(value));
	}
	TRY(emitOpArg(Op::CreateArray, expr.values.size(), expr.start, expr.end()));*/
	return Status::Error;
}

Compiler::Status Compiler::getFieldExpr(const GetFieldExpr& expr)
{
	TRY(compile(expr.lhs));
	auto fieldName = createStringConstant(expr.fieldName);
	loadConstant(fieldName);
	emitOp(Op::GetProperty);
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
	// TODO: Make this better maybe change scopes to store the count or maybe store it with a local.
	size_t localsCount = 0;
	auto functionDepth = m_scopes.back().functionDepth;
	for (auto scope = m_scopes.crbegin(); scope != m_scopes.crend(); scope++)
	{
		if (scope->functionDepth != functionDepth)
			break;
		localsCount += scope->localVariables.size();
	}
	locals[name] = Local{ static_cast<uint32_t>(localsCount) };

	return Status::Ok;
}

uint32_t Compiler::createConstant(Value value)
{
	currentByteCode().constants.push_back(value);
	return static_cast<uint32_t>(currentByteCode().constants.size() - 1);
}

uint32_t Compiler::createStringConstant(std::string_view name)
{
	return createConstant(Value(reinterpret_cast<Obj*>(m_allocator->allocateString(name))));
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
	currentByteCode().code.push_back(static_cast<uint8_t>(op));
	currentByteCode().lineNumberAtOffset.push_back(m_lineNumberStack.back());
}

Compiler::Status Compiler::emitOpArg(Op op, size_t arg, size_t start, size_t end)
{
	if (arg > std::numeric_limits<uint32_t>::max())
	{
		return errorAt(start, end, "argument size too big");
	}
	emitOp(op);
	emitUint32(static_cast<uint32_t>(arg));
	return Status::Ok;
}

void Compiler::emitUint32(uint32_t value)
{
	auto& code = currentByteCode();
	code.code.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
	code.code.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
	code.code.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
	code.code.push_back(static_cast<uint8_t>(value & 0xFF));

	for (int _ = 0; _ < 4; _++)
	{
		code.lineNumberAtOffset.push_back(m_lineNumberStack.back());
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
	auto jumpSize = static_cast<uint32_t>((currentLocation() + sizeof(uint32_t)) - location);
	emitUint32(jumpSize);
}

void Compiler::setJumpToHere(size_t placeToPatch)
{
	auto jumpSize = static_cast<uint32_t>(currentLocation() - (placeToPatch + sizeof(uint32_t))) ;
	patch(placeToPatch, jumpSize);
}

void Compiler::patch(size_t placeToPatch, uint32_t value)
{
	uint8_t* jumpLocation = &currentByteCode().code[placeToPatch];
	jumpLocation[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
	jumpLocation[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
	jumpLocation[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
	jumpLocation[3] = static_cast<uint8_t>(value & 0xFF);
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
	for (size_t _ = 0; _ < m_scopes.back().localVariables.size(); _++)
	{
		emitOp(Op::PopStack);
	}
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