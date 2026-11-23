#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace blitzdj::crypto
{
    std::array<std::uint8_t, 32> sha256(const std::string& data);

    std::vector<std::uint8_t> randomBytes(std::size_t count);

    std::string base64Url(const std::uint8_t* data, std::size_t size);
}
