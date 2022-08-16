#pragma once

#include <stdarg.h>
#include <string_view>

namespace Voxl
{

std::string_view formatToTempBuffer(const char* format, va_list args);

}