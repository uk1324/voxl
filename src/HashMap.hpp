#pragma once

#include <Allocator.hpp>
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

	void print();

	static constexpr size_t INITIAL_SIZE = 8;
	static constexpr float MAX_LOAD_FACTOR = 0.75f;

private:
	Bucket* data();
	size_t capacity() const;
	void setAllKeysToNull();

private:
	size_t m_size;
	ObjAllocation* m_data;
};

}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::init(HashMap& map, Lang::Allocator& allocator)
{
	map.m_data = allocator.allocateRawMemory(sizeof(Bucket) * INITIAL_SIZE);
	map.setAllKeysToNull();
	map.m_size = 0;
}

template<typename Key, typename Value, typename KeyTraits>
bool Lang::HashMap<Key, Value, KeyTraits>::insert(Allocator& allocator, const Key& key, Value value)
{
	if ((static_cast<float>(m_size + 1) / static_cast<float>(capacity())) > MAX_LOAD_FACTOR)
	{
		const auto oldData = data();
		const auto oldCapacity = capacity();
		m_data = allocator.allocateRawMemory(sizeof(Bucket) * oldCapacity * 2);
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

	auto& bucket = findBucket(key);
	bucket.key = key;
	bucket.value = value;
	m_size += 1;

	return true;
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

		index = (index + 1) % m_data->size;
	}
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
	return reinterpret_cast<Bucket*>(m_data->data());
}

template<typename Key, typename Value, typename KeyTraits>
size_t Lang::HashMap<Key, Value, KeyTraits>::capacity() const
{
	return m_data->size / sizeof(Bucket);
}

template<typename Key, typename Value, typename KeyTraits>
void Lang::HashMap<Key, Value, KeyTraits>::setAllKeysToNull()
{
	for (size_t i = 0; i < capacity(); i++)
	{
		KeyTraits::setKeyNull(data()[i].key);
	}
}