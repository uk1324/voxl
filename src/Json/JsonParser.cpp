#include <Json/JsonParser.hpp>

#include <iostream>

#include <optional>
#include <charconv>

namespace
{
	class Parser
	{
	public:
		Parser();
		std::optional<Json::Value> parse(std::string_view text);

	private:
		std::optional<Json::Value> value();

		std::optional<Json::Value::StringType> string();
		std::optional<Json::Value> stringValue();
		[[nodiscard]] bool consumeUnescapedUtf8Char(Json::Value::StringType& string);
		std::optional<int> countRequiredOctets(char c);
		bool isValidUnescapedAsciiChar(char c);
		[[nodiscard]] bool consumeEscapedUtf8Char(Json::Value::StringType& string);
		std::optional<int> hexDigitToInt(char c);
		std::optional<Json::Value> object();
		std::optional<Json::Value> array();
		std::optional<Json::Value> keyword();
		std::optional<Json::Value> number();

		char peek();
		bool isAtEnd();
		void advance();
		bool match(char c);
		void skipWhitespace();
		bool isDigit(char c);
		bool isAlpha(char c);



	private:
		static constexpr int MAX_NESTING_DEPTH = 64;
		int m_nestingDepth;

		std::string_view m_text;
		size_t m_currentCharIndex;
	};

	Parser::Parser()
		: m_nestingDepth(0)
		, m_currentCharIndex(0)
	{}

	std::optional<Json::Value> Parser::parse(std::string_view view)
	{
		m_nestingDepth = 0;
		m_currentCharIndex = 0;
		m_text = view;

		auto val = value();
		skipWhitespace();
		if (isAtEnd() == false)
			return std::nullopt;
		return val;
	}

	std::optional<Json::Value> Parser::value()
	{
		skipWhitespace();

		char c = peek();

		switch (c)
		{
			case '"': return stringValue();
			case '{': return object();
			case '[': return array();
			case '-': return number();

			default:
				if (isDigit(c))
					return number();
				else if (isAlpha(c))
					return keyword();

				return std::nullopt;
		}
	}

	std::optional<Json::Value::StringType> Parser::string()
	{
		advance(); // Skip '"'

		Json::Value::StringType string;

		while (match('"') == false)
		{
			if (isAtEnd())
				return std::nullopt;

			if (match('\\'))
			{
				if (consumeEscapedUtf8Char(string) == false)
					return std::nullopt;
			}
			else
			{
				if (consumeUnescapedUtf8Char(string) == false)
					return std::nullopt;
			}

		}

		return string;
	}

	std::optional<Json::Value> Parser::stringValue()
	{
		auto value = string();
		if (value.has_value())
			return Json::Value(std::move(value.value()));
		return std::nullopt;
	}

	bool Parser::consumeUnescapedUtf8Char(Json::Value::StringType& string)
	{
		// Char. number range  |        UTF-8 octet sequence
		//    (hexadecimal)    |              (binary)
		// --------------------+---------------------------------------------
		// 0000 0000-0000 007F | 0xxxxxxx
		// 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
		// 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
		// 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

		// There are more invalid UTF-8 sequences like the ones reserved for UTF-16 but I don't want to deal with that.

		auto octets = countRequiredOctets(peek());

		if (octets.has_value() == false)
			return false;

		// If no additional octets are required it is an ascii char
		if ((octets.value() == 0) && (isValidUnescapedAsciiChar(peek()) == false))
			return false;

		string += peek();
		advance();

		for (int i = 0; i < octets.value(); i++)
		{
			char chr = peek();
			// Continuation bytes must start with bits 10
			if ((chr & 0b11000000) != 0b10000000)
				return false;
			string += chr;
			advance();
		}

		return true;
	}

	std::optional<int> Parser::countRequiredOctets(char c)
	{
		// ASCII
		if ((c >= 0x00) && (c <= 0x7F))
			return 0;

		// ANDing with required bits + 1 so 5 leading bits are not allowed.

		if ((c & 0b11111000) == 0b11110000)
			return 3;
		if ((c & 0b11110000) == 0b11100000)
			return 2;
		if ((c & 0b11100000) == 0b11000000)
			return 1;

		return std::nullopt;
	}

	bool Parser::isValidUnescapedAsciiChar(char chr)
	{
		// unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
		// '"' == 0x22
		// '\' == 0x5C

		return (chr >= 0x20 && chr <= 0x21)
			|| (chr >= 0x23 && chr <= 0x5B)
			|| (chr >= 0x5D && chr <= 0x7F);
	}

	bool Parser::consumeEscapedUtf8Char(Json::Value::StringType& string)
	{
		char c = peek();
		advance();

		switch (c)
		{
			case '"':  string += '"';  break;
			case '\\': string += '\\'; break;
			case '/':  string += '/';  break;
			case 'b':  string += '\b'; break;
			case 'f':  string += '\f'; break;
			case 'n':  string += '\n'; break;
			case 'r':  string += '\r'; break;
			case 't':  string += '\t'; break;
			case 'u':
				for (int i = 0; i < 2; i++)
				{
					auto digit1 = hexDigitToInt(peek());
					advance();
					auto digit2 = hexDigitToInt(peek());
					advance();

					if ((digit1.has_value() == false) || (digit2.has_value() == false))
						return false;

					string += static_cast<char>(digit1.value() * 0x10 + digit2.value());
				}
				break;

			default:
				return false;
		}
		return true;
	}

