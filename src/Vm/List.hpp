#pragma once

#include <Value.hpp>
#include <Allocator.hpp>

namespace Voxl
{

struct List : public ObjNativeInstance
{
	static constexpr int iterArgCount = 1;
	static LocalValue iter(Context& c);
	static constexpr int pushArgCount = 2;
	static LocalValue push(Context& c);
	static constexpr int getSizeArgCount = 1;
	static LocalValue get_size(Context& c);
	static constexpr int getIndexArgCount = 2;
	static LocalValue get_index(Context& c);
	static constexpr int setIndexArgCount = 3;
	static LocalValue set_index(Context& c);

	void push(const Value& value);

	static void init(List* list);
	static void free(List* list);
	static void mark(List* list, Allocator& allocator);

	size_t capacity;
	size_t size;
	Value* data;
};

struct ListIterator : public ObjNativeInstance
{
	static constexpr int initArgCount = 2;
	static LocalValue init(Context& c);
	static constexpr int nextArgCount = 1;
	static LocalValue next(Context& c);

	// TODO: Maybe rename initFunction to something else to prevent ambigiuties in function pointer overloads.
	static void construct(ListIterator* iterator);
	static void mark(ListIterator* iterator, Allocator& allocator);

	List* list;
	size_t index;
};

}