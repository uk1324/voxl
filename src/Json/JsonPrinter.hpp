#pragma once

#include <Json/JsonValue.hpp>

std::ostream& operator<< (std::ostream& os, const Json::Value& json);

namespace Json
{
	std::ostream& prettyPrint(std::ostream& os, const Value& json);
	std::ostream& print(std::ostream& os, const Value& json);
	std::string stringify(const Value& json);
}
