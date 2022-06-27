#pragma once


#if defined(__clang__) || defined(__GNUC__)
	#define FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
	#define FUNCTION_NAME __FUNCSIG__
#endif

template<typename T>
inline constexpr char* typeIdImplementation()
{
	return FUNCTION_NAME;
}

#undef FUNCTION_NAME

#define TYPE_ID(type) reinterpret_cast<size_t>(typeIdImplementation<type>());