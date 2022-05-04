#pragma once

#include <ByteCode.hpp>
#include <Allocator.hpp>

// If I want to use instructions like extend arg the has to treat the extend arg and everything after it as one instruction
// for it to make sense. I would need to make extend arg a special case in the vm.
// Check if is extend arg if yes then count all the extends after it or if not extend arg execute any other instruction and print it .

namespace Lang
{

void debugPrintValue(const Value& value);
size_t disassembleInstruction(const ByteCode& byteCode, size_t offset, const Allocator& allocator);
void disassembleByteCode(const ByteCode& byteCode, const Allocator& allocator);

}