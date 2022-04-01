#include <ByteCode.hpp>

using namespace Lang;

void ByteCode::emitOp(Op op)
{
	data.push_back(static_cast<uint8_t>(op));
}

void ByteCode::emitByte(uint8_t byte)
{
	data.push_back(byte);
}

void ByteCode::emitWord(uint16_t word)
{
	
}

void ByteCode::emitUint32(uint32_t dword)
{
	emitByte(static_cast<uint8_t>((dword >> 24) & 0xFF));
	emitByte(static_cast<uint8_t>((dword >> 16) & 0xFF));
	emitByte(static_cast<uint8_t>((dword >> 8) & 0xFF));
	emitByte(static_cast<uint8_t>(dword & 0xFF));
}
