#pragma once 

#include <string_view>
#include <string>
#include <vector>

namespace Lang
{
	// Lines numbers start from 0
	struct SourceInfo
	{
	public:
		std::string_view getLineText(size_t line) const;
		size_t getLine(size_t offsetInFile) const;

	public:
		// Don't know if it is safe for this to be a std::string_view.
		std::string_view filename;
		std::string_view source;
		std::vector<size_t> lineStartOffsets;
	};
}