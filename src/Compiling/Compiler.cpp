#include <Compiling/Compiler.hpp>
#include <Debug/DebugOptions.hpp>
#include <Asserts.hpp>
#include <Debug/Disassembler.hpp>
#include <Format.hpp>
#include <iostream>

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
	, m_errorReporter(nullptr)
	, m_sourceInfo(nullptr)
	, m_rootMarkingFunctionHandle(allocator.registerMarkingFunction(this, Compiler::mark))
	, m_module(nullptr)
{}

Compiler::Result Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& ast, const SourceInfo& sourceInfo, ErrorReporter& errorReporter)
{
	m_hadError = false;
	m_errorReporter = &errorReporter;
	m_sourceInfo = &sourceInfo;

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
	m_lineNumberStack.push_back(m_sourceInfo->lineStartOffsets.size());
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
	const SourceLocation& location)
{
	m_functionByteCodeStack.push_back(&function->byteCode);
	m_functions.push_back(Function{});

	beginScope();
	currentScope().functionDepth++;
	for (size_t i = 0; i < arguments.size(); i++)
	{
		const auto& argumentName = arguments[i];
		// This allows for any function to have it's first argument be called '$'. Might want to change it later. Could pass an argument that says
		// if this is a function or method.
		if ((i == 0) && (argumentName == "$"))
		{
			TRY(createSpecialVariable(argumentName, location));
		}
		else
		{
			// Could store the token for better error messages about redeclaration.
			TRY(createVariable(argumentName, location));
		}
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

	m_lineNumberStack.push_back(m_sourceInfo->getLine(stmt->start));
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
		// Local variables are created by just leaving the result of the initializer on top of the stack.
		if (initializer.has_value())
		{
			TRY(compile(*initializer));
		}
		else
		{
			emitOp(Op::LoadNull);
		}

		// The initializer is evaluated before declaring the variable so a variable from an outer scope with the same name can be used.
		TRY(createVariable(name, stmt.location()));

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
	TRY(createVariable(stmt.name, stmt.location()));
	return compileFunction(function, stmt.arguments, stmt.stmts, stmt.location());
}

Compiler::Status Compiler::retStmt(const RetStmt& stmt)
{
	// Not using backwards iterators because cleanUpBeforeJumpingOutOfScope can change m_scopes invalidating iterators.
	for (auto i = m_scopes.size() - 1; (i != 0) && (m_scopes[i].functionDepth == currentScope().functionDepth);)
	{
		const auto& scope = m_scopes[i];
		if (scope.type == ScopeType::Finally)
		{
			return errorAt(stmt.location(), "ret not allowed inside finally block");
		}
		else
		{
			TRY(cleanUpBeforeJumpingOutOfScope(scope, false));
		}

		if (i == 0)
			break;
		i--;
	}
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
	if (m_loops.empty() || (m_scopes[m_loops.back().scopeDepth].functionDepth != currentFunctionDepth()))
	{
		return errorAt(stmt.location(), "cannot use break outside of a loop");
	}
	auto& currentLoop = m_loops.back();

	// Not using iterators because cleanUpBeforeJumpingOutOfScope can change m_scopes invalidating iterators.
	for (auto i = currentLoop.scopeDepth; i < m_scopes.size(); i++)
	{
		const auto scope = m_scopes[i];

		if ((scope.functionDepth == currentScope().functionDepth) && (scope.type == ScopeType::Finally))
			return errorAt(stmt.location(), "break not allowed inside finally block");

		TRY(cleanUpBeforeJumpingOutOfScope(scope, true));
	}
	const auto jump = emitJump(Op::Jump);
	currentLoop.breakJumpLocations.push_back(jump);
	return Status::Ok;
}

Compiler::Status Compiler::tryStmt(const TryStmt& stmt)
{
	/*
	Code generated
	try { There is no scope created for the first try just TryBegin emmited.
		try {
			<actual try>
		} catch {
			<actual catch>
			<rethrow if not handled>
		}
	} catch thrown_inside_catch_or_not_caught {
		<finally>
		<rethrow thrown_inside_catch_or_not_caught> 
	}
	<finally>

	TODO:
	Currently the same code is generated even if a try finally is used. There is no <actual catch> code so it always gets to <rethrow if not handled>
	which jumps to the finally and just rethrows it again. This could be improved, but if should be implemented in a seperate block or function
	even if the code is duplicated because otherwise it makes a mess.
	*/

	ByteCode finallyBlockByteCode;
	if (stmt.finallyBlock.has_value())
	{
		m_functionByteCodeStack.push_back(&finallyBlockByteCode);
		beginScope(ScopeType::Finally);
		emitOp(Op::FinallyBegin);
		TRY(compile(*stmt.finallyBlock));
		endScope();
		m_functionByteCodeStack.pop_back();
	}
	
	// TODO Could just store one copy of this or maybe just store a std::optional<Bytecode&>.
	ByteCode emptyByteCode;
	// TODO: could remove this if there is no finally.
	const auto jumpToFinallyWithRethrow = emitJump(Op::TryBegin);
	beginScope(ScopeType::Try);
	currentScope().try_.finallyBlockByteCode = &emptyByteCode;

	const auto jumpToCatchBlocks = emitJump(Op::TryBegin);
	beginScope(ScopeType::Try);
	currentScope().try_.finallyBlockByteCode = &finallyBlockByteCode;
	TRY(compile(stmt.tryBlock));
	endScope();
	const auto jumpToEndOfCatchBlocks = emitJump(Op::Jump);

	setJumpToHere(jumpToCatchBlocks);
	std::vector<size_t> jumpsToCatchEpilogue;
	for (const auto& catchBlock : stmt.catchBlocks)
	{
		TRY(compile(catchBlock.pattern));
		const auto jumpToNextHandler = emitJump(Op::JumpIfFalseAndPop);

		beginScope();

		// Declare a variable even if unused so the VM catch doesn't have to handle it as a special case.
		// This also registers it inside the compiler so break and continue statements know to pop it off.
		const auto name = catchBlock.caughtValueName.has_value() ? *catchBlock.caughtValueName : "";
		TRY(createVariable(name, stmt.location()));
		beginScope(ScopeType::Catch);
		currentScope().catch_.finallyBlockByteCode = &finallyBlockByteCode;
		TRY(compile(catchBlock.block));

		endScope();

		jumpsToCatchEpilogue.push_back(emitJump(Op::Jump));
		// Remove caught value variable so it remains on the stack throughout multiple handlers.
		const auto variablesRemoved = m_scopes.back().localVariables.erase(name);
		ASSERT(variablesRemoved == 1);
		endScope();
		setJumpToHere(jumpToNextHandler);
	}
	// Rethrow value if it didn't match any catch block.
	emitOp(Op::Throw);

	for (const auto& jump : jumpsToCatchEpilogue)
	{
		setJumpToHere(jump);
	}
	emitOp(Op::PopStack); // Pop the caught value.

	setJumpToHere(jumpToEndOfCatchBlocks);

	endScope(); // jumpToFinallyWithRethrow end


	if (stmt.finallyBlock.has_value())
	{
		currentByteCode().append(finallyBlockByteCode);
		const auto jumpPastFinallyRethrow = emitJump(Op::Jump);

		setJumpToHere(jumpToFinallyWithRethrow);
		beginScope();
		// Register caught value to the compiler so it is popped of when needed.
		const auto caughtValueName = "";
		TRY(createVariable(caughtValueName, stmt.location()));
		currentByteCode().append(finallyBlockByteCode);
		// Remove caught value so it can be rethrown.
		const auto variablesRemoved = m_scopes.back().localVariables.erase(caughtValueName);
		ASSERT(variablesRemoved == 1);
		endScope();
		emitOp(Op::Throw);

		setJumpToHere(jumpPastFinallyRethrow);
	}
	else
	{
		const auto jumpPastFinallyRethrow = emitJump(Op::Jump);
		setJumpToHere(jumpToFinallyWithRethrow);
		// This code handles the case if there is a throw inside catch. 
		// TODO: Comment this better and maybe rename some things.
		emitOp(Op::Throw);
		setJumpToHere(jumpPastFinallyRethrow);
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

		TRY(compileFunction(function, arguments, method->stmts, method->location()));

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
		return errorAt(stmt.location(), "classes can only be created at global scope");

	const auto [classNameConstant, className] = m_allocator.allocateStringConstant(stmt.name);

	TRY(loadConstant(classNameConstant));
	emitOp(Op::CreateClass);

	if (stmt.superclassName.has_value())
	{
		TRY(loadVariable(*stmt.superclassName));
		emitOp(Op::Inherit);
	}

	TRY(compileMethods(stmt.name, stmt.methods));

	// If I allow classes to be created in local scope then this code needs to change becuase else the class wouldn't
	// be able to access itself.
	TRY(createVariable(stmt.name, stmt.location()));

	return Status::Ok;
}

Compiler::Status Compiler::implStmt(const ImplStmt& stmt)
{
	if (m_scopes.size() > 0)
		return errorAt(stmt.location(), "impl statements can only appear at global scope");
	// TODO: Add type checking if the type is a class either here or in Op::StoreMethod.
	TRY(loadVariable(stmt.typeName));
	TRY(compileMethods(stmt.typeName, stmt.methods));
	emitOp(Op::PopStack);
	return Status::Ok;
}

Compiler::Status Compiler::matchStmt(const MatchStmt& stmt)
{
	// TODO: Check if there are multiple AlwaysTrue patterns or patterns after AlwaysTrue. Report only warning.

	beginScope();

	TRY(compile(stmt.expr));
	TRY(createVariable(".matchedValue", stmt.location()));
	
	std::vector<size_t> jumpsToEnd;
	for (const auto& case_ : stmt.cases)
	{
		TRY(compile(case_.pattern));
		const auto jumpToNextCase = emitJump(Op::JumpIfFalseAndPop);
		auto isStmtAllowedInMatchStmt = [](const std::unique_ptr<Stmt>& stmt) {
			// TODO: Maybe give better error messages.
			// Using a switch to get warnings if this is not exhaustive.
			switch (stmt->type)
			{
			case StmtType::Expr: return true;
			case StmtType::VariableDeclaration: return false;
			case StmtType::Block: return true;
			case StmtType::Fn: return false;
			case StmtType::Ret: return true;
			case StmtType::If: return true;
			case StmtType::Loop: return true;
			case StmtType::Break: return true;
			case StmtType::Class: return false;
			case StmtType::Impl: return false;
			case StmtType::Try: return true;
			case StmtType::Throw: return true;
			case StmtType::Match: return true;
			case StmtType::Use: return false;
			case StmtType::UseAll: return false;
			case StmtType::UseSelective: return false;
			}
			return false;
		};
		if (isStmtAllowedInMatchStmt(case_.stmt) == false)
			return errorAt(stmt.location(), "statement not allowed in match expression");

		TRY(compile(case_.stmt));
		jumpsToEnd.push_back(emitJump(Op::Jump));
		setJumpToHere(jumpToNextCase);
	}

	for (const auto jump : jumpsToEnd)
	{
		setJumpToHere(jump);
	}

	endScope();

	return Status::Ok;
}

Compiler::Status Compiler::loadModule(std::string_view filePath)
{
	const auto filenameConstant = m_allocator.allocateStringConstant(filePath).constant;
	TRY(loadConstant(filenameConstant));
	emitOp(Op::Import);
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

	TRY(createVariable(variableName.u8string(), stmt.location()));
	return Status::Ok;
}

Compiler::Status Compiler::useAllStmt(const UseAllStmt& stmt)
{
	if (m_scopes.size() > 0)
		return errorAt(stmt.location(), "use all can only appear at global scope");
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
			return errorAt(stmt.location(), "imported variable ('%.*s') name is the same as it's alias name", newName->length(), newName->data());

		emitOp(Op::CloneTop); // getField consumes the value so the TOS has to be cloned.
		TRY(getField(originalName));
		TRY(createVariable(newName.has_value() ? *newName : originalName, stmt.location()));
	}
	emitOp(Op::PopStack); // Pop the module.
	return Status::Ok;
}

Compiler::Status Compiler::compile(const std::unique_ptr<Expr>& expr)
{
#define CASE_EXPR_TYPE(exprType, exprFunction) \
	case ExprType::exprType: TRY(exprFunction(*static_cast<exprType##Expr*>(expr.get()))); break;

	m_lineNumberStack.push_back(m_sourceInfo->getLine(expr->start));
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
		CASE_EXPR_TYPE(List, listExpr)
		CASE_EXPR_TYPE(GetField, getFieldExpr)
		CASE_EXPR_TYPE(Lambda, lambdaExpr)
		CASE_EXPR_TYPE(Stmt, stmtExpr)
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
	return loadVariable(expr.identifier);
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
		TRY(setVariable(lhs, expr.location()));
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

	return errorAt(expr.location(), "invalid left side of assignment");
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

Compiler::Status Compiler::listExpr(const ListExpr& expr)
{
	// Could allocate with capacity to fit all the values.
	// Python optimizes by evaluating all elements frist and using an instruction that takes the size of the list if there are less than or exacly 30
	// elements else it just creates array of size 0 and uses append.
	emitOp(Op::CreateList);
	for (const auto& value : expr.values)
	{
		TRY(compile(value));
		emitOp(Op::ListPush);
	}
	return Status::Ok;
}

Compiler::Status Compiler::lambdaExpr(const LambdaExpr& expr)
{
	static constexpr std::string_view ANONYMOUS_FUNCTION_NAME = "";
	const auto [nameConstant, name] = m_allocator.allocateStringConstant(ANONYMOUS_FUNCTION_NAME);
	const auto [functionConstant, function] =
		m_allocator.allocateFunctionConstant(name, static_cast<int>(expr.arguments.size()), &m_module->globals);
	TRY(compileFunction(function, expr.arguments, expr.stmts, expr.location()));
	TRY(loadConstant(functionConstant));
	return Status::Ok;
}

Compiler::Status Compiler::getFieldExpr(const GetFieldExpr& expr)
{
	TRY(compile(expr.lhs));
	TRY(getField(expr.fieldName));
	return Status::Ok;
}

Compiler::Status Compiler::stmtExpr(const StmtExpr& /*expr*/)
{
	ASSERT_NOT_REACHED();
	return Status::Error;
	//if (expr.stmt->type == StmtType::Ret)
	//{
	//	const auto& stmt = *static_cast<RetStmt*>(expr.stmt.get());
	//	if ((m_statementExpressions.size() == 0) || (m_statementExpressions.back().functionDepth != currentFunctionDepth()))
	//		return errorAt(expr, "@ret can only be used inside statement expressions");
	//	if (stmt.returnValue.has_value() == false)
	//		return errorAt(expr, "must return a value");
	//	TRY(compile(*stmt.returnValue));
	//	emitOp(Op::ExpressionStatementReturn);
	//	return Status::Ok;
	//}

	//auto isStmtAllowedInStmtExpr = [](const std::unique_ptr<Stmt>& stmt) {
	//	// TODO: Maybe give better error messages.
	//	// Using a switch to get warnings if this is not exhaustive.
	//	switch (stmt->type)
	//	{
	//	case StmtType::Expr: return false;
	//	case StmtType::VariableDeclaration: return false;
	//	case StmtType::Block: return true;
	//	case StmtType::Fn: return false;
	//	case StmtType::Ret: ASSERT_NOT_REACHED(); return false;
	//	case StmtType::If: return true;
	//	case StmtType::Loop: return true;
	//	case StmtType::Break: return false;
	//	case StmtType::Class: return false;
	//	case StmtType::Impl: return false;
	//	case StmtType::Try: return true;
	//	case StmtType::Throw: return false;
	//	case StmtType::Match: return true;
	//	case StmtType::Use: return false;
	//	case StmtType::UseAll: return false;
	//	case StmtType::UseSelective: return false;
	//	}
	//	return false;
	//};

	//if (isStmtAllowedInStmtExpr(expr.stmt) == false)
	//	return errorAt(*expr.stmt.get(), "statement not allowed in statment expression");

	//// This could be implemented by during runtime storing the stack state at the beginning of the expr and the restoring that
	//// stack top on return. 
	//// This could also be implmented by tracking the stack state inside the compiler and having an instruction that takes TOS and
	//// pops n elements then pushes the previous TOS back on top again. A similiar optmization could be applied to returns so the stack
	// before the call wouldn't need to be saved on inside the stack frame.
	//// Another way to make it efficient is to make only the last expression always returned like rust does. This be easy to implement
	//// becuase it would only need to not pop the expression from the expression statement and also it would be easier to check if the
	//// expression always returns a value.
	//// The syntax could also be changed maybe from '@ret <expr>' to just '@<expr>' 
	//m_statementExpressions.push_back(StatementExpression{ currentFunctionDepth() });
	//emitOp(Op::ExpressionStatementBegin);
	//TRY(compile(expr.stmt));
	//m_statementExpressions.pop_back();
	//return Status::Ok;
}

// The expression being matches must be TOS.
Compiler::Status Compiler::compile(const std::unique_ptr<Ptrn>& ptrn)
{
#define CASE_PTRN_TYPE(ptrnType, ptrnFunction) \
	case PtrnType::ptrnType: TRY(ptrnFunction(*static_cast<ptrnType##Ptrn*>(ptrn.get()))); break;	

	m_lineNumberStack.push_back(m_sourceInfo->getLine(ptrn->start));
	switch (ptrn->type)
	{
		CASE_PTRN_TYPE(Class, classPtrn)
		CASE_PTRN_TYPE(Expr, exprPtrn)
		CASE_PTRN_TYPE(AlwaysTrue, alwaysTruePtrn)
	}
#undef CASE_PTRN_TYPE

	return Status::Ok;
}

Compiler::Status Compiler::alwaysTruePtrn(const AlwaysTruePtrn& /*ptrn*/)
{
	// TODO: This could be optmizied by removing the check inside matchStmt when the PtrnType is AlwaysTrue.
	emitOp(Op::LoadTrue);
	return Status::Ok;
}

Compiler::Status Compiler::exprPtrn(const ExprPtrn& ptrn)
{
	emitOp(Op::CloneTop);
	TRY(compile(ptrn.expr));
	emitOp(Op::Equals);
	return Status::Ok;
}

Compiler::Status Compiler::classPtrn(const ClassPtrn& ptrn)
{
	TRY(loadVariable(ptrn.className));
	emitOp(Op::MatchClass);

	if (ptrn.fieldPtrns.size() > 0)
	{
		const auto jumpToEndOfPtrn0 = emitJump(Op::JumpIfFalse);
		emitOp(Op::PopStack); // Pop result of MatchClass.
		std::vector<size_t> jumpsIfFailed;
		for (const auto& [name, pattern] : ptrn.fieldPtrns)
		{
			emitOp(Op::CloneTop);
			TRY(getField(name));
			TRY(compile(pattern));
			jumpsIfFailed.push_back(emitJump(Op::JumpIfFalseAndPop));
			emitOp(Op::PopStack);
		}
		const auto jumpIfMatched = emitJump(Op::Jump);

		for (const auto jump : jumpsIfFailed)
		{
			setJumpToHere(jump);
		}
		emitOp(Op::PopStack); // Pop the field
		emitOp(Op::LoadFalse);
		const auto jumpToEndOfPtrn1 = emitJump(Op::Jump);

		setJumpToHere(jumpIfMatched);
		emitOp(Op::LoadTrue);

		setJumpToHere(jumpToEndOfPtrn0);
		setJumpToHere(jumpToEndOfPtrn1);
	}	
	return Status::Ok;
}

Compiler::Status Compiler::createVariableImplementation(std::string_view name, const SourceLocation& location)
{
	if (m_scopes.size() == 0)
	{
		TRY(loadConstant(m_allocator.allocateStringConstant(name).constant));
		emitOp(Op::CreateGlobal);
		return Status::Ok;
	}

	auto& locals = currentScope().localVariables;
	const auto variable = locals.find(name);
	if (variable != locals.end())
	{
		return errorAt(location, "redeclaration of variable '%.*s'", name.size(), name.data());
	}
	// TODO: Make this better maybe change scopes to store the count or maybe store it with a local.
	size_t localsCount = 0;
	const auto functionDepth = currentScope().functionDepth;
	for (auto scope = m_scopes.crbegin(); scope != m_scopes.crend(); scope++)
	{
		if (scope->functionDepth != functionDepth)
			break;
		localsCount += scope->localVariables.size();
	}
	locals[name] = Local{ static_cast<uint32_t>(localsCount), false };

	return Status::Ok;
}

// Could make the function get variable location which would return a sum type with all the possible options 
// but just using a flag to for load or set is simpler.
Compiler::Status Compiler::variable(std::string_view name, bool trueIfLoadFalseIfSet)
{
	for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++)
	{
		auto& scope = *it;
		auto local = scope.localVariables.find(name);
		if (local != scope.localVariables.end())
		{
			auto& variable = local->second;
			if (scope.functionDepth != currentFunctionDepth())
			{
				variable.isCaptured = true;

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

				if (trueIfLoadFalseIfSet)
					emitOp(Op::GetUpvalue);
				else
					emitOp(Op::SetUpvalue);
				emitUint32(static_cast<uint32_t>(lastIndex));
			}
			else
			{
				if (trueIfLoadFalseIfSet)
					emitOp(Op::GetLocal);
				else
					emitOp(Op::SetLocal);
				emitUint32(static_cast<uint32_t>(local->second.index));
			}
			
			return Status::Ok;
		}
	}

	const auto constant = m_allocator.allocateStringConstant(name).constant;
	TRY(loadConstant(constant));
	if (trueIfLoadFalseIfSet)
		emitOp(Op::GetGlobal);
	else
		emitOp(Op::SetGlobal);
	return Status::Ok;
}

Compiler::Status Compiler::loadVariable(std::string_view name)
{
	return variable(name, true);
}

Compiler::Status Compiler::setVariable(std::string_view name, const SourceLocation& location)
{
	if (canVariableBeCreatedAndAssigned(name) == false)
		return errorAt(location, "cannot assign to special variables");
	return variable(name, false);
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

Compiler::Status Compiler::emitOpArg(Op op, size_t arg, const SourceLocation& location)
{
	if (arg > std::numeric_limits<uint32_t>::max())
	{
		return errorAt(location, "argument size too big");
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

int Compiler::currentFunctionDepth()
{
	return currentScope().functionDepth;
}

Compiler::Status Compiler::createVariable(std::string_view name, const SourceLocation& location)
{
	if (canVariableBeCreatedAndAssigned(name) == false)
		return errorAt(location, "user defined variables cannot start with '$'");
	return createVariableImplementation(name, location);
}

Compiler::Status Compiler::createSpecialVariable(std::string_view name, const SourceLocation& location)
{
	return createVariableImplementation(name, location);
}

void Compiler::beginScope(ScopeType scopeType)
{
	if (m_scopes.size() == 0)
	{
		m_scopes.push_back(Scope{ {}, 0, scopeType, {} });
	}
	else
	{
		m_scopes.push_back(Scope{ {}, currentFunctionDepth(), scopeType, {} });
	}
}

void Compiler::endScope()
{
	ASSERT(m_scopes.size() > 0);

	if ((currentScope().functionDepth == 0)
	// No need to pop the values if the function return pops them.
		|| ((m_scopes.size() > 1) && (currentScope().functionDepth == (&currentScope())[-1].functionDepth)))
	{
		scopeCleanUp(currentScope());
	}

	m_scopes.pop_back();
}

Compiler::Scope& Compiler::currentScope()
{
	return m_scopes.back();
}

void Compiler::popOffLocals(const Scope& scope)
{
	// Could also execute this code on return so it doesn't need to be done at runtime.
	// Not sure if there are any cases when it would not work.
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

void Compiler::scopeCleanUp(const Scope& scope)
{
	popOffLocals(scope);
	if (scope.type == ScopeType::Try)
	{
		emitOp(Op::TryEnd);
	}
	else if (scope.type == ScopeType::Finally)
	{
		emitOp(Op::FinallyEnd);
	}
}

Compiler::Status Compiler::cleanUpBeforeJumpingOutOfScope(const Scope& scope, bool popOffLocals)
{
	if (popOffLocals)
	{
		Compiler::popOffLocals(scope);
	}

	if ((scope.type == ScopeType::Try))
	{
		emitOp(Op::TryEnd);
		currentByteCode().append(*scope.try_.finallyBlockByteCode);
	}
	else if ((scope.type == ScopeType::Catch))
	{
		currentByteCode().append(*scope.catch_.finallyBlockByteCode);
	}
	return Status::Ok;
}

bool Compiler::canVariableBeCreatedAndAssigned(std::string_view name)
{
	if (name.size() > 0)
		return (name[0] != '$');
	return true;
}

Compiler::Status Compiler::errorAt(const SourceLocation& location, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorReporter->onCompilerError(location, formatToTempBuffer(format, args));
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
