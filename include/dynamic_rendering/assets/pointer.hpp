#pragma once

#include <functional>
#include <memory>

namespace Assets {

template<typename T>
using Deleter = std::function<void(T*)>;

template<typename T>
using Pointer = std::unique_ptr<T, Deleter<T>>;

}