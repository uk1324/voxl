#pragma once

#include <memory>

template<typename T>
using OwnPtr = std::unique_ptr<T>;

template<typename T>
using Rc = std::shared_ptr<T>;

template<typename T>
constexpr auto makeUnique = std::make_unique<T>;

template<typename T>
constexpr auto makeShared = std::make_shared<T>;