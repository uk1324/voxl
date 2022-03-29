#pragma once

#include <Lang/Ast/Expr.hpp>
#include <Lang/Ast/Stmt.hpp>
#include <Lang/Parsing/Token.hpp>
#include <Lang/Parsing/SourceInfo.hpp>
#include <Lang/ErrorPrinter.hpp>
#include <Utils/SmartPointers.hpp>

#include <vector>

namespace Lang
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
			std::vector<OwnPtr<Stmt>> ast;
		};

	public:
		Parser();

		// Could use move here on return.
		Result parse(const std::vector<Token>& tokens, ErrorPrinter& errorPrinter, const SourceInfo& sourceInfo);

	private:
		OwnPtr<Stmt> stmt();
		OwnPtr<Stmt> exprStmt();
		OwnPtr<Stmt> printStmt();
		OwnPtr<Stmt> letStmt();

		OwnPtr<Expr> expr();
		OwnPtr<Expr> factor();
		OwnPtr<Expr> primary();

		const Token& peek() const;
		const Token& peekPrevious() const;
		void advance();
		bool isAtEnd();
		bool match(TokenType type);
		void expect(TokenType type, const char* format, ...);
		void synchronize();
		ParsingError errorAt(size_t start, size_t end, const char* format, ...);
		ParsingError errorAt(const Token& token, const char* format, ...);
		void errorAtImplementation(size_t start, size_t end, const char* format, va_list args);

	private:
		const std::vector<Token>* m_tokens;
		size_t m_currentTokenIndex;

		ErrorPrinter* m_errorPrinter;
		bool m_hadError;

		const SourceInfo* m_sourceInfo;
	};
}