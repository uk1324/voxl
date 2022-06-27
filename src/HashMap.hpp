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
// TODO: Make this non templated and implement getting a key from a string view instead of ObjString
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
		m_capacity = (m_capacity == 0) ? INITIAL_SIZE : capacity() * 2;
		m_data = reinterpret_cast<Bucket*>(::operator new(sizeof(Bucket) * m_capacity));
		m_size = 0;
		setAllKeysToNull();

		for (size_t i = 0; i < oldCapacity; i++)
		{
			const auto& [key, value] = oldData[i];
			if ((KeyTraits::isKeyNull(key) == false) && (KeyTraits::isKeyTombstone(key) == false))
			{
				const auto isNewItem = set(key, value);
				// TODO: This can be slightly optmized by changing the call to set() with just 
				// inserting the item without doing the check if it is a new item becuase
				// when rehashing it should always be a new key.
				ASSERT(isNewItem);
			}
		}

		::operator delete(oldData);
	}

	auto& bucket = findBucket(newKey);
	const auto isNewItem = isBucketEmpty(bucket);
	if (isNewItem)
		m_size++;

	bucket.key = newKey;
	bucket.value = newValue;

	return isNewItem;
}

template<typename Key, typename Value, typename KeyTraits>
bool Lang::HashMap<Key, Value, KeyTraits>::remove(const Key& key)
{
	if (m_size == 0)
		return false;

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
	
	Bucket* tombstone = nullptr;

	for (;;)
	{
		auto& bucket = data()[index];
		
		if (KeyTraits::isKeyNull(bucket.key))
		{
			return (tombstone == nullptr) ? bucket : *tombstone;
		}
		else if (KeyTraits::isKeyTombstone(bucket.key))
		{
			// This check makes it so the first found tombstone is always used.
			// When using linear probing this check makes it so the elements are inserted closer
			// to their ideal spot.
			// Example without the if check
			// [_, 1, 2, _]
			// [_, T, T, _]
			// [_, T, 1, _]
			// [_, T, 1, 2]
			// The probe length increased for both buckets and an extra tombstone exists.
			if (tombstone == nullptr)
			{
				tombstone = &bucket;
			}
		}
		else if (KeyTraits::compareKeys(bucket.key, key))
		{
			return bucket;
		}

		// TODO: Find a better probing method
		// Robing hood probing and siwsstables implementation are apparently good.
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