#pragma once

#include <string_view>
#include <optional>

namespace Voxl
{
	std::optional<std::string> stringFromFile(std::string_view path);
}
