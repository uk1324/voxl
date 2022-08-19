#pragma once

#include <Value.hpp>
#include <Allocator.hpp>


namespace Voxl
{

namespace Number
{

static constexpr int floorArgCount = 1;
LocalValue floor(Context& c);
static constexpr int ceilArgCount = 1;
LocalValue ceil(Context& c);
static constexpr int roundArgcount = 1;
LocalValue round(Context& c);

static constexpr int powArgCount = 2;
LocalValue pow(Context& c);
static constexpr int sqrtArgCount = 1;
LocalValue sqrt(Context& c);

static constexpr int isInfArgCount = 1;
LocalValue is_inf(Context& c);
static constexpr int isNanArgCount = 1;
LocalValue is_nan(Context& c);

static constexpr int sinArgCount = 1;
LocalValue sin(Context& c);
static constexpr int cosArgCount = 1;
LocalValue cos(Context& c);
static constexpr int tanArgCount = 1;
LocalValue tan(Context& c);

}

}