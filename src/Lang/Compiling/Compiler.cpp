#include <Lang/Compiling/Compiler.hpp>
#include <Utils/Assertions.hpp>

#include <iostream>

using namespace Lang;


Compiler::Compiler()
	: m_errorPrinter(nullptr)
	, m_hadError(false)
{
}

Compiler::Result Compiler::compile(const std::vector<OwnPtr<Stmt>>& ast, ErrorPrinter* errorPrinter)
{
	m_program = Program{};
	m_currentScope = std::nullopt;

	for (const auto& stmt : ast)
	{
		stmt->accept(*this);
	}

	emitOp(Op::Return);

	return Result{ m_hadError, std::move(m_program) };
}

void Compiler::visitExprStmt(const ExprStmt& stmt)
{
	stmt.expr->accept(*this);
	emitOp(Op::PopStack);
}

void Compiler::visitPrintStmt(const PrintStmt& stmt)
{
	stmt.expr->accept(*this);
	emitOp(Op::Print);
	emitOp(Op::PopStack);
}

void Compiler::visitLetStmt(const LetStmt& stmt)
{
	if (m_currentScope.has_value())
	{
		ASSERT((*m_currentScope)->localVariables.find(stmt.name) == (*m_currentScope)->localVariables.end());
		(*m_currentScope)->localVariables[stmt.name] = Local{ uint32_t((*m_currentScope)->localVariables.size()) };
		if (stmt.initializer.has_value())
		{
			// The expression result on top of the stack becomes the variable location.
			stmt.initializer.value()->accept(*this);
		}
		else
		{
			// Because there is no null value to make space for there variable a just leave it uninitalized.
			// Initializations are checked at compile time so this shouldn't be a problem.
			emitOp(Op::IncrementStack);
		}
	}
	else
	{
		ASSERT(m_gobalVariables.find(stmt.name) == m_gobalVariables.end());
		m_gobalVariables[stmt.name] = Global{ uint32_t(m_gobalVariables.size()) };
		if (stmt.initializer.has_value())
		{
			stmt.initializer.value()->accept(*this);
			emitOp(Op::IncrementGlobals);
			emitOp(Op::SetGlobal);
			emitUint32(m_gobalVariables.size() - 1);
			emitOp(Op::PopStack);
		}
		else
		{
			emitOp(Op::IncrementGlobals);
		}
	}
}

void Compiler::visitBinaryExpr(const BinaryExpr& expr)
{
	expr.lhs->accept(*this);
	expr.rhs->accept(*this);
	switch (expr.op.type)
	{
		case TokenType::Plus:
			if (expr.dataType.type == DataTypeType::Int)
			{
				emitOp(Op::AddInt);
			}
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
	loadVariable(expr.name);
}

uint32_t Compiler::createConstant(Value value)
{
	m_program.constants.push_back(value);
	return static_cast<uint32_t>(m_program.constants.size() - 1);
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
	if (m_currentScope.has_value())
	{
		ASSERT((*m_currentScope)->localVariables.find(name) != (*m_currentScope)->localVariables.end());
		uint32_t variable = (*m_currentScope)->localVariables[name].index;
		emitOp(Op::LoadLocal);
		emitUint32(variable);
	}
	else 
	{
		ASSERT(m_gobalVariables.find(name) != m_gobalVariables.end());
		uint32_t variable = m_gobalVariables[name].index;
		emitOp(Op::LoadGlobal);
		emitUint32(variable);
	}
}
