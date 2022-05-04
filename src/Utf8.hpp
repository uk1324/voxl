#pragma once

namespace Lang
{

namespace Utf8
{

// Expects a valid UTF-8 string.
size_t strlen(const char* str, size_t size);
int strcmp(const char* a, size_t aSize, const char* b, size_t bSize);

}

}