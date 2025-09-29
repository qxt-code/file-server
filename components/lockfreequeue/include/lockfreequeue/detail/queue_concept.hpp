#pragma once

#include <concepts>

namespace lf {

template <typename T>
concept HasCapacity = requires(T t) { t.capacity(); };


}; // namespace lf