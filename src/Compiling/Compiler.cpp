#include <Compiling/Compiler.hpp>

#include <iostream>

using namespace Lang;

Compiler::Compiler()
	: m_hadError(false)
	, m_allocator(nullptr)
{}

Compiler::Result Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& ast, Allocator& allocator)
{
	m_hadError = false;
	m_program = Program{};
	m_currentScope = std::nullopt;

	for (const auto& stmt : ast)
	{
		try
		{
			//stmt->accept(*this);
		}
		catch (const Error&)
		{
			break;
		}
	}

	emitOp(Op::Return);

	return Result{ m_hadError, std::move(m_program) };
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
	//va_start(args, format);
	//m_errorPrinter->at(start, end, format, args);
	//va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Stmt& stmt, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(stmt.start, stmt.start + stmt.length, format, args);
	//va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Expr& expr, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(expr.start, expr.start + expr.length, format, args);
	//va_end(args);
	return Error{};
}

Compiler::Error Compiler::errorAt(const Token& token, const char* format, ...)
{
	m_hadError = true;
	va_list args;
	//va_start(args, format);
	//m_errorPrinter->at(token.start, token.end, format, args);
	//va_end(args);
	return Error{};
}
