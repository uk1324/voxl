#include <Json/JsonPrinter.hpp>
#include <Json/JsonPrinter.hpp>

#include <ostream>
#include <sstream>

static void escapeString(std::ostream& os, const Json::Value::StringType string)
{
	// unescaped = 0x20-0x21 / 0x23-0x5B / 0x5D-0x10FFFF

	for (char chr : string)
	{
		switch (chr)
		{
			case '"':
				os << "\\\"";
				break;
			case '\\':
				os << "\\\\";
				break;
			case '\b':
				os << "\\b";
				break;
			case '\f':
				os << "\\f";
				break;
			case '\n':
				os << "\\n";
				break;
			case '\r':
				os << "\\r";
				break;
			case '\t':
				os << "\\t";
				break;
			default:
				static constexpr char hexToInt[] = "0123456789abcdef";
				unsigned char c = chr;
				if (c < 0x10)
				{
					os << "\\u000" << hexToInt[c];
				}
				else if (c < 0x20)
				{
					os << "\\u001" << hexToInt[c - 0x10];
				}
				else
				{
					os << c;
				}
				break;
		}
	}
}

static void printIndentation(std::ostream& os, int count)
{
	for (int i = 0; i < count; i++)
		os << "    ";
}

static void prettyPrintImplementation(std::ostream& os, const Json::Value& value, int depth)
{
	switch (value.type())
	{
		case Json::Value::Type::Int:
			os << value.intNumber();
			break;
		case Json::Value::Type::Float:
			os << value.floatNumber();
			break;
		case Json::Value::Type::Null:
			os << "null";
			break;
		case Json::Value::Type::Boolean:
			os << (value.boolean() ? "true" : "false");
			break;
		case Json::Value::Type::String:
			os << '"';
			escapeString(os, value.string());
			os << '"';
			break;
		case Json::Value::Type::Array:
			if (value.array().empty())
			{
				os << "[]";
			}
			else
			{
				os << "[\n";
				for (auto it = value.array().cbegin(); it != value.array().cend() - 1; it++)
				{
					printIndentation(os, depth);

					// This if is here because you want to print objects and arrays as properites like
					// "abc": {
					// ...
					// }
					// But in arrays like
					// [
					//		{
					//		...
					//		}
					// ]
					if (it->isArray() || it->isObject())
						prettyPrintImplementation(os, *it, depth + 1);
					else
						prettyPrintImplementation(os, *it, depth);
					os << ",\n";
				}
				printIndentation(os, depth);

				if ((value.array().cend() - 1)->isArray() || (value.array().cend() - 1)->isObject())
					prettyPrintImplementation(os, *(value.array().cend() - 1), depth + 1);
				else
					prettyPrintImplementation(os, *(value.array().cend() - 1), depth);

				os << "\n";

				printIndentation(os, depth - 1);
				os << "]";
			}
			break;

		case Json::Value::Type::Object:
			if (value.object().empty())
			{
				os << "{}";
			}
			else
			{
				// std::unordered_map doesn't overload the subtraction operator so the decrement operator is used.
				os << "{\n";
				for (auto it = value.object().cbegin(); it != (--value.object().cend()); it++)
				{
					printIndentation(os, depth);
					os << '"' << it->first << "\": ";
					prettyPrintImplementation(os, it->second, depth + 1);
					os << ",\n";
				}
				printIndentation(os, depth);
				os << '"' << (--value.object().cend())->first << "\": ";
				prettyPrintImplementation(os, (--value.object().cend())->second, depth + 1);
				os << "\n";

				printIndentation(os, depth - 1);
				os << "}";
			}
			break;
	}
}

static void printImplementation(std::ostream& os, const Json::Value& value)
{
	switch (value.type())
	{
	case Json::Value::Type::Int:
		os << value.intNumber();
		break;
	case Json::Value::Type::Float:
		os << value.floatNumber();
		break;
	case Json::Value::Type::Null:
		os << "null";
		break;
	case Json::Value::Type::Boolean:
		os << (value.boolean() ? "true" : "false");
		break;
	case Json::Value::Type::String:
		os << '"';
		escapeString(os, value.string());
		os << '"';
		break;
	case Json::Value::Type::Array:
		if (value.array().empty())
		{
			os << "[]";
		}
		else
		{
			os << '[';
			for (auto it = value.array().cbegin(); it != value.array().cend() - 1; it++)
			{
				printImplementation(os, *it);
				os << ',';
			}
			printImplementation(os, *(value.array().cend() - 1));
			os << ']';
		}
		break;

	case Json::Value::Type::Object:
		if (value.object().empty())
		{
			os << "{}";
		}
		else
		{
			// std::unordered_map doesn't overload the subtraction operator so the decrement operator is used.
			os << "{";
			for (auto it = value.object().cbegin(); it != (--value.object().cend()); it++)
			{
				os << '"' << it->first << "\":";
				printImplementation(os, it->second);
				os << ',';
			}
			os << '"' << (--value.object().cend())->first << "\":";
			printImplementation(os, (--value.object().cend())->second);
			os << "}";
		}
		break;
	}
}

std::ostream& operator<<(std::ostream& os, const Json::Value& json)
{
	return Json::prettyPrint(os, json);
}

std::ostream& Json::prettyPrint(std::ostream& os, const Value& json)
{
	prettyPrintImplementation(os, json, 1);
	return os;
}

std::ostream& Json::print(std::ostream& os, const Value& json)
{
	printImplementation(os, json);
	return os;
}

std::string Json::stringify(const Value& json)
{
	std::stringstream stream;
	print(stream, json);
	return stream.str();
}
