#pragma once

#include <Value.hpp>
#include <Allocator.hpp>

namespace Voxl
{

namespace String
{

static constexpr int lenArgCount = 1;
LocalValue len(Context& c);
static constexpr int hashArgCount = 1;
LocalValue hash(Context& c);

}

}