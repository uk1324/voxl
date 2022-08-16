#pragma once

#include <iterator>

template<typename T>
class Span
{

public:
	Span(T* data, size_t size);

	T* data();
	const T* data() const;
	size_t size() const;

	T* begin();
	T* end();
	const T* cbegin() const;
	const T* cend() const;

	std::reverse_iterator<T*> rbegin();
	std::reverse_iterator<T*> rend();
	std::reverse_iterator<const T*> crbegin() const;
	std::reverse_iterator<const T*> crend() const;

private:
	T* m_data;
	size_t m_size;
};

template<typename T>
Span<T>::Span(T* data, size_t size)
	: m_data(data)
	, m_size(size)
{}

template<typename T>
T* Span<T>::data()
{
	return m_data;
}

template<typename T>
const T* Span<T>::data() const
{
	return m_data;
}

template<typename T>
size_t Span<T>::size() const
{
	return m_size;
}

template<typename T>
inline T* Span<T>::begin()
{
	return m_data;
}

template<typename T>
T* Span<T>::end()
{
	return m_data + m_size;
}

template<typename T>
const T* Span<T>::cbegin() const
{
	return m_data;
}

template<typename T>
const T* Span<T>::cend() const
{
	return m_data + m_size;
}

template<typename T>
std::reverse_iterator<T*> Span<T>::rbegin()
{
	return std::reverse_iterator(end());
}

template<typename T>
std::reverse_iterator<T*> Span<T>::rend()
{
	return std::reverse_iterator(begin());
}

template<typename T>
std::reverse_iterator<const T*> Span<T>::crbegin() const
{
	return std::reverse_iterator(cend());
}

template<typename T>
std::reverse_iterator<const T*> Span<T>::crend() const
{
	return std::reverse_iterator(cbegin());
}
