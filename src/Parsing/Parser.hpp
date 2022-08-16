#pragma once

#include <Ast.hpp>
#include <Parsing/Token.hpp>
#include <Parsing/SourceInfo.hpp>
#include <ErrorReporter.hpp>

#include <vector>

namespace Voxl
{

class Parser
{
public:
	// Maybe later add an optional hint/help parameter like rust
	class ParsingError
	{};

	class Result
	{
	public:
		bool hadError;
		bool errorAtEof;
		StmtList ast;
	};

public:
	Parser();
	Parser(bool ignoreEofErrors);

	Result parse(const std::vector<Token>& tokens, const SourceInfo& sourceInfo, ErrorReporter& errorReporter);

	// Remember to check isAtEnd() when using loops!
private:
	std::unique_ptr<Stmt> stmt();
	std::unique_ptr<Stmt> exprStmt();
	std::unique_ptr<Stmt> blockStmt();
	std::unique_ptr<Stmt> fnStmt();
	std::unique_ptr<Stmt> retStmt();
	std::unique_ptr<Stmt> ifStmt();
	std::unique_ptr<Stmt> loopStmt();
	std::unique_ptr<Stmt> whileStmt();
	std::unique_ptr<Stmt> forStmt();
	std::unique_ptr<Stmt> breakStmt();
	std::unique_ptr<Stmt> classStmt();
	std::unique_ptr<Stmt> implStmt();
	std::unique_ptr<Stmt> tryStmt();
	std::unique_ptr<Stmt> throwStmt();
	std::unique_ptr<Stmt> variableDeclarationStmt();
	std::unique_ptr<Stmt> matchStmt();
	std::unique_ptr<Stmt> useStmt();

	std::unique_ptr<FnStmt> function(size_t start);
	std::vector<std::unique_ptr<Stmt>> block();

	std::unique_ptr<Expr> expr();
	std::unique_ptr<Expr> stmtExpr();
	std::unique_ptr<Expr> assignment();
	std::unique_ptr<Expr> and();
	std::unique_ptr<Expr> or();
	std::unique_ptr<Expr> equality();
	std::unique_ptr<Expr> comparasion();
	std::unique_ptr<Expr> factor();
	std::unique_ptr<Expr> term();
	std::unique_ptr<Expr> unary();
	std::unique_ptr<Expr> callOrFieldAccessOrIndex();
	std::unique_ptr<Expr> primary();

	std::unique_ptr<Ptrn> ptrn();
	std::unique_ptr<Ptrn> classPtrn();

	const Token& peek() const;
	const Token& peekPrevious() const;
	const Token& peekNext() const;
	void advance();
	bool isAtEnd() const;
	bool match(TokenType type);
	bool check(TokenType type);
	void expect(TokenType type, const char* format, ...);
	void expectSemicolon();
	void synchronize();
	// Returning the value to prevent errors about not all paths returning a value.
	ParsingError errorAt(const Token& token, const char* format, ...);
	void errorAtImlementation(const Token& token, const char* format, va_list args);

private:
	const std::vector<Token>* m_tokens;
	size_t m_currentTokenIndex;

	bool m_hadError;
	bool m_ignoreEofErrors;

	const SourceInfo* m_sourceInfo;
	ErrorReporter* m_errorReporter;
};

}