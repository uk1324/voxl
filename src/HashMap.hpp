#pragma once

#include <Asserts.hpp>
#include <optional>

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

class Allocator;
struct ObjAllocation;

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
	static void init(HashMap& map, Allocator& allocator);
	bool insert(Allocator& allocator, const Key& key, Value value);
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

public:
	ObjAllocation* allocation;
private:
	size_t m_size;
};

}

#include <Allocator.hpp>

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::init(HashMap& map, Lang::Allocator& allocator)
{
	map.allocation = nullptr;
	map.m_size = 0;
	// Cannot allocate an initial size becuase the GC might run and update the pointers on the stack.
	// This map would still refer to the old instance and the new instance would be left in an invalid state.
	// It may be possible way to fix this by setting allocation to nullptr and the size to zero 
	// before doing the allocation so even if the GC happens the new one will copy the old ones valid state.
	// TODO: A better way to fix this would be to merge the allocation for the instance and the hash map.
	// This would allow to remove the nullptr checks in other HashMap functions.
}

template<typename Key, typename Value, typename KeyTraits>
bool Lang::HashMap<Key, Value, KeyTraits>::insert(Allocator& allocator, const Key& newKey, Value newValue)
{
	if ((allocation == nullptr) || ((static_cast<float>(m_size + 1) / static_cast<float>(capacity())) > MAX_LOAD_FACTOR))
	{
		const auto oldData = (allocation == nullptr) ? nullptr : data();
		const auto oldCapacity = (allocation == nullptr) ? 0 : capacity();
		auto newCapacity = (allocation == nullptr) ? INITIAL_SIZE : capacity() * 2;
		allocation = allocator.allocateRawMemory(sizeof(Bucket) * newCapacity);
		setAllKeysToNull();

		for (size_t i = 0; i < oldCapacity; i++)
		{
			const auto& [key, value] = oldData[i];
			if ((KeyTraits::isKeyNull(key) == false) && (KeyTraits::isKeyTombstone(key) == false))
			{
				insert(allocator, key, value);
			}
		}
	}

	auto& bucket = findBucket(newKey);
	bool wasNotAlreadySet = isBucketEmpty(bucket);
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
	if (allocation == nullptr)
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
	auto index = KeyTraits::hashKey(key) % capacity();
	
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
	return reinterpret_cast<Bucket*>(allocation->data());
}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::clear()
{
	m_size = 0;
	if (allocation != nullptr)
	{
		for (size_t i = 0; i < capacity(); i++)
		{
			KeyTraits::setKeyNull(data()[i].key);
		}
	}
}

template<typename Key, typename Value, typename KeyTraits>
size_t Lang::HashMap<Key, Value, KeyTraits>::capacity() const
{
	return allocation->size / sizeof(Bucket);
}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::setAllKeysToNull()
{
	for (size_t i = 0; i < capacity(); i++)
	{
		KeyTraits::setKeyNull(data()[i].key);
	}
}