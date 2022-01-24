#include <Lang/TypeChecking/TypeChecker.hpp>


// Not going to be spending time on error recoveryright now.
// Could probably recover from undefined variable errors by defining a variable with the error type
// and allowing variables with an error type to be redeclared.

using namespace Lang;

TypeChecker::TypeChecker()
	: m_errorPrinter(nullptr)
	, m_hadError(false)
{}

TypeChecker::Result TypeChecker::checkAndInferTypes(std::vector<OwnPtr<Stmt>>& ast, ErrorPrinter& errorPrinter)
{
	m_errorPrinter = &errorPrinter;
	m_hadError = false;
	m_currentScope = std::nullopt;
	for (const auto& stmt : ast)
	{
		try
		{
			stmt->accept(*this);
		}
		catch (const Error&)
		{
			//std::cout << "type error: " << error.message << '\n';
			//return;
			break;
		}
	}

	return Result{ m_hadError };
}

void TypeChecker::visitExprStmt(ExprStmt& stmt)
{
	stmt.expr->accept(*this);
}

void TypeChecker::visitPrintStmt(PrintStmt& stmt)
{
	stmt.expr->accept(*this);
}

void TypeChecker::visitLetStmt(LetStmt& stmt)
{
	Variable variable;

	if (stmt.initializer.has_value())
	{
		variable.isInitialized = true;
		(*stmt.initializer)->accept(*this);

		if (stmt.dataType.type == DataTypeType::Dynamic)
		{
			return;
		}

		if (stmt.dataType.type == DataTypeType::Unspecified)
		{
			stmt.dataType = (*stmt.initializer)->dataType;
			variable.dataType = (*stmt.initializer)->dataType;
			declareVariable(stmt.name, variable);
		}
		else
		{
			if (stmt.dataType.type != (*stmt.initializer)->dataType.type)
			{
				if ((*stmt.initializer)->dataType.type == DataTypeType::Error)
				{
					throw errorAt(stmt, "can't initialize variable of type a with type b");
				}
				stmt.dataType = DataType(DataTypeType::Error);
				variable.dataType = (*stmt.initializer)->dataType;
				declareVariable(stmt.name, variable);
				return;
			}
		}
	}
	else
	{
		variable.isInitialized = false;
		if (stmt.dataType.type == DataTypeType::Dynamic)
		{
			return;
		}

		if (stmt.dataType.type == DataTypeType::Unspecified)
		{
			throw errorAt(stmt, "types don't match");
			stmt.dataType = DataType(DataTypeType::Error);
			variable.dataType = (*stmt.initializer)->dataType;
			declareVariable(stmt.name, variable);
			return;
		}

	}
}

void TypeChecker::visitBinaryExpr(BinaryExpr& expr)
{
	expr.lhs->accept(*this);
	expr.rhs->accept(*this);
	// Don't know if I should check if the operator matches types here on in the compiler.
	// constness and comparing??
	if (expr.lhs->dataType.type != expr.lhs->dataType.type)
	{
		throw errorAt(expr, "types don't match");
		expr.dataType = DataType(DataTypeType::Error);
		return;
	}
	expr.dataType = expr.lhs->dataType;
}

void TypeChecker::visitIntConstantExpr(IntConstantExpr& expr)
{
	expr.dataType = DataType(DataTypeType::Int);
}

void TypeChecker::visitFloatConstantExpr(FloatConstantExpr& expr)
{
	expr.dataType = DataType(DataTypeType::Float);
}

void TypeChecker::visitIdentifierExpr(IdentifierExpr& expr)
{
	Opt<Scope*> scope = m_currentScope;

	while (scope.has_value())
	{
		const auto& variable = (*scope)->localVariables.find(expr.name);
		if (variable == (*scope)->localVariables.end())
		{
			scope = (*scope)->previousScope;
		}
		else
		{
			expr.dataType = variable->second.dataType;
			return;
		}
	}

	const auto& variable = m_gobalVariables.find(expr.name);
	if (variable != m_gobalVariables.end())
	{
		expr.dataType = variable->second.dataType;
		return;
	}

	throw errorAt(expr, "undefined variable");
}

void TypeChecker::declareVariable(const Token& name, const Variable& variable)
{
	if (m_currentScope.has_value())
	{
		if ((*m_currentScope)->localVariables.find(name) != (*m_currentScope)->localVariables.end())
		{
			//throw errorAt(name, "can't redeclare a variable");
		}
		(*m_currentScope)->localVariables[name] = variable;
	}
	else
	{
		if (m_gobalVariables.find(name) != m_gobalVariables.end())
		{
			//throw Parser::ParsingError("can't redeclare a variable", name);
		}
		m_gobalVariables[name] = variable;
	}

}

TypeChecker::Error TypeChecker::errorAt(size_t start, size_t end, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	errorAtImplementation(start, end, format, args);
	va_end(args);
	return Error{};
}

TypeChecker::Error TypeChecker::errorAt(const Expr& expr, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	errorAtImplementation(expr.start, expr.start + expr.length, format, args);
	va_end(args);
	return Error{};
}

TypeChecker::Error TypeChecker::errorAt(const Stmt& stmt, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	errorAtImplementation(stmt.start, stmt.start + stmt.length, format, args);
	va_end(args);
	return Error{};
}

void TypeChecker::errorAtImplementation(size_t start, size_t end, const char* format, va_list args)
{
	m_hadError = true;
	m_errorPrinter->at(start, end, format, args);
}
