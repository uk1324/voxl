#pragma once

#include <unordered_map>
#include <Program.hpp>
#include <Ast.hpp>
#include <Allocator.hpp>

namespace Lang
{

class Compiler
{
public:
	class Result
	{
	public:
		bool hadError;
		Program program;
	};

private:
	struct Local
	{
	public:
		uint32_t index;
	};

	struct Global
	{
	public:
		uint32_t index;
	};

	/*struct Scope
	{
	public:
		std::unordered_map<Token, Local> localVariables;
		Opt<Scope*> previousScope;
	};*/

	class [[nodiscard]] Error
	{};

public:
	Compiler();

	Result compile(const std::vector<std::unique_ptr<Stmt>>& ast, Allocator& allocator);

	void compile(const Stmt& stmt);
	void compile(const Expr& expr);

private:
	uint32_t createConstant(Value value);
	uint32_t createIdentifierConstant(const Token& name);
	void loadConstant(uint32_t index);
	void emitOp(Op op);
	void emitUint32(uint32_t value);

	void declareVariable(const Token& name);
	void loadVariable(const Token& name);

	Compiler::Error errorAt(size_t start, size_t end, const char* format, ...);
	Compiler::Error errorAt(const Stmt& stmt, const char* format, ...);
	Compiler::Error errorAt(const Expr& expr, const char* format, ...);
	Compiler::Error errorAt(const Token& token, const char* format, ...);

private:
	Allocator* m_allocator;

	std::optional<void*> m_currentScope;

	bool m_hadError;

	Program m_program;
};

}