#pragma once 

#include <optional>

template<typename T>
class std::optional<T&>
{
public:
	constexpr optional();
	constexpr optional(T& value);
	constexpr optional(std::nullopt_t);

	constexpr bool has_value() const noexcept;

	constexpr optional<T&>& operator=(const optional<T&> other);
	constexpr optional<T&>& operator=(std::nullopt_t);

	constexpr const T* operator->() const noexcept;
	constexpr T* operator->() noexcept;
	constexpr const T& operator*() const noexcept;
	constexpr T& operator*() noexcept;

private:
	T* value;
};

template<typename T>
constexpr std::optional<T&>::optional()
	: value(nullptr)
{}

template<typename T>
constexpr std::optional<T&>::optional(T& value)
	: value(&value)
{}

template<typename T>
constexpr std::optional<T&>::optional(std::nullopt_t)
	: value(nullptr)
{}

template<typename T>
constexpr bool std::optional<T&>::has_value() const noexcept
{
	return value != nullptr;
}

template<typename T>
constexpr std::optional<T&>& std::optional<T&>::operator=(const std::optional<T&> other)
{
	value = other.value;
	return *this;
}

template<typename T>
constexpr std::optional<T&>& std::optional<T&>::operator=(std::nullopt_t)
{
	value = nullptr;
	return *this;
}

template<typename T>
constexpr const T* std::optional<T&>::operator->() const noexcept
{
	return value;
}

template<typename T>
constexpr T* std::optional<T&>::operator->() noexcept
{
	return value;
}

template<typename T>
constexpr const T& std::optional<T&>::operator*() const noexcept
{
	return *value;
}

template<typename T>
constexpr T& std::optional<T&>::operator*() noexcept
{
	return *value;
}