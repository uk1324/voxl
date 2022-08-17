#include <HashTable.hpp>
#include <Obj.hpp>
#include <iostream>

using namespace Voxl;

HashTable::HashTable()
{
	m_size = 0;
	m_capacity = 0;
	m_data = nullptr;
}

bool HashTable::set(ObjString* key, const Value& value)
{
	resizeIfNeeded(m_size + 1);

	auto& bucket = findBucket(key);
	const auto isNewItem = isBucketEmpty(bucket);
	if (isNewItem)
		m_size++;

	bucket.key = key;
	bucket.value = value;

	return isNewItem;
}

bool HashTable::insertIfNotSet(ObjString* key, const Value& value)
{
	resizeIfNeeded(m_size + 1);

	auto& bucket = findBucket(key);
	const auto isNewItem = isBucketEmpty(bucket);
	if (isNewItem)
	{
		bucket.key = key;
		bucket.value = value;
		m_size++;
	}
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

bool HashTable::remove(std::string_view key)
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
	return getImplementation<const ObjString*>(key);
}

std::optional<Value&> HashTable::get(std::string_view key)
{
	return getImplementation<std::string_view>(key);
}

HashTable::Bucket& HashTable::findBucket(const ObjString* key)
{
	return findBucketImplementation<const ObjString*>(key);
}

HashTable::Bucket& HashTable::findBucket(std::string_view key)
{
	return findBucketImplementation<std::string_view>(key);
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

HashTable::Iterator HashTable::begin()
{
	auto bucket = m_data;
	while ((bucket < m_data + m_capacity) && isBucketEmpty(*bucket))
	{
		bucket++;
	}
	return Iterator(*this, bucket);
}

HashTable::Iterator HashTable::end()
{
	return Iterator(*this, m_data + m_capacity);
}

HashTable::ConstIterator HashTable::cbegin() const
{
	auto bucket = m_data;
	while ((bucket < m_data + m_capacity) && isBucketEmpty(*bucket))
	{
		bucket++;
	}
	return ConstIterator(*this, bucket);
}

HashTable::ConstIterator HashTable::cend() const
{
	return ConstIterator(*this, m_data + m_capacity);
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

void HashTable::resizeIfNeeded(size_t newSize)
{
	if ((static_cast<float>(newSize) / static_cast<float>(m_capacity)) > MAX_LOAD_FACTOR)
	{
		const auto oldData = m_data;
		const auto oldCapacity = m_capacity;
		m_capacity = (m_capacity == 0) ? INITIAL_SIZE : capacity() * 2;
		m_data = reinterpret_cast<Bucket*>(::operator new(sizeof(Bucket) * m_capacity));
		m_size = newSize;
		setAllKeysToNull();

		for (size_t i = 0; i < oldCapacity; i++)
		{
			const auto& [key, value] = oldData[i];
			if ((isKeyNull(key) == false) && (isKeyTombstone(key) == false))
			{
				// TODO: This can be slightly optmized by changing the call to set() with just 
				// inserting the item without doing the check if it is a new item becuase
				// when rehashing it should always be a new key.
				auto& bucket = findBucket(key);
				const auto isNewItem = isBucketEmpty(bucket);
				ASSERT(isNewItem);
				bucket.key = key;
				bucket.value = value;
			}
		}

		::operator delete(oldData);
	}
}

bool HashTable::compareKeys(const ObjString* a, const ObjString* b)
{
	return a == b;
}

bool HashTable::compareKeys(std::string_view a, const ObjString* b)
{
	return (a.size() == b->size) && (memcmp(a.data(), b->chars, a.size()) == 0);
}

size_t HashTable::hashKey(const ObjString* key)
{
	return key->hash;
}

size_t HashTable::hashKey(std::string_view key)
{
	return ObjString::hashString(key.data(), key.size());
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

HashTable::Iterator::Iterator(HashTable& hashTable, Bucket* bucket)
	: m_hashTable(hashTable)
	, m_bucket(bucket)
{}

HashTable::Bucket* HashTable::Iterator::operator->()
{
	return m_bucket;
}

HashTable::Bucket& HashTable::Iterator::operator*()
{
	return *m_bucket;
}

HashTable::Iterator& HashTable::Iterator::operator++()
{
	if (*this == m_hashTable.end())
		return *this;
	do
	{
		m_bucket++;
	} while (*this != m_hashTable.end() && HashTable::isBucketEmpty(*m_bucket));
	return *this;
}

bool HashTable::Iterator::operator==(const Iterator& other) const
{
	ASSERT(&m_hashTable == &other.m_hashTable);
	return m_bucket == other.m_bucket;
}

bool HashTable::Iterator::operator!=(const Iterator& other) const
{
	return !(*this == other);
}


HashTable::ConstIterator::ConstIterator(const HashTable& hashTable, const Bucket* bucket)
	: m_hashTable(hashTable)
	, m_bucket(bucket)
{}

const HashTable::Bucket* HashTable::ConstIterator::operator->() const
{
	return m_bucket;
}

const HashTable::Bucket& HashTable::ConstIterator::operator*() const
{
	return *m_bucket;
}

HashTable::ConstIterator& HashTable::ConstIterator::operator++()
{
	if (*this == m_hashTable.cend())
		return *this;
	do
	{
		m_bucket++;
	} while (*this != m_hashTable.cend() && HashTable::isBucketEmpty(*m_bucket));
	return *this;
}

bool HashTable::ConstIterator::operator==(const ConstIterator& other) const
{
	ASSERT(&m_hashTable == &other.m_hashTable);
	return m_bucket == other.m_bucket;
}

bool HashTable::ConstIterator::operator!=(const ConstIterator& other) const
{
	return !(*this == other);
}