	std::optional<int> Parser::hexDigitToInt(char c)
	{
		if ((c >= 'A') && (c <= 'F'))
		{
			return c - 'A' + 10;
		}
		else if ((c >= 'a') && (c <= 'f'))
		{
			return c - 'a' + 10;
		}
		else if ((c >= '0') && (c <= '9'))
		{
			return c - '0';
		}

		return std::nullopt;
	}

	std::optional<Json::Value> Parser::object()
	{
		m_nestingDepth++;
		if (m_nestingDepth > MAX_NESTING_DEPTH)
			return std::nullopt;

		advance(); // Skip '{'

		Json::Value object = Json::Value::emptyObject();

		if (match('}'))
			return object;

		do
		{
			skipWhitespace();

			auto key = string();
			if (key.has_value() == false)
				return std::nullopt;

			skipWhitespace();
			if (match(':') == false)
				return std::nullopt;
			skipWhitespace();

			auto val = value();
			if (val.has_value() == false)
				return std::nullopt;

			object[key.value()] = val.value();

			skipWhitespace();

		} while (match(','));

		if (match('}') == false)
			return std::nullopt;

		m_nestingDepth--;
		return object;
	}

	std::optional<Json::Value> Parser::array()
	{
		m_nestingDepth++;
		if (m_nestingDepth > MAX_NESTING_DEPTH)
			return std::nullopt;

		advance(); // Skip '['

		Json::Value array = Json::Value::emptyArray();

		skipWhitespace();

		if (match(']'))
			return array;

		do
		{
			skipWhitespace();

			auto val = value();
			if (val.has_value() == false)
				return std::nullopt;

			array.array().push_back(val.value());

			skipWhitespace();

		} while (match(','));

		if (match(']') == false)
			return std::nullopt;

		m_nestingDepth--;
		return array;
	}

	std::optional<Json::Value> Parser::keyword()
	{
		#define MATCH_KEYWORD(text, value) \
			case text[0]: \
				if (((m_text.length() - m_currentCharIndex) >= (sizeof(text) - 1)) \
				&&   (memcmp(m_text.data() + m_currentCharIndex + 1, text + 1, sizeof(text) - 2) == 0)) \
				{ \
					m_currentCharIndex += sizeof(text) - 1; \
					return value; \
				} \
				break;

		switch (peek())
		{
			MATCH_KEYWORD("true",  Json::Value(true))
			MATCH_KEYWORD("false", Json::Value(false))
			MATCH_KEYWORD("null",  Json::Value(nullptr))				
		}

		return std::nullopt;

		#undef MATCH_KEYWORD
	}

	std::optional<Json::Value> Parser::number()
	{
		size_t numberStart = m_currentCharIndex;

		bool isFloat = false;

		// Just minus at start
		if (match('-') && (isDigit(peek()) == false))
			return std::nullopt;

		// Leading zeros not allowed
		if (match('0') && isDigit(peek()))
		{
			return std::nullopt;
		}
		else
		{
			while ((isAtEnd() == false) && (isDigit(peek())))
			{
				advance();
			}
		}

		if ((isAtEnd() == false) && (match('.')))
		{
			isFloat = true;
			// Numbers can't end with a dot
			if (isDigit(peek()) == false)
				return std::nullopt;

			while ((isAtEnd() == false) && isDigit(peek()))
				advance();
		}

		if ((isAtEnd() == false) && (match('e') || match('E')))
		{
			isFloat = true;
			// Numbers can't end with a '-' or '+'
			if ((match('-') || match('+')) && (isDigit(peek()) == false))
				return std::nullopt;

			// Numbers can't end with an 'E'
			else if (isDigit(peek()) == false)
				return std::nullopt;

			while ((isAtEnd() == false) && (isDigit(peek())))
			{
				advance();
			}
		}

		const char* first = m_text.data() + numberStart;
		const char* last = m_text.data() + m_currentCharIndex;

		if (isFloat)
		{
			Json::Value::FloatType value;
			auto result = std::from_chars(first, last, value);
			return ((bool(result.ec) == false) && (result.ptr == last))
				? std::optional<Json::Value>(value)
				: std::nullopt;
		}
		else
		{
			Json::Value::IntType value;
			auto result = std::from_chars(first, last, value, 10);
			return ((bool(result.ec) == false) && (result.ptr == last))
				? std::optional<Json::Value>(value)
				: std::nullopt;
		}
	}

	char Parser::peek()
	{
		if (isAtEnd())
			return '\0';
		return m_text[m_currentCharIndex];
	}

	bool Parser::isAtEnd()
	{
		return m_currentCharIndex >= m_text.length();
	}

	void Parser::advance()
	{
		m_currentCharIndex++;
	}

	bool Parser::match(char c)
	{
		if (peek() == c)
		{
			advance();
			return true;
		}
		return false;
	}

	void Parser::skipWhitespace()
	{
		while (match(' ') || match('\n') || match('\t') || match('\r'))
			;
	}

	bool Parser::isDigit(char c)
	{
		return (c >= '0') && (c <= '9');
	}

	bool Parser::isAlpha(char c)
	{
		return (c >= 'a') && (c <= 'z')
			|| (c >= 'A') && (c <= 'Z');
	}
}

Json::Value Json::parse(std::string_view text)
{
	Parser parser;
	auto value = parser.parse(text);
	if (value.has_value())
		return value.value();
	else
		throw Json::ParsingError();
}