#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Render::Models::BmdModelFile
{
constexpr std::size_t MaxFileBytes = 64 * 1024 * 1024;
bool ValidateBody(std::span<const std::uint8_t> body);
// Returns only complete, renderer-compatible model data. Output is cleared on failure.
bool Decode(std::span<const std::uint8_t> file, std::vector<std::uint8_t>& body);
} // namespace Render::Models::BmdModelFile
