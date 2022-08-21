#include <ReadFile.hpp>
#include <fstream>
#include <iostream>

std::optional<std::string> Voxl::stringFromFile(std::string_view path)
{
	std::ifstream file(path.data(), std::ios::binary);

	if (file.fail())
	{
		return std::nullopt;
	}

	auto start = file.tellg();
	file.seekg(0, std::ios::end);
	auto end = file.tellg();
	file.seekg(start);
	auto fileSize = end - start;

	std::string result;
	// Pointless memset
	result.resize(fileSize);

	file.read(result.data(), fileSize);
	if (file.fail())
	{
		return std::nullopt;
	}

	return result;
}