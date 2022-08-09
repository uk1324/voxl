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

using namespace Voxl;

Compiler::Compiler(Allocator& allocator)
	: m_hadError(false)
	, m_allocator(allocator)
	, m_errorPrinter(nullptr)
	, m_rootMarkingFunctionHandle(allocator.registerMarkingFunction(this, Compiler::mark))
	, m_module(nullptr)
{}

Compiler::Result Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& ast, ErrorPrinter& errorPrinter)
{
	m_hadError = false;
	m_errorPrinter = &errorPrinter;

	m_module = m_allocator.allocateModule();
	const auto scriptName = m_allocator.allocateStringConstant("script").value;
	auto scriptFunction = m_allocator.allocateFunctionConstant(scriptName, 0, &m_module->globals).value;
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
	// Have to load null becuase module functions have to return just like normal functions.
	// If nothing was on the stack it might return a value from an empty stack.
	emitOp(Op::LoadNull);
	emitOp(Op::Return);
	m_lineNumberStack.pop_back();

	m_functionByteCodeStack.pop_back();
	m_functions.pop_back();

#ifdef VOXL_DEBUG_PRINT_COMPILED_FUNCTIONS
	if (m_hadError == false)
	{
		std::cout << "----<script>\n";
		disassembleByteCode(scriptFunction->byteCode, m_allocator);
	}
#endif

	return Result{ m_hadError, scriptFunction, m_module };
}

Compiler::Status Compiler::compileFunction( 
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
	std::cout << "----" << function->name->chars << '\n';
	disassembleByteCode(function->byteCode, m_allocator);
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
		CASE_STMT_TYPE(VariableDeclaration, variableDeclarationStmt)
		CASE_STMT_TYPE(Block, blockStmt)
		CASE_STMT_TYPE(Fn, fnStmt)
		CASE_STMT_TYPE(Ret, retStmt)
		CASE_STMT_TYPE(If, ifStmt)
		CASE_STMT_TYPE(Loop, loopStmt)
		CASE_STMT_TYPE(Break, breakStmt)
		CASE_STMT_TYPE(Class, classStmt)
		CASE_STMT_TYPE(Impl, implStmt)
		CASE_STMT_TYPE(Try, tryStmt)
		CASE_STMT_TYPE(Throw, throwStmt)
		CASE_STMT_TYPE(Match, matchStmt)
		CASE_STMT_TYPE(Use, useStmt)
		CASE_STMT_TYPE(UseAll, useAllStmt)
		CASE_STMT_TYPE(UseSelective, useSelectiveStmt)
	}
#undef CASE_STMT_STMT
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
	const auto [nameConstant, name] = m_allocator.allocateStringConstant(stmt.name);
	const auto [functionConstant, function] =
		m_allocator.allocateFunctionConstant(name, static_cast<int>(stmt.arguments.size()), &m_module->globals);
	TRY(loadConstant(functionConstant));
	TRY(createVariable(stmt.name, stmt.start, stmt.end()));
	return compileFunction(function, stmt.arguments, stmt.stmts, stmt.start, stmt.end());
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

	beginScope();
	TRY(compile(stmt.ifThen));
	endScope();

	size_t jumpToEndOfElse = 0;
	if (stmt.elseThen.has_value())
	{
		jumpToEndOfElse = emitJump(Op::Jump);
	}

	setJumpToHere(jumpToElse);

	if (stmt.elseThen.has_value())
	{
		beginScope();
		TRY(compile(*stmt.elseThen));
		endScope();
		setJumpToHere(jumpToEndOfElse);
	}

	return Status::Ok;
}

Compiler::Status Compiler::loopStmt(const LoopStmt& stmt)
{
	beginScope();
	if (stmt.initStmt.has_value())
	{
		TRY(compile(*stmt.initStmt));
	}

	const auto beginning = currentLocation();
	m_loops.push_back(Loop{ beginning, m_scopes.size() });
	size_t jumpToEnd = 0; // Won't be used if there is no condition.
	if (stmt.condition.has_value())
	{
		TRY(compile(*stmt.condition));
		jumpToEnd = emitJump(Op::JumpIfFalseAndPop);
	}

	beginScope();
	TRY(compile(stmt.block));
	if (stmt.iterationExpr.has_value())
	{
		TRY(compile(*stmt.iterationExpr));
		emitOp(Op::PopStack);
	}
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

	endScope();

	m_loops.pop_back();

	return Status::Ok;
}

