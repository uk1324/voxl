#pragma once

#include <string_view>
#include <iostream>

void success(std::string_view testName);
void fail(std::string_view testName, int lineNumber, const char* filename);

#define SUCCESS() (success(__func__))
#define FAIL() (fail(__func__, __LINE__, __FILE__))

#define ASSERT_TRUE(expr) \
	do \
	{ \
		if ((expr) == false) \
		{ \
			FAIL(); \
			std::cerr << '\n'; \
			return; \
		} \
	} while (false)
	
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(got, expected) \
	do \
	{ \
		const auto g = (got); \
		const auto e = (expected); \
		if (g != e) \
		{ \
			FAIL(); \
			std::cerr << "expected " <<  e << " got " << g << '\n'; \
			return; \
		} \
	} while(false)

void hashTableTests();