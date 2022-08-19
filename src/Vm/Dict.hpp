#pragma once

#include <Value.hpp>
#include <Allocator.hpp>
#include <unordered_map>

namespace Voxl
{

class Dict : public ObjInstance
{
	static constexpr int getIndexArgCount = 2;
	static LocalValue get_index(Context& c);
	static constexpr int setIndexArgCount = 3;
	static LocalValue set_index(Context& c);

	static void init(Dict* self);
	static void free(Dict* self);
	static void mark(Dict* self, Allocator& allocator);

	//struct Key
	//{
	//	size_t hash;
	//	Value value;
	//};
	//std::unordered_map<Key, Value, > dict;
};

}