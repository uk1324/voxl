#include <Utf8.hpp>
#include <Asserts.hpp>

size_t Lang::Utf8::strlen(const char* str, size_t size)
{
	size_t length = 0;
	for (size_t i = 0; i < size;)
	{
		char c = str[i];
		if ((c & 0b1111'1000) == 0b1111'0000)
		{
			i += 4;
		}
		else if ((c & 0b1111'0000) == 0b1110'0000)
		{
			i += 3;
		}
		else if ((c & 0b1110'0000) == 0b1100'0000)
		{
			i += 2;
		}
		else if ((c & 0b1000'0000) == 0b0000'0000)
		{
			i += 1;
		}
		else
		{
			ASSERT_NOT_REACHED();
		}
		length++;
	}
	return length;
}

int Lang::Utf8::strcmp(const char* a, size_t aSize, const char* b, size_t bSize)
{
	if (aSize < bSize)
		return -1;

	if (aSize > bSize)
		return 1;

	for (size_t i = 0; i < aSize; i++)
	{
		if (a[i] < b[1])
		{
			return -1;
		}
		else if (a[i] > b[1])
		{
			return 1;
		}
	}

	return 0;
}
