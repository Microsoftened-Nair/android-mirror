#pragma once

#include <bit>
#include <cstdint>

namespace mirror::protocol {

[[nodiscard]] constexpr uint16_t to_big_endian(uint16_t val) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return val;
    } else {
#if __cpp_lib_byteswap >= 202110L
        return std::byteswap(val);
#else
        return __builtin_bswap16(val);
#endif
    }
}

[[nodiscard]] constexpr uint16_t from_big_endian(uint16_t val) noexcept {
    return to_big_endian(val);
}

[[nodiscard]] constexpr uint32_t to_big_endian(uint32_t val) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return val;
    } else {
#if __cpp_lib_byteswap >= 202110L
        return std::byteswap(val);
#else
        return __builtin_bswap32(val);
#endif
    }
}

[[nodiscard]] constexpr uint32_t from_big_endian(uint32_t val) noexcept {
    return to_big_endian(val);
}

[[nodiscard]] constexpr int32_t to_big_endian(int32_t val) noexcept {
    return static_cast<int32_t>(to_big_endian(static_cast<uint32_t>(val)));
}

[[nodiscard]] constexpr int32_t from_big_endian(int32_t val) noexcept {
    return to_big_endian(val);
}

[[nodiscard]] constexpr uint64_t to_big_endian(uint64_t val) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return val;
    } else {
#if __cpp_lib_byteswap >= 202110L
        return std::byteswap(val);
#else
        return __builtin_bswap64(val);
#endif
    }
}

[[nodiscard]] constexpr uint64_t from_big_endian(uint64_t val) noexcept {
    return to_big_endian(val);
}

} // namespace mirror::protocol
