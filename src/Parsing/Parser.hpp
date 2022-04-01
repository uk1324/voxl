#pragma once

#include <Ast.hpp>
#include <Parsing/Token.hpp>
#include <Parsing/SourceInfo.hpp>
#include <ErrorPrinter.hpp>

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
			std::vector<std::unique_ptr<Stmt>> ast;
		};

	public:
		Parser();

		// Could use move here on return.
		Result parse(const std::vector<Token>& tokens, const SourceInfo& sourceInfo, ErrorPrinter& errorPrinter);

	private:
		std::unique_ptr<Stmt> stmt();
		std::unique_ptr<Stmt> exprStmt();
		std::unique_ptr<Stmt> printStmt();
		std::unique_ptr<Stmt> letStmt();

		std::unique_ptr<Expr> expr();
		std::unique_ptr<Expr> factor();
		std::unique_ptr<Expr> primary();

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

		bool m_hadError;

		const SourceInfo* m_sourceInfo;
		ErrorPrinter* m_errorPrinter;
	};
}