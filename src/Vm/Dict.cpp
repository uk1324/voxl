#include <Vm/Dict.hpp>
#include <Vm/Vm.hpp>
#include <Context.hpp>

using namespace Voxl;

LocalValue Dict::get_index(Context& c)
{
	auto self = c.args(0).asObj<Dict>();
	auto key = c.args(1);
	auto [_, bucket] = self->findBucket(c, key);
	if (bucket.has_value())
		return LocalValue(bucket->value, c);
	return LocalValue::null(c);
}

LocalValue Dict::set_index(Context& c)
{
	auto self = c.args(0).asObj<Dict>();
	auto key = c.args(1);
	auto value = c.args(2);

	auto [buckets, bucket] = self->findBucket(c, key);

	if ((self->size / self->bucketLists.capacity()) > MAX_LOAD_FACTOR)
	{
		// TODO: Rehash.
		/*std::swap()
		Buckets newBuckets;
		newBuckets.resize(self->bucketLists.capacity() * 2);*/

	}
	if (bucket.has_value())
	{
		bucket->value = value.value;
	}
	else
	{
		buckets.push_back({ key.value, value.value });
	}
	self->size++;
	return value;
}

LocalValue Dict::get_size(Context& c)
{
	auto self = c.args(0).asObj<Dict>();
	return LocalValue::intNum(static_cast<Int>(self->size), c);
}

void Dict::init(Dict* self)
{
	new (&self->bucketLists) Buckets();
	self->bucketLists.resize(8);
	self->size = 0;
}

void Dict::free(Dict* self)
{
	self->bucketLists.~Buckets();
}

void Dict::mark(Dict* self, Allocator& allocator)
{
	for (const auto& list : self->bucketLists)
	{
		for (const auto& [key, value] : list)
		{
			allocator.addValue(key);
			allocator.addValue(value);
		}
	}
}

std::pair<std::list<Dict::Bucket>&, std::optional<Dict::Bucket&>> Dict::findBucket(Context& c, LocalValue& key)
{
	auto hashValue = key.get("$hash")();
	if (hashValue.asInt() == false)
	{
		auto typeError = c.get("TypeError");
		throw NativeException(typeError(LocalValue("$hash() has to return an 'Int'", c)));
	}
	const auto hash = static_cast<size_t>(hashValue.asInt()) % bucketLists.capacity();
	auto& buckets = bucketLists[hash];
	for (auto& bucket : buckets)
	{
		if (key == LocalValue(bucket.key, c))
		{
			return { buckets, bucket };
		}
	}
	return { buckets, std::nullopt };
}
