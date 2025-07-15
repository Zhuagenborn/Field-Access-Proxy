#pragma once

#include <bit>

constexpr std::endian GetOppositeEndian() noexcept {
    return std::endian::native == std::endian::little ? std::endian::big : std::endian::little;
}