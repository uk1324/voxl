#include <ByteCode.hpp>

using namespace Voxl;

void ByteCode::append(const ByteCode& src)
{
	code.insert(code.end(), src.code.begin(), src.code.end());
	lineNumberAtOffset.insert(lineNumberAtOffset.end(), src.lineNumberAtOffset.begin(), src.lineNumberAtOffset.end());
}
