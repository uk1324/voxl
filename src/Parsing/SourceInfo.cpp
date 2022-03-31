#include <Parsing/SourceInfo.hpp>
#include <assert.h>

using namespace Lang;

std::string_view SourceInfo::getLineText(size_t line) const
{
	assert(line < lineStartOffsets.size());

	size_t lineStart = lineStartOffsets[line];

	if ((line + 1) == lineStartOffsets.size())
	{
		return std::string_view(source.data() + lineStart, source.length() - lineStart);
	}

	size_t lineEnd = lineStartOffsets[line + 1];

	return std::string_view(source.data() + lineStart, lineEnd - lineStart);
}

size_t SourceInfo::getLine(size_t offsetInFile) const
{
	// + 1 because end offset points one element past position.
	assert(offsetInFile < source.length() + 1);

	// Could use binary search here
	for (size_t i = 0; i < lineStartOffsets.size(); i++)
	{
		if (offsetInFile <= lineStartOffsets[i])
		{
			return i;
		}
	}
	return lineStartOffsets.size() - 1;
}
