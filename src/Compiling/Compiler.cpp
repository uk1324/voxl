#include "Compiler.hpp"
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

	const auto scriptName = m_allocator->allocateStringConstant("script").value;
	auto scriptFunction = m_allocator->allocateFunctionConstant(scriptName, 0).value;
	m_functionByteCodeStack.push_back(&scriptFunction->byteCode);
	m_functions.push_back(Function{});

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
	m_functions.pop_back();

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
	std::cout << "----<script>\n";
	disassembleByteCode(scriptFunction->byteCode, *m_allocator);
#endif

	return Result{ m_hadError, reinterpret_cast<ObjFunction*>(scriptFunction) };
}

Compiler::Status Compiler::compileFunction(
	std::string_view functionName, 
	ObjFunction* function,
	const std::vector<std::string_view>& arguments, 
	const StmtList& stmts, 
	size_t start, 
	size_t end)
{
	m_functionByteCodeStack.push_back(&function->byteCode);
	m_functions.push_back(Function{});

	beginScope();
	m_scopes.back().functionDepth++;

	for (const auto& argumentName : arguments)
	{
		// Could store the token for better error messages about redeclaration.
		TRY(createVariable(argumentName, start, end));
	}

	TRY(compile(stmts));
	emitOp(Op::LoadNull);
	emitOp(Op::Return);

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
	std::cout << "----" << functionName << '\n';
	disassembleByteCode(function->byteCode, *m_allocator);
#endif

	endScope();
	m_functionByteCodeStack.pop_back();

	const auto& upvalues = m_functions.back().upvalues;
	function->upvalueCount = static_cast<int>(upvalues.size());
	if (upvalues.size() != 0)
	{
		emitOp(Op::Closure);
		if (upvalues.size() > UINT8_MAX)
		{
			ASSERT_NOT_REACHED();
		}
		emitUint8(static_cast<uint8_t>(upvalues.size()));
		for (const auto upvalue : upvalues)
		{

			emitUint8(static_cast<uint8_t>(upvalue.index));
			emitUint8(static_cast<uint8_t>(upvalue.isLocal));
		}
	}

	m_functions.pop_back();

	return Status::Ok;
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
		CASE_STMT_TYPE(VariableDeclaration, variableDeclarationStmt)
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

Compiler::Status Compiler::variableDeclarationStmt(const VariableDeclarationStmt& stmt)
{
	for (const auto& [name, initializer] : stmt.variables)
	{
		// Local variables are create by just leaving the result of the initializer on top of the stack.
		if (initializer.has_value())
		{
			TRY(compile(*initializer));
		}
		else
		{
			emitOp(Op::LoadNull);
		}

		// The initializer is evaluated before declaring the variable so a variable from an outer scope with the same name can be used.
		TRY(createVariable(name, stmt.start, stmt.end()));

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
	const auto [nameConstant, name] = m_allocator->allocateStringConstant(stmt.name);
	const auto [functionConstant, function] = m_allocator->allocateFunctionConstant(name, static_cast<int>(stmt.arguments.size()));
	TRY(loadConstant(functionConstant));
	TRY(createVariable(stmt.name, stmt.start, stmt.end()));
	return compileFunction(stmt.name, function, stmt.arguments, stmt.stmts, stmt.start, stmt.end());
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

		// Declare a variable even if unused so the VM catch doesn't have to handle it as a special case.
		auto name = stmt.caughtValueName.has_value() ? *stmt.caughtValueName : "";
		TRY(createVariable(name, stmt.start, stmt.end()));

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
	// If classes outside the global scope were allowed they would need to be created each time the scope is executed.
	// Creating the class at compile time would require registering it to the GC so it doesn't get deleted.
	if (m_scopes.size() > 0)
		return errorAt(stmt, "classes can only be created at global scope");

	const auto [classNameConstant, className] = m_allocator->allocateStringConstant(stmt.name);

	TRY(loadConstant(classNameConstant));
	emitOp(Op::CreateClass);

	for (const auto& method : stmt.methods)
	{
		auto arguments = method->arguments;
		arguments.insert(arguments.begin(), "$");
		const auto [nameConstant, name] = m_allocator->allocateStringConstant(std::string(stmt.name) + '.' + std::string(method->name));
		const auto [functionConstant, function] =
			m_allocator->allocateFunctionConstant(name, static_cast<int>(arguments.size()));

		TRY(compileFunction(method->name, function, arguments, method->stmts, method->start, method->end()));

		const auto methodNameConstant = m_allocator->allocateStringConstant(method->name).constant;
		TRY(loadConstant(functionConstant));
		TRY(loadConstant(methodNameConstant));
		emitOp(Op::StoreMethod);
	}

	// If I allow classes to be created in local scope then this code needs to change becuase else the class wouldn't
	// be able to access itself.
	TRY(createVariable(stmt.name, stmt.start, stmt.end()));

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
		CASE_EXPR_TYPE(Null, nullExpr)
		CASE_EXPR_TYPE(StringConstant, stringConstantExpr)
		CASE_EXPR_TYPE(Binary, binaryExpr)
		CASE_EXPR_TYPE(Unary, unaryExpr)
		CASE_EXPR_TYPE(Identifier, identifierExpr)
		CASE_EXPR_TYPE(Call, callExpr)
		CASE_EXPR_TYPE(Assignment, assignmentExpr)
		CASE_EXPR_TYPE(Array, arrayExpr)
		CASE_EXPR_TYPE(GetField, getFieldExpr)
		CASE_EXPR_TYPE(Lambda, lambdaExpr)
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
	case TokenType::NotEquals: emitOp(Op::NotEquals); break;
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
	auto constant = m_allocator->createConstant(Value(expr.value));
	TRY(loadConstant(constant));
	return Status::Ok;
}

Compiler::Status Compiler::floatConstantExpr(const FloatConstantExpr& expr)
{
	auto constant = m_allocator->createConstant(Value(expr.value));
	TRY(loadConstant(constant));
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

Compiler::Status Compiler::nullExpr(const NullExpr& expr)
{
	emitOp(Op::LoadNull);
	return Status::Ok;
}

Compiler::Status Compiler::stringConstantExpr(const StringConstantExpr& expr)
{
	const auto constant = m_allocator->allocateStringConstant(expr.text, expr.length).constant;
	TRY(loadConstant(constant));
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
	return variable(expr.identifier, VariableOp::Get);
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
		TRY(variable(lhs, VariableOp::Set));
		return Status::Ok;
	}
	else if (expr.lhs->type == ExprType::GetField)
	{
		const auto lhs = static_cast<GetFieldExpr*>(expr.lhs.get());
		TRY(compile(lhs->lhs));
		const auto fieldNameConstant = m_allocator->allocateStringConstant(lhs->fieldName).constant;
		TRY(loadConstant(fieldNameConstant));
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

Compiler::Status Compiler::lambdaExpr(const LambdaExpr& expr)
{
	std::string_view anonymousFunctionName = "";
	const auto [nameConstant, name] = m_allocator->allocateStringConstant(anonymousFunctionName);
	const auto [functionConstant, function] = m_allocator->allocateFunctionConstant(name, static_cast<int>(expr.arguments.size()));
	TRY(compileFunction(anonymousFunctionName, function, expr.arguments, expr.stmts, expr.start, expr.end()));
	TRY(loadConstant(functionConstant));
	return Status::Ok;
}

Compiler::Status Compiler::getFieldExpr(const GetFieldExpr& expr)
{
	TRY(compile(expr.lhs));
	auto fieldNameConstant = m_allocator->allocateStringConstant(expr.fieldName).constant;
	TRY(loadConstant(fieldNameConstant));
	emitOp(Op::GetProperty);
	return Status::Ok;
}

Compiler::Status Compiler::createVariable(std::string_view name, size_t start, size_t end)
{
	if (m_scopes.size() == 0)
	{
		TRY(loadConstant(m_allocator->allocateStringConstant(name).constant));
		emitOp(Op::CreateGlobal);
		return Status::Ok;
	}

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
	locals[name] = Local{ static_cast<uint32_t>(localsCount), false };

	return Status::Ok;
}

Compiler::Status Compiler::variable(std::string_view name, VariableOp op)
{
	for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++)
	{
		auto& scope = *it;
		auto local = scope.localVariables.find(name);
		if (local != scope.localVariables.end())
		{
			auto& variable = local->second;
			if (scope.functionDepth != m_scopes.back().functionDepth)
			{
				variable.isCaptured = true;

				//m_functions[scope.functionDepth].upvalues.push_back(Upvalue{ variable.index, true });
				auto depth = scope.functionDepth + 1;

				auto& localUpvalues = m_functions[depth].upvalues;
				auto lastIndex = localUpvalues.size();
				for (size_t i = 0; i < localUpvalues.size(); i++)
				{
					if ((localUpvalues[i].index == variable.index) && (localUpvalues[i].isLocal == true))
					{
						lastIndex = i;
						break;
					}
				}
				if (lastIndex == localUpvalues.size())
				{
					localUpvalues.push_back(Upvalue{ variable.index, true });
				}
				depth++;

				for (; depth < m_functions.size(); depth++)
				{
					auto& upvalues = m_functions[depth].upvalues;
					auto newLastIndex = upvalues.size();
					for (size_t i = 0; i < upvalues.size(); i++)
					{
						if ((upvalues[i].index == lastIndex))
						{
							newLastIndex = i;
							break;
						}
					}
					if (newLastIndex == upvalues.size())
					{
						upvalues.push_back(Upvalue{ newLastIndex, false });
					}

					lastIndex = newLastIndex;
				}

				if (op == VariableOp::Set)
				{
					emitOp(Op::SetUpvalue);
				}
				else if (op == VariableOp::Get)
				{
					emitOp(Op::LoadUpvalue);
				}
				emitUint32(static_cast<uint32_t>(lastIndex));
			}
			else
			{
				if (op == VariableOp::Set)
				{
					emitOp(Op::SetLocal);
				}
				else if (op == VariableOp::Get)
				{
					emitOp(Op::LoadLocal);
				}
				emitUint32(static_cast<uint32_t>(local->second.index));
			}
			
			return Status::Ok;
		}
	}

	const auto constant = m_allocator->allocateStringConstant(name).constant;
	TRY(loadConstant(constant));
	if (op == VariableOp::Set)
	{
		emitOp(Op::SetGlobal);
	}
	else if (op == VariableOp::Get)
	{
		emitOp(Op::LoadGlobal);
	}
	return Status::Ok;
}

Compiler::Status Compiler::loadConstant(size_t index)
{
	if (index > UINT32_MAX)
		return Status::Error;
	emitOp(Op::LoadConstant);
	emitUint32(static_cast<uint32_t>(index));
	return Status::Ok;
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

void Compiler::emitUint8(uint8_t value)
{
	auto& code = currentByteCode();
	code.code.push_back(value);
	code.lineNumberAtOffset.push_back(m_lineNumberStack.back());
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
	auto jumpSize = static_cast<uint32_t>(currentLocation() - (placeToPatch + sizeof(uint32_t)));
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

	// No need to pop the values if the function return pops them.
	if ((m_scopes.back().functionDepth == 0)
		|| ((m_scopes.size() > 1) && (m_scopes.back().functionDepth == (&m_scopes.back())[-1].functionDepth)))
	{
		for (const auto& [_, local] : m_scopes.back().localVariables)
		{
			emitOp(Op::PopStack);
			if (local.isCaptured)
			{
				emitOp(Op::CloseUpvalue);
				emitUint8(static_cast<uint8_t>(local.index));
			}
		}
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