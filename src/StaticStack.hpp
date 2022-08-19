#pragma once

#include <Asserts.hpp>
#include <stddef.h>
#include <stdint.h>

template<typename T, size_t SIZE>
class StaticStack
{
public:
	struct ConstReverseIterator
	{
		ConstReverseIterator& operator++();
		const T& operator*() const;
		const T* operator->();
		bool operator==(const ConstReverseIterator& other) const;
		bool operator!=(const ConstReverseIterator& other) const;

		const T* ptr;
	};

public:
	StaticStack();
	~StaticStack();

	[[nodiscard]] bool push(const T& value);
	[[nodiscard]] bool push();
	void pop();
	T& peek(size_t i);
	T& top();
	const T& top() const;
	T& operator[](size_t i);
	const T& operator[](size_t i) const;
	bool isEmpty() const;

	T* data();
	const T* data() const;
	size_t size() const;
	size_t maxSize() const;
	T* begin();
	T* end();
	const T* cbegin() const;
	const T* cend() const;
	ConstReverseIterator crbegin() const;
	ConstReverseIterator crend() const;
	void clear();

public:
	T* topPtr;
private:
	alignas(T) uint8_t m_data[SIZE * sizeof(T)];
};

template<typename T, size_t SIZE>
StaticStack<T, SIZE>::StaticStack()
	: topPtr(data())
{}

template<typename T, size_t SIZE>
StaticStack<T, SIZE>::~StaticStack()
{
	for (auto& item : *this)
	{
		item.~T();
	}
}

template<typename T, size_t SIZE>
bool StaticStack<T, SIZE>::push(const T& value)
{
	if (topPtr >= data() + maxSize())
	{
		return false;
	}
	*topPtr = value;
	topPtr++;
	return true;
}

template<typename T, size_t SIZE>
bool StaticStack<T, SIZE>::push()
{
	if (topPtr >= data() + maxSize())
	{
		return false;
	}
	topPtr++;
	return true;
}

template<typename T, size_t SIZE>
void StaticStack<T, SIZE>::pop()
{
	ASSERT(topPtr != data());
	topPtr--;
	topPtr->~T();
}

template<typename T, size_t SIZE>
T& StaticStack<T, SIZE>::peek(size_t i)
{
	return *(topPtr - 1 - i);
}

template<typename T, size_t SIZE>
T& StaticStack<T, SIZE>::top()
{
	ASSERT(size() > 0);
	return *(topPtr - 1);
}

template<typename T, size_t SIZE>
const T& StaticStack<T, SIZE>::top() const
{
	return *(topPtr - 1);
}

template<typename T, size_t SIZE>
T& StaticStack<T, SIZE>::operator[](size_t i)
{
	return data()[i];
}

template<typename T, size_t SIZE>
const T& StaticStack<T, SIZE>::operator[](size_t i) const
{
	return data()[i];
}

template<typename T, size_t SIZE>
bool StaticStack<T, SIZE>::isEmpty() const
{
	return topPtr == data();
}

template<typename T, size_t SIZE>
T* StaticStack<T, SIZE>::data()
{
	return reinterpret_cast<T*>(m_data);
}

template<typename T, size_t SIZE>
const T* StaticStack<T, SIZE>::data() const
{
	return reinterpret_cast<const T*>(m_data);
}

template<typename T, size_t SIZE>
inline size_t StaticStack<T, SIZE>::size() const
{
	return topPtr - data();
}

template<typename T, size_t SIZE>
size_t StaticStack<T, SIZE>::maxSize() const
{
	return SIZE;
}

template<typename T, size_t SIZE>
T* StaticStack<T, SIZE>::begin()
{
	return data();
}

template<typename T, size_t SIZE>
T* StaticStack<T, SIZE>::end()
{
	return topPtr;
}

template<typename T, size_t SIZE>
const T* StaticStack<T, SIZE>::cbegin() const
{
	return data();
}

template<typename T, size_t SIZE>
const T* StaticStack<T, SIZE>::cend() const
{
	return topPtr;
}

template<typename T, size_t SIZE>
typename StaticStack<T, SIZE>::ConstReverseIterator StaticStack<T, SIZE>::crbegin() const
{
	return ConstReverseIterator{ topPtr - 1 };
}

template<typename T, size_t SIZE>
typename StaticStack<T, SIZE>::ConstReverseIterator StaticStack<T, SIZE>::crend() const
{
	return ConstReverseIterator{ data() - 1 };
}

template<typename T, size_t SIZE>
void StaticStack<T, SIZE>::clear()
{
	for (auto& item : *this)
	{
		item.~T();
	}
	topPtr = data();
}

template<typename T, size_t SIZE>
typename StaticStack<T, SIZE>::ConstReverseIterator& StaticStack<T, SIZE>::ConstReverseIterator::operator++()
{
	ptr--;
	return *this;
}

template<typename T, size_t SIZE>
const T& StaticStack<T, SIZE>::ConstReverseIterator::operator*() const
{
	return *ptr;
}

template<typename T, size_t SIZE>
const T* StaticStack<T, SIZE>::ConstReverseIterator::operator->()
{
	return ptr;
}

template<typename T, size_t SIZE>
bool StaticStack<T, SIZE>::ConstReverseIterator::operator==(const ConstReverseIterator& other) const
{
	return ptr == other.ptr;
}

template<typename T, size_t SIZE>
bool StaticStack<T, SIZE>::ConstReverseIterator::operator!=(const ConstReverseIterator& other) const
{
	return ptr != other.ptr;
}
