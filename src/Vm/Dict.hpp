#pragma once

#include <Value.hpp>
#include <Allocator.hpp>

namespace Voxl
{

struct Dict : public ObjNativeInstance
{
	static constexpr int getIndexArgCount = 2;
	static LocalValue get_index(Context& c);
	static constexpr int setIndexArgCount = 3;
	static LocalValue set_index(Context& c);
	static constexpr int getSizeArgCount = 0;
	static LocalValue get_size(Context& c);

	static void init(Dict* self);
	static void free(Dict* self);
	static void mark(Dict* self, Allocator& allocator);

	struct Bucket
	{
		Value key;
		Value value;
	};

	static constexpr float MAX_LOAD_FACTOR = 0.75f;

	std::pair<std::list<Bucket>&, std::optional<Bucket&>> findBucket(Context& c, LocalValue& key);
	using Buckets = std::vector<std::list<Bucket>>;
	Buckets bucketLists;
	size_t size;
};

}