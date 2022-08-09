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
	bool remove(const ObjString* key);
	bool remove(std::string_view key);
	std::optional<Value&> get(const ObjString* key);
	std::optional<Value&> get(std::string_view key);
	Bucket& findBucket(const ObjString* key);
	Bucket& findBucket(std::string_view key);

	static bool isBucketEmpty(const Bucket& bucket);
	void print();
	size_t capacity() const;
	Bucket* data();
	void clear();
	Iterator begin();
	Iterator end();
	ConstIterator cbegin() const;
	ConstIterator cend() const;

	static constexpr size_t INITIAL_SIZE = 8;
	static constexpr float MAX_LOAD_FACTOR = 0.75f;

private:
	void setAllKeysToNull();
	static bool compareKeys(const ObjString* a, const ObjString* b);
	static bool compareKeys(std::string_view a, const ObjString* b);
	static size_t hashKey(const ObjString* key);
	static size_t hashKey(std::string_view key);
	static void setKeyNull(ObjString*& key);
	static void setKeyTombstone(ObjString*& key);
	static bool isKeyNull(const ObjString* key);
	static bool isKeyTombstone(const ObjString* key);

private:
	Bucket* m_data;
	size_t m_capacity;
	size_t m_size;
};

}