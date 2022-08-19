#pragma once

#include <Value.hpp>
#include <Allocator.hpp>

namespace Voxl
{

namespace NameError
{
	static constexpr int initArgCount = 2;
	LocalValue init(Context& c);
	static constexpr int strArgCount = 1;
	LocalValue str(Context& c);
}

}