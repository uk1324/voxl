#include <../test/tests.hpp>
#include <HashMap.hpp>

using namespace Lang;

struct ObjStringKeyTraits
{
	static bool compareKeys(const ObjString* a, const ObjString* b)
	{
		return (a->size == b->size) && (memcmp(a->chars, b->chars, a->size) == 0);
	}

	static size_t hashKey(const ObjString* key)
	{
		return std::hash<std::string_view>()(std::string_view(key->chars, key->size));
	}

	static void setKeyNull(ObjString*& key)
	{
		key = nullptr;
	}

	static void setKeyTombstone(ObjString*& key)
	{
		key = reinterpret_cast<ObjString*>(1);
	}

	static bool isKeyNull(const ObjString* key)
	{
		return key == nullptr;
	}

	static bool isKeyTombstone(const ObjString* key)
	{
		return key == reinterpret_cast<ObjString*>(1);
	}
};

#define INIT() \
	Allocator _a; \
	HashMap<ObjString*, int, ObjStringKeyTraits> _m;  \
	_m.init(_m, _a);

#define PUT(k, v) _m.insert(_a, _a.allocateString(k), v)
#define GET(k) (_m.get(_a.allocateString(k)))
#define DEL(k) (_m.remove(_a.allocateString(k)))

static void insertTest()
{
	INIT();

	PUT("abc", 5);
	const auto v = GET("abc");
	ASSERT_TRUE(v.has_value());
	ASSERT_EQ(**GET("abc"), 5);

	SUCCESS();
}

static void insertAndDeleteTest()
{
	INIT();

	PUT("test", 2);
	const auto v = GET("test");
	ASSERT_TRUE(v.has_value());
	ASSERT_EQ(**GET("test"), 2);
	DEL("test");
	ASSERT_FALSE(GET("test").has_value());

	SUCCESS();
}

static void rehashTest()
{
	INIT();

	static constexpr size_t SIZE = 10;
	for (char i = 0; i < SIZE; i++)
	{
		char key[] = { 'a' + i, '\0' };
		PUT(key, i);
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
		ASSERT_EQ(**v, i);
	}

	for (char i = 0; i < SIZE; i += 2)
	{
		char key[] = { 'a' + i, '\0' };
		auto v = GET(key);
		ASSERT_FALSE(v.has_value());
	}

	SUCCESS();
}

void getNonExistingKeyTest()
{
	INIT();

	const auto v = GET("123");
	ASSERT_FALSE(v.has_value());

	SUCCESS();
}

// Can't allocate too much because memory the hashmap isn't registered as a GC root.

void hashMapTests()
{
	std::cout << "HashMap tests\n";
	insertTest();
	insertAndDeleteTest();
	rehashTest();
	getNonExistingKeyTest();
}