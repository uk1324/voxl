#include <Format.hpp>
#include <Asserts.hpp>

// This is unsafe it would be better to allocate make a macro that allocates a buffer on the stack or just mallocs it.
std::string_view Voxl::formatToTempBuffer(const char* format, va_list args)
{
	static thread_local char buffer[2048];

	const auto bytesWritten = vsnprintf(buffer, sizeof(buffer), format, args);
	if (bytesWritten >= sizeof(buffer))
	{
		static constexpr char DOTS[] = "...";
		static constexpr size_t DOTS_SIZE = sizeof(DOTS) - 1; // Minus null byte
		memcpy(buffer + sizeof(buffer) - 1 - DOTS_SIZE, DOTS, DOTS_SIZE);
	}

	return std::string_view(buffer, (bytesWritten >= sizeof(buffer)) ? sizeof(buffer) : bytesWritten);
}
