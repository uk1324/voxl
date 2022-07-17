#include <HashTable.hpp>
#include <Obj.hpp>
#include <iostream>

using namespace Lang;

HashTable::HashTable()
{
	m_size = 0;
	m_capacity = 0;
	m_data = nullptr;
}

bool HashTable::set(ObjString* newKey, Value newValue)
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
			if ((isKeyNull(key) == false) && (isKeyTombstone(key) == false))
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

bool HashTable::remove(const ObjString* key)
{
	if (m_size == 0)
		return false;

	auto& bucket = findBucket(key);
	if (isKeyTombstone(bucket.key) || isKeyNull(bucket.key))
	{
		return false;
	}
	setKeyTombstone(bucket.key);
	m_size--;
	return true;
}

std::optional<Value&> HashTable::get(const ObjString* key)
{
	if (m_size == 0)
		return std::nullopt;

	auto& bucket = findBucket(key);
	if (isKeyNull(bucket.key) || isKeyTombstone(bucket.key))
	{
		return std::nullopt;
	}
	return bucket.value;
}

HashTable::Bucket& HashTable::findBucket(const ObjString* key)
{
	auto index = hashKey(key) % m_capacity;

	Bucket* tombstone = nullptr;

	for (;;)
	{
		auto& bucket = data()[index];

		if (isKeyNull(bucket.key))
		{
			return (tombstone == nullptr) ? bucket : *tombstone;
		}
		else if (isKeyTombstone(bucket.key))
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
		else if (compareKeys(bucket.key, key))
		{
			return bucket;
		}

		// TODO: Find a better probing method
		// Robing hood probing and siwsstables implementation are apparently good.
		index = (index + 1) % capacity();
	}
}

bool HashTable::isBucketEmpty(const Bucket& bucket)
{
	return isKeyNull(bucket.key) || isKeyTombstone(bucket.key);
}

void HashTable::print()
{
	for (size_t i = 0; i < capacity(); i++)
	{
		const auto& [key, value] = data()[i];
		if ((isKeyNull(key) == false) && (isKeyTombstone(key) == false))
		{
			std::cout << key->chars << " : " << value << '\n';
		}
	}
}

HashTable::Bucket* HashTable::data()
{
	return m_data;
}

void HashTable::clear()
{
	m_size = 0;
	for (size_t i = 0; i < capacity(); i++)
	{
		setKeyNull(data()[i].key);
	}
}

size_t HashTable::capacity() const
{
	return m_capacity;
}

void HashTable::setAllKeysToNull()
{
	for (size_t i = 0; i < capacity(); i++)
	{
		setKeyNull(data()[i].key);
	}
}

bool HashTable::compareKeys(const ObjString* a, const ObjString* b)
{
	return (a->size == b->size) && (memcmp(a->chars, b->chars, a->size) == 0);
}

size_t HashTable::hashKey(const ObjString* key)
{
	return std::hash<std::string_view>()(std::string_view(key->chars, key->size));
}

void HashTable::setKeyNull(ObjString*& key)
{
	key = nullptr;
}

void HashTable::setKeyTombstone(ObjString*& key)
{
	key = reinterpret_cast<ObjString*>(1);
}

bool HashTable::isKeyNull(const ObjString* key)
{
	return key == nullptr;
}

bool HashTable::isKeyTombstone(const ObjString* key)
{
	return key == reinterpret_cast<ObjString*>(1);
}