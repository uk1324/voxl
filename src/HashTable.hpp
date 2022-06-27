#pragma once

#include <Asserts.hpp>
#include <Optional.hpp>
#include <Value.hpp>

namespace Lang
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

public:
	HashTable();
	bool set(ObjString* key, Value value);
	bool remove(const ObjString* key);
	std::optional<Value&> get(const ObjString* key);
	Bucket& findBucket(const ObjString* key);

	static bool isBucketEmpty(const Bucket& bucket);
	void print();
	size_t capacity() const;
	Bucket* data();
	void clear();

	static constexpr size_t INITIAL_SIZE = 8;
	static constexpr float MAX_LOAD_FACTOR = 0.75f;

private:
	void setAllKeysToNull();
	static bool compareKeys(const ObjString* a, const ObjString* b);
	static size_t hashKey(const ObjString* key);
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