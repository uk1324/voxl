#include <Lang/Debug/AstJsonifier.hpp>

#include <string>

using namespace Lang;

// Maybe make function for accept and return.

// Don't know if the move actually does anything because node is a local object so rvo might apply.
#define RETURN(value) m_returnValue = std::move((value))
// Also don't know if the move does anything but in this case it probably does because of m_returnValue being non local.
#define RETURN_VALUE std::move(m_returnValue)

Json::Value AstJsonifier::jsonify(const std::vector<std::unique_ptr<Stmt>>& ast)
{
	Json::Value json = Json::Value::emptyArray();

	for (const auto& stmt : ast)
	{
		toJson(stmt);
		json.array().push_back(std::move(m_returnValue));
	}
	
	return json;
}

void AstJsonifier::visitBinaryExpr(const BinaryExpr& expr)
{
	Json::Value node = Json::Value::emptyObject();
	addCommon(node, expr);
	node["operator"] = std::string(expr.op.text);
	toJson(expr.lhs);
	node["left"] = RETURN_VALUE;
	toJson(expr.rhs);
	node["right"] = RETURN_VALUE;
	RETURN(node);
}

void AstJsonifier::visitIntConstantExpr(const IntConstantExpr& expr)
{
	Json::Value node = Json::Value::emptyObject();
	addCommon(node, expr);
	node["value"] = Json::Value::IntType(expr.value);
	RETURN(node);
}

void AstJsonifier::visitFloatConstantExpr(const FloatConstantExpr& expr)
{
	Json::Value node = Json::Value::emptyObject();
	addCommon(node, expr);
	node["value"] = Json::Value::FloatType(expr.value);
	RETURN(node);
}

void AstJsonifier::visitIdentifierExpr(const IdentifierExpr& expr)
{
	Json::Value node = Json::Value::emptyObject();
	addCommon(node, expr);
	node["name"] = Json::Value::StringType(expr.name.text);
	RETURN(node);
}

void AstJsonifier::visitExprStmt(const ExprStmt& stmt)
{
	Json::Value node = Json::Value::emptyObject();
	addCommon(node, stmt);
	toJson(stmt.expr);
	node["expression"] = RETURN_VALUE;
	RETURN(node);
}

void AstJsonifier::visitPrintStmt(const PrintStmt& stmt)
{
	auto node = Json::Value::emptyObject();
	addCommon(node, stmt);
	toJson(stmt.expr);
	node["expression"] = RETURN_VALUE;
	RETURN(node);
}

void AstJsonifier::visitLetStmt(const LetStmt& stmt)
{
	auto node = Json::Value::emptyObject();
	addCommon(node, stmt);
	if (stmt.initializer.has_value())
	{
		toJson(stmt.initializer.value());
		node["initializer"] = RETURN_VALUE;
	}
	else
	{
		node["initializer"] = Json::Value::null();
	}

	node["name"] = Json::Value(std::string(stmt.name.text));
	
	RETURN(node);
}

void AstJsonifier::toJson(const OwnPtr<Expr>& expr)
{
	expr->accept(*this);
}

void AstJsonifier::toJson(const OwnPtr<Stmt>& stmt)
{
	stmt->accept(*this);
}

void AstJsonifier::addCommon(Json::Value& node, const Expr& expr)
{
	node["start"] = Json::Value::IntType(expr.start);
	node["length"] = Json::Value::IntType(expr.length);
	node["type"] = exprTypeToString(expr.type());
}

void AstJsonifier::addCommon(Json::Value& node, const Stmt& stmt)
{
	node["start"] = Json::Value::IntType(stmt.start);
	node["length"] = Json::Value::IntType(stmt.length);
	node["type"] = stmtTypeToString(stmt.type());
}