#pragma once

#include <Context.hpp>

Voxl::LocalValue testModuleMain(Voxl::Context& c);

struct U8 : public Voxl::ObjNativeInstance
{
	static constexpr int initArgCount = 2;
	static Voxl::LocalValue init(Voxl::Context& c);
	static constexpr int getArgCount = 1;
	static Voxl::LocalValue get(Voxl::Context& c);
	static constexpr int setArgCount = 2;
	static Voxl::LocalValue set(Voxl::Context& c);

	static void mark(U8*, Voxl::Allocator&) {};

	uint8_t value;
};