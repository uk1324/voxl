#include <Lang/Compiling/Compiler.hpp>
#include <Utils/Assertions.hpp>

#include <iostream>

using namespace Lang;

Compiler::Compiler()
	: m_errorPrinter(nullptr)
	, m_hadError(false)
	, m_allocator(nullptr)
{}

Compiler::Result Compiler::compile(const std::vector<OwnPtr<Stmt>>& ast, ErrorPrinter& errorPrinter, Allocator& allocator)
{
	m_hadError = false;
	m_program = Program{};
	m_currentScope = std::nullopt;
	m_errorPrinter = &errorPrinter;

	for (const auto& stmt : ast)
	{
		try
		{
			stmt->accept(*this);
		}
		catch (const Error&)
		{
			break;
		}
	}

	emitOp(Op::Return);

	return Result{ m_hadError, std::move(m_program) };
}

void Compiler::compile(const Stmt& stmt)
{
	stmt.accept(*this);
}

void Compiler::compile(const Expr& expr)
{
	expr.accept(*this);
}

void Compiler::visitExprStmt(const ExprStmt& stmt)
{
	compile(stmt);
	emitOp(Op::PopStack);
}

void Compiler::visitPrintStmt(const PrintStmt& stmt)
{
	compile(stmt);
	emitOp(Op::Print);
	emitOp(Op::PopStack);
}

void Compiler::visitLetStmt(const LetStmt& stmt)
{
	if (m_currentScope.has_value())
	{
		if ((*m_currentScope)->localVariables.find(stmt.name) != (*m_currentScope)->localVariables.end())
		{
			throw errorAt(stmt, "redeclaration of variable %.*s", stmt.name.text.length(), stmt.name.text.data());
		}

		(*m_currentScope)->localVariables[stmt.name] = Local{ uint32_t((*m_currentScope)->localVariables.size()) };
		// The expression result on top of the stack becomes the variable location.
		if (stmt.initializer.has_value())
		{
			compile(**stmt.initializer);
		}
		else
		{
			emitOp(Op::LoadNull);
		}
	}
	else
	{
		loadConstant(createIdentifierConstant(stmt.name));
		emitOp(Op::DefineGlobal);

		if (stmt.initializer.has_value())
		{
			compile(**stmt.initializer);
			emitOp(Op::SetGlobal);
			emitOp(Op::PopStack);
		}
		emitOp(Op::PopStack);
	}
}

void Compiler::visitBinaryExpr(const BinaryExpr& expr)
{
	compile(*expr.lhs);
	compile(*expr.rhs);
	switch (expr.op.type)
	{
		case TokenType::Plus:
			emitOp(Op::Add);
			break;

		default:
			ASSERT_NOT_REACHED();
	}
}

void Compiler::visitIntConstantExpr(const IntConstantExpr& expr)
{
	auto constant = createConstant(Value(expr.value));
	loadConstant(constant);
}

void Compiler::visitFloatConstantExpr(const FloatConstantExpr& expr)
{

}

void Compiler::visitIdentifierExpr(const IdentifierExpr& expr)
{
	if (m_currentScope.has_value())
	{
		if ((*m_currentScope)->localVariables.find(expr.name) == (*m_currentScope)->localVariables.end())
		{
			throw errorAt(expr, "use of undeclared variable '%.*s'", expr.name.text.length(), expr.name.text.data());
		}
		uint32_t variable = (*m_currentScope)->localVariables[expr.name].index;
		emitOp(Op::LoadLocal);
		emitUint32(variable);
	}
	else
	{
		emitOp(Op::LoadGlobal);
		emitUint32(createIdentifierConstant(expr.name));
	}
}

uint32_t Compiler::createConstant(Value value)
{
	m_program.constants.push_back(value);
	return static_cast<uint32_t>(m_program.constants.size() - 1);
}

uint32_t Compiler::createIdentifierConstant(const Token& name)
{
	auto string = m_allocator->allocateString(name.text);
	auto constant = createConstant(Value(string));
}

void Compiler::loadConstant(uint32_t index)
{
	emitOp(Op::LoadConstant);
	emitUint32(index);
}

void Compiler::emitOp(Op op)
{
	m_program.code.emitOp(op);
}

void Compiler::emitUint32(uint32_t value)
{
	m_program.code.emitDword(value);
}

void Compiler::declareVariable(const Token& name)
{
	//if (m_currentScope.has_value())
	//{
	//	ASSERT((*m_currentScope)->localVariables.find(name) == (*m_currentScope)->localVariables.end());
	//	(*m_currentScope)->localVariables[name] = Local{ (*m_currentScope)->localVariables.size() };
	//	return;
	//}

	//ASSERT(m_gobalVariables.find(name) == m_gobalVariables.end());
	//m_gobalVariables[name] = Global{ m_gobalVariables.size() };
}

void Compiler::loadVariable(const Token& name)
{

}

Compiler::Error Compiler::errorAt(size_t start, size_t end, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(start, end, format, args);
	va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Stmt& stmt, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(stmt.start, stmt.start + stmt.length, format, args);
	va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Expr& expr, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(expr.start, expr.start + expr.length, format, args);
	va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Token& token, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	va_start(args, format);
	m_errorPrinter->at(token.start, token.end, format, args);
	va_end(args);
	return Error{};
}
