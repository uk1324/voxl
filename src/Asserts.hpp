#pragma once

#include <Debug/DebugOptions.hpp>

#ifdef VOXL_EXECUTE_ASSERTS_IN_RELEASE
	#include <iostream>
	#define NOMINMAX
	#include <windows.h>
	#define ASSERT(condition) \
		do \
		{ \
			if ((condition) == false) \
			{ \
				std::cout << "assertion failed\n"; \
				DebugBreak(); \
			} \
		} while (false)
	#define ASSERT_NOT_REACHED() DebugBreak();
#else
	#include <assert.h>
	#define ASSERT assert
	#define ASSERT_NOT_REACHED() assert(false)
#endif