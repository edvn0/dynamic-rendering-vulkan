#pragma once

#include <functional>
#include <memory>

namespace Assets {

template<typename T>
using Pointer = std::unique_ptr<T, std::function<void(T*)>>;

}