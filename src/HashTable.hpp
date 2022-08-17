#pragma once

#include <Asserts.hpp>
#include <Optional.hpp>
#include <Value.hpp>

namespace Voxl
{

struct ObjString;

class HashTable
{
public:
	struct Bucket
	{
		ObjString* key;
		Value value;
	};

	struct Iterator
	{
		Iterator(HashTable& hashTable, Bucket* bucket);

		Bucket* operator->();
		Bucket& operator*();
		Iterator& operator++();

		bool operator==(const Iterator& other) const;
		bool operator!=(const Iterator& other) const;

	private:
		HashTable& m_hashTable;
		Bucket* m_bucket;
	};

	struct ConstIterator
	{
		ConstIterator(const HashTable& hashTable, const Bucket* bucket);

		const Bucket* operator->() const;
		const Bucket& operator*() const;
		ConstIterator& operator++();

		bool operator==(const ConstIterator& other) const;
		bool operator!=(const ConstIterator& other) const;

	private:
		const HashTable& m_hashTable;
		const Bucket* m_bucket;
	};

public:
	HashTable();
	bool set(ObjString* key, const Value& value);
	bool insertIfNotSet(ObjString* key, const Value& value);
	bool remove(const ObjString* key);
	bool remove(std::string_view key);
	std::optional<Value&> get(const ObjString* key);
	std::optional<Value&> get(std::string_view key);
	
	void print();
	size_t capacity() const;
	Bucket* data();
	void clear();
	Iterator begin();
	Iterator end();
	ConstIterator cbegin() const;
	ConstIterator cend() const;

private:
	static constexpr size_t INITIAL_SIZE = 8;
	static constexpr float MAX_LOAD_FACTOR = 0.75f;

	Bucket& findBucket(const ObjString* key);
	Bucket& findBucket(std::string_view key);
	template<typename T>
	Bucket& findBucketImplementation(T key);
	template<typename T>
	std::optional<Value&> getImplementation(T key);
	void setAllKeysToNull();
	void resizeIfNeeded(size_t newSize);
	static bool compareKeys(const ObjString* a, const ObjString* b);
	static bool compareKeys(std::string_view a, const ObjString* b);
	static size_t hashKey(const ObjString* key);
	static size_t hashKey(std::string_view key);
	static void setKeyNull(ObjString*& key);
	static void setKeyTombstone(ObjString*& key);
	static bool isKeyNull(const ObjString* key);
	static bool isKeyTombstone(const ObjString* key);
	static bool isBucketEmpty(const Bucket& bucket);

private:
	Bucket* m_data;
	size_t m_capacity;
	size_t m_size;
};

template<typename T>
HashTable::Bucket& HashTable::findBucketImplementation(T key)
{
	// ANDing can be used to perform modulo, because the capacity is always a power of 2.
	auto index = hashKey(key) & (m_capacity - 1);

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
		else if (compareKeys(key, bucket.key))
		{
			return bucket;
		}

		// TODO: Find a better probing method
		// Robing hood probing and siwsstables implementation are apparently good.
		index = (index + 1) & (m_capacity - 1);
	}
}

template<typename T>
std::optional<Value&> HashTable::getImplementation(T key)
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

}