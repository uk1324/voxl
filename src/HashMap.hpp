#pragma once

#include <Asserts.hpp>
#include <optional>
#include <iostream>

/* 
struct KeyTraits
{
	static bool compareKeys(const Key& a, const Key& b);
	static size_t hashKey(const Key& key);
	static void setKeyNull(Key& key);
	static void setKeyTombstone(Key& key);
	static bool isKeyNull(const Key& key);
	static bool isKeyTombstone(const Key& key);
}
*/

namespace Lang
{

template<typename Key, typename Value, typename KeyTraits>
class HashMap
{
public:
	struct Bucket
	{
		Key key;
		Value value;
	};

public:
	HashMap();
	bool set(const Key& key, Value value);
	bool remove(const Key& key);
	std::optional<Value*> get(const Key& key);
	Bucket& findBucket(const Key& key);

	static bool isBucketEmpty(const Bucket& bucket);
	void print();
	size_t capacity() const;
	Bucket* data();
	void clear();

	static constexpr size_t INITIAL_SIZE = 8;
	static constexpr float MAX_LOAD_FACTOR = 0.75f;

private:
	void setAllKeysToNull();

private:
	Bucket* m_data;
	size_t m_capacity;
	size_t m_size;
};

}

template<typename Key, typename Value, typename KeyTraits>
Lang::HashMap<Key, Value, KeyTraits>::HashMap()
{
	m_size = 0;
	m_capacity = 0;
	m_data = nullptr;
}

template<typename Key, typename Value, typename KeyTraits>
bool Lang::HashMap<Key, Value, KeyTraits>::set(const Key& newKey, Value newValue)
{
	if ((static_cast<float>(m_size + 1) / static_cast<float>(m_capacity)) > MAX_LOAD_FACTOR)
	{
		const auto oldData = m_data;
		const auto oldCapacity = m_capacity;
		m_capacity = (m_data == nullptr) ? INITIAL_SIZE : capacity() * 2;
		m_data = reinterpret_cast<Bucket*>(::operator new(sizeof(Bucket) * m_capacity));
		m_size = 0;
		setAllKeysToNull();

		for (size_t i = 0; i < oldCapacity; i++)
		{
			const auto& [key, value] = oldData[i];
			if ((KeyTraits::isKeyNull(key) == false) && (KeyTraits::isKeyTombstone(key) == false))
			{
				set(key, value);
			}
		}
	}

	auto& bucket = findBucket(newKey);
	const auto wasNotAlreadySet = isBucketEmpty(bucket);
	if (wasNotAlreadySet)
		m_size++;

	bucket.key = newKey;
	bucket.value = newValue;

	return wasNotAlreadySet;
}

template<typename Key, typename Value, typename KeyTraits>
bool Lang::HashMap<Key, Value, KeyTraits>::remove(const Key& key)
{
	auto& bucket = findBucket(key);
	if (KeyTraits::isKeyTombstone(bucket.key) || KeyTraits::isKeyNull(bucket.key))
	{
		return false;
	}
	KeyTraits::setKeyTombstone(bucket.key);
	m_size--;
	return true;
}

template<typename Key, typename Value, typename KeyTraits>
std::optional<Value*> Lang::HashMap<Key, Value, KeyTraits>::get(const Key& key)
{
	if (m_size == 0)
		return std::nullopt;
	auto& bucket = findBucket(key);
	if (KeyTraits::isKeyNull(bucket.key) || KeyTraits::isKeyTombstone(bucket.key))
	{
		return std::nullopt;
	}
	return &bucket.value;
}

template<typename Key, typename Value, typename KeyTraits>
typename Lang::HashMap<Key, Value, KeyTraits>::Bucket& Lang::HashMap<Key, Value, KeyTraits>::findBucket(const Key& key)
{
	auto index = KeyTraits::hashKey(key) % m_capacity;
	
	Bucket* grave = nullptr;

	for (;;)
	{
		auto& bucket = data()[index];
		
		if (KeyTraits::isKeyNull(bucket.key))
		{
			return (grave == nullptr) ? bucket : *grave;
		}
		else if (KeyTraits::isKeyTombstone(bucket.key))
		{
			grave = &bucket;
		}
		else if (KeyTraits::compareKeys(bucket.key, key))
		{
			return bucket;
		}

		index = (index + 1) % capacity();
	}
}

template<typename Key, typename Value, typename KeyTraits>
bool Lang::HashMap<Key, Value, KeyTraits>::isBucketEmpty(const Bucket& bucket)
{
	return KeyTraits::isKeyNull(bucket.key) || KeyTraits::isKeyTombstone(bucket.key);
}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::print()
{
	for (size_t i = 0; i < capacity(); i++)
	{
		const auto& [key, value] = data()[i];
		if ((KeyTraits::isKeyNull(key) == false) && (KeyTraits::isKeyTombstone(key) == false))
		{
			std::cout << key->chars << " : " << value << '\n';
		}
	}
}

template<typename Key, typename Value, typename KeyTraits>
typename Lang::HashMap<Key, Value, KeyTraits>::Bucket* Lang::HashMap<Key, Value, KeyTraits>::data()
{
	return m_data;
}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::clear()
{
	m_size = 0;
	for (size_t i = 0; i < capacity(); i++)
	{
		KeyTraits::setKeyNull(data()[i].key);
	}
}

template<typename Key, typename Value, typename KeyTraits>
size_t Lang::HashMap<Key, Value, KeyTraits>::capacity() const
{
	return m_capacity;
}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::setAllKeysToNull()
{
	for (size_t i = 0; i < capacity(); i++)
	{
		KeyTraits::setKeyNull(data()[i].key);
	}
}