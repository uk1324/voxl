#include <../test/tests.hpp>
#include <Allocator.hpp>

// TODO: Add tests that check the return value of insert and remove.

using namespace Voxl;

static void markHashTable(HashTable* hashTable, Allocator& allocator)
{
	allocator.addHashTable(*hashTable);
}

#define INIT() \
	Allocator _a; \
	HashTable _m; \
	const auto _h = _a.registerMarkingFunction(&_m, markHashTable);

#define PUT(k, v) _m.set(_a.allocateString(k), v)
#define PUT_IF_NOT_SET(k, v) _m.insertIfNotSet(_a.allocateString(k), v)
#define GET(k) (_m.get(_a.allocateString(k)))
#define DEL(k) (_m.remove(_a.allocateString(k)))

static void insertTest()
{
	INIT();

	const auto isNewKey = PUT("abc", Value::integer(5));
	ASSERT_TRUE(isNewKey);
	const auto v = GET("abc");
	ASSERT_TRUE(v.has_value());
	ASSERT_EQ(GET("abc")->asInt(), 5);

	SUCCESS();
}

static void insertIfNotSetTest()
{
	INIT();

	PUT("abc", Value::integer(5));
	const bool inserted = PUT_IF_NOT_SET("abc", Value::integer(3));
	ASSERT_FALSE(inserted);
	const auto v = GET("abc");
	ASSERT_EQ(GET("abc")->asInt(), 5);

	SUCCESS();
}

static void insertAndDeleteTest()
{
	INIT();

	PUT("test", Value::integer(2));
	const auto v = GET("test");
	ASSERT_TRUE(v.has_value());
	ASSERT_EQ(GET("test")->asInt(), 2);
	const auto deleted = DEL("test");
	ASSERT_TRUE(deleted);
	ASSERT_FALSE(GET("test").has_value());

	SUCCESS();
}

static void deleteNonexistentKeyTest()
{
	INIT();

	const auto deleted = DEL("test");
	ASSERT_FALSE(deleted);

	SUCCESS();
}

static void rehashTest()
{
	INIT();

	static constexpr size_t SIZE = 10;
	for (char i = 0; i < SIZE; i++)
	{
		char key[] = { 'a' + i, '\0' };
		PUT(key, Value::integer(i));
	}

	for (char i = 0; i < SIZE; i += 2)
	{
		char key[] = { 'a' + i, '\0' };
		DEL(key);
	}

	for (char i = 1; i < SIZE; i += 2)
	{
		char key[] = { 'a' + i, '\0' };
		auto v = GET(key);
		ASSERT_TRUE(v.has_value());
		ASSERT_EQ(v->asInt(), i);
	}

	for (char i = 0; i < SIZE; i += 2)
	{
		char key[] = { 'a' + i, '\0' };
		auto v = GET(key);
		ASSERT_FALSE(v.has_value());
	}

	SUCCESS();
}

static void getNonexistentKeyTest()
{
	INIT();

	const auto v = GET("123");
	ASSERT_FALSE(v.has_value());

	SUCCESS();
}

static void iteratorTest()
{
	INIT();

	// TODO: Instead of this generate random pairs and insert them into a vector then insert them into the hash table
	const auto ITEMS_COUNT = 10;
	for (char i = 0; i < ITEMS_COUNT; i++)
	{
		char key[] = { 'a' + i, '\0' };
		PUT(key, Value::integer(i));
	}

#define CHECK(expectedKey, expectedValue) \
	else if (strcmp(key->chars, expectedKey) == 0) \
	{ \
		ASSERT_TRUE(value.isInt()); \
		ASSERT_EQ(value.as.intNumber, expectedValue); \
		count++; \
	}
	auto count = 0;
	for (const auto& [key, value] : _m)
	{
		if (false) {}
		CHECK("a", 0)
		CHECK("b", 1)
		CHECK("c", 2)
		CHECK("d", 3)
		CHECK("e", 4)
		CHECK("f", 5)
		CHECK("g", 6)
		CHECK("h", 7)
		CHECK("i", 8)
		CHECK("j", 9)
		else
			ASSERT_TRUE(false);
	}
#undef CHECK
	ASSERT_EQ(count, ITEMS_COUNT);

	SUCCESS();
}

// Can't allocate too much because memory the hashmap isn't registered as a GC root.

void hashTableTests()
{
	std::cout << "HashTable tests\n";
	insertTest();
	insertIfNotSetTest();
	insertAndDeleteTest();
	deleteNonexistentKeyTest();
	rehashTest();
	getNonexistentKeyTest();
	iteratorTest();
}