Compiler::Status Compiler::breakStmt(const BreakStmt& stmt)
{
	if (m_loops.empty())
	{
		return errorAt(stmt, "cannot use break outside of a loop");
	}
	auto& loop = m_loops.back();
	for (auto scope = m_scopes.begin() + loop.scopeDepth; scope != m_scopes.end(); scope++)
	{
		popOffLocals(*scope);
	}
	const auto jump = emitJump(Op::Jump);
	loop.breakJumpLocations.push_back(jump);
	return Status::Ok;
}

Compiler::Status Compiler::tryStmt(const TryStmt& stmt)
{
	ASSERT((stmt.catchBlocks.empty() == false) || stmt.finallyBlock.has_value());
	// TODO: Currently this isn't handled properly.
	ASSERT((stmt.finallyBlock.has_value() && stmt.catchBlocks.empty()) == false);

	const auto jumpToCatchBlocks = emitJump(Op::TryBegin);
	beginScope();
	TRY(compile(stmt.tryBlock));
	endScope();
	emitOp(Op::TryEnd);
	const auto jumpToEndOfCatchBlocks = emitJump(Op::Jump);

	setJumpToHere(jumpToCatchBlocks);
	std::vector<size_t> jumpsToCatchEpilogue;
	for (const auto& catchBlock : stmt.catchBlocks)
	{
		TRY(compile(catchBlock.pattern));
		const auto jumpToNextHandler = emitJump(Op::JumpIfFalseAndPop);

		beginScope();

		// Declare a variable even if unused so the VM catch doesn't have to handle it as a special case.
		const auto name = catchBlock.caughtValueName.has_value() ? *catchBlock.caughtValueName : "";
		TRY(createVariable(name, stmt.start, stmt.end()));

		beginScope();
		TRY(compile(catchBlock.block));
		endScope();
		jumpsToCatchEpilogue.push_back(emitJump(Op::Jump));

		// Not calling endScope() so the caught value remains on the stack through multiple handlers.
		m_scopes.pop_back();
		setJumpToHere(jumpToNextHandler);
	}

	emitOp(Op::Rethrow);

	for (const auto& jump : jumpsToCatchEpilogue)
	{
		setJumpToHere(jump);
	}
	// Pop the caught value.
	emitOp(Op::PopStack);

	setJumpToHere(jumpToEndOfCatchBlocks);

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

// Class has to be TOS.
Compiler::Status Compiler::compileMethods(std::string_view className, const std::vector<std::unique_ptr<FnStmt>>& methods)
{
	for (const auto& method : methods)
	{
		auto arguments = method->arguments;
		arguments.insert(arguments.begin(), "$");
		const auto [nameConstant, name] = m_allocator.allocateStringConstant(
			std::string(className) + '.' + std::string(method->name));
		const auto [functionConstant, function] =
			m_allocator.allocateFunctionConstant(name, static_cast<int>(arguments.size()), &m_module->globals);

		TRY(compileFunction(function, arguments, method->stmts, method->start, method->end()));

		const auto methodNameConstant = m_allocator.allocateStringConstant(method->name).constant;
		TRY(loadConstant(functionConstant));
		TRY(loadConstant(methodNameConstant));
		emitOp(Op::StoreMethod);
	}

	return Status::Ok;
}

Compiler::Status Compiler::classStmt(const ClassStmt& stmt)
{
	// If classes outside the global scope were allowed they would need to be created each time the scope is executed.
	// Creating the class at compile time would require registering it to the GC so it doesn't get deleted.
	if (m_scopes.size() > 0)
		return errorAt(stmt, "classes can only be created at global scope");

	const auto [classNameConstant, className] = m_allocator.allocateStringConstant(stmt.name);

	// TODO: When inheriting from native classes also allocate a native class.
	TRY(loadConstant(classNameConstant));
	emitOp(Op::CreateClass);

	TRY(compileMethods(stmt.name, stmt.methods));

	// If I allow classes to be created in local scope then this code needs to change becuase else the class wouldn't
	// be able to access itself.
	TRY(createVariable(stmt.name, stmt.start, stmt.end()));

	return Status::Ok;
}

Compiler::Status Compiler::implStmt(const ImplStmt& stmt)
{
	if (m_scopes.size() > 0)
		return errorAt(stmt, "impl statements can only appear at global scope");
	// TODO: Add type checking if the type is a class either here or in Op::StoreMethod.
	TRY(variable(stmt.typeName, VariableOp::Get));
	TRY(compileMethods(stmt.typeName, stmt.methods));
	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::matchStmt(const MatchStmt& stmt)
{
	TRY(compile(stmt.expr));
	
	std::vector<size_t> jumpsToEnd;
	for (const auto& case_ : stmt.cases)
	{
		TRY(compile(case_.pattern));
		const auto jumpToNextCase = emitJump(Op::JumpIfFalseAndPop);
		TRY(compile(case_.block));
		jumpsToEnd.push_back(emitJump(Op::Jump));
		setJumpToHere(jumpToNextCase);
	}

	for (const auto jump : jumpsToEnd)
	{
		setJumpToHere(jump);
	}

	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::loadModule(std::string_view filePath)
{
	const auto filenameConstant = m_allocator.allocateStringConstant(filePath).constant;
	TRY(loadConstant(filenameConstant));
	emitOp(Op::Import);
	// Pop the return value of the module main function. This can't be done inside Op::Import becuase to execute a non native main
	// the instruction has to move onto the next one. It also may be posible to fix this by making module calls special; making them not return
	// a value and removing the code for getting the return value in Op::Return.
	emitOp(Op::PopStack);
	emitOp(Op::ModuleSetLoaded);
	return Status::Ok;
}

Compiler::Status Compiler::useStmt(const UseStmt& stmt)
{
	TRY(loadModule(stmt.path));
	// TODO Maybe check if this is a valid UTF-8 string.
	auto variableName = stmt.variableName.has_value()
		? *stmt.variableName
		: std::filesystem::path(stmt.path).stem();

	TRY(createVariable(variableName.u8string(), stmt.start, stmt.end()));
	return Status::Ok;
}

Compiler::Status Compiler::useAllStmt(const UseAllStmt& stmt)
{
	if (m_scopes.size() > 0)
		return errorAt(stmt, "use all can only appear at global scope");
	TRY(loadModule(stmt.path));
	emitOp(Op::ModuleImportAllToGlobalNamespace);
	return Status::Ok;
}

Compiler::Status Compiler::useSelectiveStmt(const UseSelectiveStmt& stmt)
{
	TRY(loadModule(stmt.path));
	for (const auto& [originalName, newName] : stmt.variablesToImport)
	{
		if (newName.has_value() && originalName == newName)
			return errorAt(stmt, "imported variable ('%.*s') name is the same as it's alias name", newName->length(), newName->data());

		TRY(getField(originalName));
		TRY(createVariable(newName.has_value() ? *newName : originalName, stmt.start, stmt.end()));
	}
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
#undef CASE_EXPR_TYPE
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
	case TokenType::LeftBracket: emitOp(Op::GetIndex); break;
	default:
		ASSERT_NOT_REACHED();
	}

	return Status::Ok;
}

Compiler::Status Compiler::intConstantExpr(const IntConstantExpr& expr)
{
	auto constant = m_allocator.createConstant(Value(expr.value));
	TRY(loadConstant(constant));
	return Status::Ok;
}

Compiler::Status Compiler::floatConstantExpr(const FloatConstantExpr& expr)
{
	auto constant = m_allocator.createConstant(Value(expr.value));
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

Compiler::Status Compiler::nullExpr(const NullExpr&)
{
	emitOp(Op::LoadNull);
	return Status::Ok;
}

Compiler::Status Compiler::stringConstantExpr(const StringConstantExpr& expr)
{
	const auto constant = m_allocator.allocateStringConstant(expr.text, expr.length).constant;
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

	// TODO: Lhs get evaluted twice this can be fixed changing the order of the operands of get 
	// and set field.
	// Evaluating lhs, duplicating it, evaluating rhs, evaluating op, assigning.
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
		const auto fieldNameConstant = m_allocator.allocateStringConstant(lhs->fieldName).constant;
		TRY(loadConstant(fieldNameConstant));
		emitOp(Op::SetField);
		return Status::Ok;
	}
	else if (expr.lhs->type == ExprType::Binary)
	{
		const auto lhs = static_cast<BinaryExpr*>(expr.lhs.get());
		if (lhs->op == TokenType::LeftBracket)
		{
			TRY(compile(lhs->lhs));
			TRY(compile(lhs->rhs));
			emitOp(Op::SetIndex);
			return Status::Ok;
		}
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

Compiler::Status Compiler::arrayExpr(const ArrayExpr&)
{
	ASSERT_NOT_REACHED();
	/*for (const auto& value : expr.values)
	{
		TRY(compile(value));
	}
	TRY(emitOpArg(Op::CreateArray, expr.values.size(), expr.start, expr.end()));*/
	return Status::Error;
}

// The expression being matches must be TOS.
Compiler::Status Compiler::compile(const std::unique_ptr<Ptrn>& ptrn)
{
#define CASE_PTRN_TYPE(ptrnType, ptrnFunction) \
	case PtrnType::ptrnType: TRY(ptrnFunction(*static_cast<ptrnType##Ptrn*>(ptrn.get()))); break;	

	m_lineNumberStack.push_back(m_errorPrinter->sourceInfo().getLine(ptrn->start));
	switch (ptrn->type)
	{
		CASE_PTRN_TYPE(Class, classPtrn)
	}
#undef CASE_PTRN_TYPE

	return Status::Ok;
}

Compiler::Status Compiler::classPtrn(const ClassPtrn& ptrn)
{
	TRY(variable(ptrn.className, VariableOp::Get));
	emitOp(Op::MatchClass);
	return Status::Ok;
}

Compiler::Status Compiler::lambdaExpr(const LambdaExpr& expr)
{
	static constexpr std::string_view ANONYMOUS_FUNCTION_NAME = "";
	const auto [nameConstant, name] = m_allocator.allocateStringConstant(ANONYMOUS_FUNCTION_NAME);
	const auto [functionConstant, function] = 
		m_allocator.allocateFunctionConstant(name, static_cast<int>(expr.arguments.size()), &m_module->globals);
	TRY(compileFunction(function, expr.arguments, expr.stmts, expr.start, expr.end()));
	TRY(loadConstant(functionConstant));
	return Status::Ok;
}

Compiler::Status Compiler::getFieldExpr(const GetFieldExpr& expr)
{
	TRY(compile(expr.lhs));
	TRY(getField(expr.fieldName));
	return Status::Ok;
}

Compiler::Status Compiler::createVariable(std::string_view name, size_t start, size_t end)
{
	if (m_scopes.size() == 0)
	{
		TRY(loadConstant(m_allocator.allocateStringConstant(name).constant));
		emitOp(Op::CreateGlobal);
		return Status::Ok;
	}

	auto& locals = m_scopes.back().localVariables;
	const auto variable = locals.find(name);
	if (variable != locals.end())
	{
		return errorAt(start, end, "redeclaration of variable '%.*s'", name.size(), name.data());
	}
	// TODO: Make this better maybe change scopes to store the count or maybe store it with a local.
	size_t localsCount = 0;
	const auto functionDepth = m_scopes.back().functionDepth;
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
					emitOp(Op::GetUpvalue);
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
					emitOp(Op::GetLocal);
				}
				emitUint32(static_cast<uint32_t>(local->second.index));
			}
			
			return Status::Ok;
		}
	}

	const auto constant = m_allocator.allocateStringConstant(name).constant;
	TRY(loadConstant(constant));
	if (op == VariableOp::Set)
	{
		emitOp(Op::SetGlobal);
	}
	else if (op == VariableOp::Get)
	{
		emitOp(Op::GetGlobal);
	}
	return Status::Ok;
}

Compiler::Status Compiler::getField(std::string_view fieldName)
{
	const auto fieldNameConstant = m_allocator.allocateStringConstant(fieldName).constant;
	TRY(loadConstant(fieldNameConstant));
	emitOp(Op::GetField);
	return Status::Ok;
}

Compiler::Status Compiler::loadConstant(size_t index)
{
	if (index > UINT32_MAX)
		return Status::Error;
	emitOp(Op::GetConstant);
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
		popOffLocals(m_scopes.back());
	}

	m_scopes.pop_back();
}

void Compiler::popOffLocals(const Scope& scope)
{
	for (const auto& [_, local] : scope.localVariables)
	{
		emitOp(Op::PopStack);
		if (local.isCaptured)
		{
			emitOp(Op::CloseUpvalue);
			emitUint8(static_cast<uint8_t>(local.index));
		}
	}
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

void Compiler::mark(Compiler* compiler, Allocator& allocator)
{
	if (compiler->m_module != nullptr)
	{
		allocator.addObj(compiler->m_module);
	}
}
